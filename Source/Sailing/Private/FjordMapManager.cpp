#include "FjordMapManager.h"
#include "FjordMapData.h"
#include "IslandActor.h"
#include "SailboatPawn.h"
#include "SaveGameSailing.h"
#include "Kismet/GameplayStatics.h"

AFjordMapManager::AFjordMapManager()
{
	PrimaryActorTick.bCanEverTick = false;
	IslandClass = AIslandActor::StaticClass();
}

void AFjordMapManager::SetSaveGame(USaveGameSailing* InSaveGame)
{
	SaveGame = InSaveGame;
}

void AFjordMapManager::BeginPlay()
{
	Super::BeginPlay();

	UFjordMapData* Data = FjordMapData;
	if (!Data && !FjordMapDataPath.IsNull())
	{
		Data = Cast<UFjordMapData>(FjordMapDataPath.TryLoad());
	}
	if (!Data)
	{
		// Fallback: built-in Oslofjord data (23 islands from OSM/Overpass, positions in m from Oslo)
		UFjordMapData* TestData = NewObject<UFjordMapData>(this);
		TestData->CoastlinePoints = {
			FVector2D(-14755.2f, -28381.5f),
			FVector2D(1952.2f, -28381.5f),
			FVector2D(1952.2f, 304.1f),
			FVector2D(-14755.2f, 304.1f)
		};
		TestData->Islands = {
			{ TEXT("Bergholmen"),   FVector2D(-8925.7f, -26381.5f), 2.0f },
			{ TEXT("Håøya"),   FVector2D(-8942.7f, -26022.9f), 2.0f },
			{ TEXT("Torvøya"),   FVector2D(-12755.2f, -23933.2f), 2.0f },
			{ TEXT("Gråøya"),   FVector2D(-12332.9f, -22250.0f), 2.0f },
			{ TEXT("Aspond"),   FVector2D(-9967.9f, -20246.8f), 2.0f },
			{ TEXT("Lågøya"),   FVector2D(-10162.7f, -19401.1f), 2.0f },
			{ TEXT("Ildjernet"),   FVector2D(-6242.5f, -7076.1f), 2.0f },
			{ TEXT("Gåsøya"),   FVector2D(-9608.2f, -6823.8f), 1.5f },
			{ TEXT("Høyerholmen"),   FVector2D(-10849.8f, -6683.9f), 2.0f },
			{ TEXT("Langåra"),   FVector2D(-11700.9f, -6521.4f), 2.0f },
			{ TEXT("Hareholmen"),   FVector2D(-10865.3f, -6142.7f), 2.0f },
			{ TEXT("Brønnøya"),   FVector2D(-11710.2f, -5634.2f), 2.0f },
			{ TEXT("Malmøya"),   FVector2D(-47.8f, -5035.0f), 2.0f },
			{ TEXT("Ostøya"),   FVector2D(-9149.1f, -4267.3f), 2.0f },
			{ TEXT("Grimsøya"),   FVector2D(-9106.3f, -4227.1f), 2.0f },
			{ TEXT("Langøyene"),   FVector2D(-1509.7f, -4066.5f), 2.0f },
			{ TEXT("Nesøya"),   FVector2D(-11830.6f, -3815.3f), 2.0f },
			{ TEXT("Kjeholmen"),   FVector2D(-9126.1f, -3608.3f), 2.0f },
			{ TEXT("Gressholmen"),   FVector2D(-1749.1f, -3064.9f), 2.5f },
			{ TEXT("Kalvøya"),   FVector2D(-11408.1f, -2647.2f), 2.0f },
			{ TEXT("Bleikøya"),   FVector2D(-949.7f, -2468.2f), 2.0f },
			{ TEXT("Lindøya"),   FVector2D(-2146.9f, -2218.3f), 2.5f },
			{ TEXT("Hovedøya"),   FVector2D(-1396.0f, -1695.9f), 3.0f }
		};
		FjordMapData = TestData;
		Data = TestData;
		UE_LOG(LogTemp, Log, TEXT("FjordMapManager: Using built-in Oslofjord data (%d islands)."), TestData->Islands.Num());
	}

	const TArray<FFjordIslandDef>& Islands = Data->Islands;
	for (int32 i = 0; i < Islands.Num(); ++i)
	{
		const FFjordIslandDef& Def = Islands[i];
		FVector SpawnLoc(Def.Position.X * DistanceScale, Def.Position.Y * DistanceScale, WaterZ);

		FActorSpawnParameters Params;
		Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

		AIslandActor* Island = GetWorld()->SpawnActor<AIslandActor>(
			IslandClass, SpawnLoc, FRotator::ZeroRotator, Params);
		if (Island)
		{
			Island->SetActorScale3D(FVector(Def.Scale, Def.Scale, Def.Scale * 0.5f));

			bool bWasDiscovered = SaveGame ? SaveGame->IsFjordIslandDiscovered(Def.Name) : false;
			Island->InitializeFjordIsland(Def.Name, i, bWasDiscovered);

			Island->OnDiscovered.AddDynamic(this, &AFjordMapManager::OnIslandDiscovered);
		}
	}

	UE_LOG(LogTemp, Log, TEXT("FjordMapManager: Spawned %d fjord islands."), Islands.Num());
}

void AFjordMapManager::OnIslandDiscovered(AIslandActor* Island, const FString& IslandName)
{
	if (!SaveGame || !Island)
	{
		return;
	}

	FIslandData Data;
	Data.FjordIslandId = IslandName;
	Data.IslandName = IslandName;
	Data.ChunkCoord = Island->ChunkCoord;
	Data.IslandIndex = Island->IslandIndex;
	Data.WorldLocation = Island->GetActorLocation();
	Data.bDiscovered = true;
	Data.DiscoveredTime = FDateTime::Now();

	SaveGame->MarkFjordIslandDiscovered(Data);

	UGameplayStatics::SaveGameToSlot(SaveGame, USaveGameSailing::SaveSlotName, 0);

	UE_LOG(LogTemp, Log, TEXT("Fjord: Lagret oppdagelse av %s. Totalt oppdaget: %d"),
		*IslandName, SaveGame->TotalIslandsDiscovered);
}
