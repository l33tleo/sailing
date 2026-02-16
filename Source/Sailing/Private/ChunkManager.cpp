#include "ChunkManager.h"
#include "IslandActor.h"
#include "SaveGameSailing.h"
#include "Data/SailingMissionDataAsset.h"
#include "Systems/SailingCoreSubsystems.h"
#include "Kismet/GameplayStatics.h"

AChunkManager::AChunkManager()
{
	PrimaryActorTick.bCanEverTick = true;
	IslandClass = AIslandActor::StaticClass();
}

void AChunkManager::BeginPlay()
{
	Super::BeginPlay();

	if (UGameInstance* GI = GetGameInstance())
	{
		if (UWorldStreamingSubsystem* WorldStreamingSubsystem = GI->GetSubsystem<UWorldStreamingSubsystem>())
		{
			LoadRadius = FMath::Max(1, WorldStreamingSubsystem->GetTargetActiveChunkRadius());
			UnloadRadius = FMath::Max(LoadRadius + 1, UnloadRadius);
			UE_LOG(LogTemp, Log, TEXT("ChunkManager: Synced radii from subsystem. Load=%d Unload=%d"), LoadRadius, UnloadRadius);
		}
	}
}

void AChunkManager::SetSaveGame(USaveGameSailing* InSaveGame)
{
	SaveGame = InSaveGame;
}

FIntPoint AChunkManager::WorldToChunk(const FVector& Location) const
{
	return FIntPoint(
		FMath::FloorToInt(Location.X / ChunkSize),
		FMath::FloorToInt(Location.Y / ChunkSize));
}

void AChunkManager::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	TimeSinceLastUpdate += DeltaTime;
	if (TimeSinceLastUpdate < UpdateInterval) return;
	TimeSinceLastUpdate = 0.0f;

	APlayerController* PC = GetWorld()->GetFirstPlayerController();
	if (!PC) return;
	APawn* PlayerPawn = PC->GetPawn();
	if (!PlayerPawn) return;

	FIntPoint PlayerChunk = WorldToChunk(PlayerPawn->GetActorLocation());

	// Load nearby chunks
	for (int32 X = PlayerChunk.X - LoadRadius; X <= PlayerChunk.X + LoadRadius; ++X)
	{
		for (int32 Y = PlayerChunk.Y - LoadRadius; Y <= PlayerChunk.Y + LoadRadius; ++Y)
		{
			FIntPoint Coord(X, Y);
			if (!SpawnedChunks.Contains(Coord))
			{
				LoadChunk(Coord);
			}
		}
	}

	UnloadDistantChunks(PlayerChunk);
}

void AChunkManager::LoadChunk(FIntPoint ChunkCoord)
{
	TArray<TWeakObjectPtr<AIslandActor>> Islands;

	// Deterministic seed from chunk coordinates
	int32 Seed = HashCombine(GetTypeHash(ChunkCoord.X), GetTypeHash(ChunkCoord.Y));
	FRandomStream RNG(Seed);

	int32 NumIslands = RNG.RandRange(0, MaxIslandsPerChunk);
	FVector ChunkOrigin(ChunkCoord.X * ChunkSize, ChunkCoord.Y * ChunkSize, 0.0f);

	for (int32 i = 0; i < NumIslands; ++i)
	{
		float OffX = RNG.FRandRange(ChunkSize * 0.1f, ChunkSize * 0.9f);
		float OffY = RNG.FRandRange(ChunkSize * 0.1f, ChunkSize * 0.9f);
		FVector SpawnLoc = ChunkOrigin + FVector(OffX, OffY, 0.0f);

		float Scale = RNG.FRandRange(0.5f, 3.0f);

		// Check if already discovered in save game
		bool bWasDiscovered = false;
		if (SaveGame)
		{
			bWasDiscovered = SaveGame->IsIslandDiscovered(ChunkCoord, i);
		}

		FActorSpawnParameters Params;
		Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		AIslandActor* Island = GetWorld()->SpawnActor<AIslandActor>(
			IslandClass, SpawnLoc, FRotator::ZeroRotator, Params);
		if (Island)
		{
			Island->SetActorScale3D(FVector(Scale, Scale, Scale * 0.5f));

			// Initialize with chunk data and discovery state
			Island->InitializeIsland(ChunkCoord, i, bWasDiscovered);

			// Bind to discovery event
			Island->OnDiscovered.AddDynamic(this, &AChunkManager::OnIslandDiscovered);

			Islands.Add(Island);
		}
	}

	SpawnedChunks.Add(ChunkCoord, Islands);
}

void AChunkManager::OnIslandDiscovered(AIslandActor* Island, const FString& IslandName)
{
	if (!SaveGame || !Island)
	{
		return;
	}

	// Create island data and save
	FIslandData Data;
	Data.ChunkCoord = Island->ChunkCoord;
	Data.IslandIndex = Island->IslandIndex;
	Data.IslandName = Island->IslandName;
	Data.WorldLocation = Island->GetActorLocation();
	Data.bDiscovered = true;
	Data.DiscoveredTime = FDateTime::Now();

	SaveGame->MarkIslandDiscovered(Data);

	int32 CreditsGranted = 0;
	if (UGameInstance* GI = GetGameInstance())
	{
		if (UEconomySubsystem* EconomySubsystem = GI->GetSubsystem<UEconomySubsystem>())
		{
			if (DiscoveryCreditReward > 0)
			{
				EconomySubsystem->AddCredits(DiscoveryCreditReward);
				CreditsGranted += DiscoveryCreditReward;
			}

			if (UMissionSubsystem* MissionSubsystem = GI->GetSubsystem<UMissionSubsystem>())
			{
				const int32 MissionReward = MissionSubsystem->CompleteActiveMissionByTrigger(ESailingMissionType::NavigationChallenge);
				if (MissionReward > 0)
				{
					EconomySubsystem->AddCredits(MissionReward);
					CreditsGranted += MissionReward;
				}
			}
		}

		if (UTelemetrySubsystem* TelemetrySubsystem = GI->GetSubsystem<UTelemetrySubsystem>())
		{
			TelemetrySubsystem->RecordCounterEvent(TEXT("IslandDiscovered"), 1);
			if (CreditsGranted > 0)
			{
				TelemetrySubsystem->RecordCounterEvent(TEXT("CreditsGranted"), CreditsGranted);
			}
		}
	}

	// Auto-save
	UGameplayStatics::SaveGameToSlot(SaveGame, USaveGameSailing::SaveSlotName, 0);

	UE_LOG(LogTemp, Log, TEXT("Lagret oppdagelse av %s. Totalt oppdaget: %d. Credits +%d"),
		*IslandName, SaveGame->TotalIslandsDiscovered, CreditsGranted);
}

void AChunkManager::UnloadDistantChunks(FIntPoint PlayerChunk)
{
	TArray<FIntPoint> ToRemove;
	for (auto& Pair : SpawnedChunks)
	{
		int32 Dist = FMath::Max(
			FMath::Abs(Pair.Key.X - PlayerChunk.X),
			FMath::Abs(Pair.Key.Y - PlayerChunk.Y));
		if (Dist > UnloadRadius)
		{
			for (auto& WeakIsland : Pair.Value)
			{
				if (WeakIsland.IsValid())
				{
					// Unbind before destroying
					WeakIsland->OnDiscovered.RemoveDynamic(this, &AChunkManager::OnIslandDiscovered);
					WeakIsland->Destroy();
				}
			}
			ToRemove.Add(Pair.Key);
		}
	}

	for (const FIntPoint& Key : ToRemove)
	{
		SpawnedChunks.Remove(Key);
	}
}
