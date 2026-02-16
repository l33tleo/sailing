#include "SailingGameMode.h"
#include "SailboatPawn.h"
#include "SailingPlayerController.h"
#include "SailingHUD.h"
#include "SailingGameInstance.h"
#include "WindActor.h"
#include "ChunkManager.h"
#include "FjordMapManager.h"
#include "FjordCoastlineActor.h"
#include "FjordMapData.h"
#include "OceanPlaneActor.h"
#include "SaveGameSailing.h"
#include "Kismet/GameplayStatics.h"
#include "TimerManager.h"

ASailingGameMode::ASailingGameMode()
{
	DefaultPawnClass = ASailboatPawn::StaticClass();
	PlayerControllerClass = ASailingPlayerController::StaticClass();
	HUDClass = ASailingHUD::StaticClass();
}

void ASailingGameMode::BeginPlay()
{
	Super::BeginPlay();

	USailingGameInstance* GI = Cast<USailingGameInstance>(GetGameInstance());
	if (GI && GI->bRequestNewGame)
	{
		GI->bRequestNewGame = false;
		SaveGame = Cast<USaveGameSailing>(
			UGameplayStatics::CreateSaveGameObject(USaveGameSailing::StaticClass()));
		UE_LOG(LogTemp, Log, TEXT("SailingGameMode: Nytt spill"));
	}
	else
	{
		LoadOrCreateSaveGame();
	}

	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

	// Spawn wind actor
	SpawnedWind = GetWorld()->SpawnActor<AWindActor>(
		AWindActor::StaticClass(), FVector::ZeroVector, FRotator::ZeroRotator, Params);

	if (bUseFjordMap)
	{
		// Fjord mode: spawn fjord map manager and coastline, no chunk manager
		AFjordMapManager* FM = GetWorld()->SpawnActor<AFjordMapManager>(
			AFjordMapManager::StaticClass(), FVector::ZeroVector, FRotator::ZeroRotator, Params);
		SpawnedFjordMapManager = FM;
		if (FM)
		{
			FM->SetSaveGame(SaveGame);
		}

		AFjordCoastlineActor* Coastline = GetWorld()->SpawnActor<AFjordCoastlineActor>(
			AFjordCoastlineActor::StaticClass(), FVector::ZeroVector, FRotator::ZeroRotator, Params);
		SpawnedFjordCoastline = Coastline;
		if (Coastline && FM && FM->FjordMapData)
		{
			Coastline->FjordMapData = FM->FjordMapData;
		}
	}
	else
	{
		// Procedural mode: spawn chunk manager for islands
		SpawnedChunkManager = GetWorld()->SpawnActor<AChunkManager>(
			AChunkManager::StaticClass(), FVector::ZeroVector, FRotator::ZeroRotator, Params);
		if (AChunkManager* CM = Cast<AChunkManager>(SpawnedChunkManager))
		{
			CM->SetSaveGame(SaveGame);
		}
	}

	// Spawn ocean plane
	SpawnedOcean = GetWorld()->SpawnActor<AOceanPlaneActor>(
		AOceanPlaneActor::StaticClass(), FVector::ZeroVector, FRotator::ZeroRotator, Params);

	// Teleport pawn: saved position (Fortsett) or fjord start (new game in fjord mode)
	const bool bUseSavedPosition = SaveGame && SaveGame->LastPlayerLocation != FVector::ZeroVector;
	const bool bUseFjordStart = bUseFjordMap && !bUseSavedPosition;

	if (bUseSavedPosition || bUseFjordStart)
	{
		FVector TargetLoc = bUseSavedPosition ? SaveGame->LastPlayerLocation : FjordStartPosition;
		GetWorld()->GetTimerManager().SetTimerForNextTick([this, TargetLoc, bUseSavedPosition]()
		{
			if (APlayerController* PC = GetWorld()->GetFirstPlayerController())
			{
				if (APawn* Pawn = PC->GetPawn())
				{
					Pawn->SetActorLocation(TargetLoc);
					UE_LOG(LogTemp, Log, TEXT("SailingGameMode: Spiller flyttet til %s"),
						bUseSavedPosition ? TEXT("lagret posisjon") : TEXT("fjord start"));
				}
			}
		});
	}

	UE_LOG(LogTemp, Log, TEXT("SailingGameMode: Spill lastet. Øyer oppdaget: %d"),
		SaveGame ? SaveGame->TotalIslandsDiscovered : 0);
}

void ASailingGameMode::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	SaveGame_();
	Super::EndPlay(EndPlayReason);
}

void ASailingGameMode::LoadOrCreateSaveGame()
{
	if (UGameplayStatics::DoesSaveGameExist(USaveGameSailing::SaveSlotName, 0))
	{
		SaveGame = Cast<USaveGameSailing>(
			UGameplayStatics::LoadGameFromSlot(USaveGameSailing::SaveSlotName, 0));
		UE_LOG(LogTemp, Log, TEXT("Lastet eksisterende lagringsfil med %d øyer"),
			SaveGame ? SaveGame->TotalIslandsDiscovered : 0);
	}

	if (!SaveGame)
	{
		SaveGame = Cast<USaveGameSailing>(
			UGameplayStatics::CreateSaveGameObject(USaveGameSailing::StaticClass()));
		UE_LOG(LogTemp, Log, TEXT("Opprettet ny lagringsfil"));
	}
}

AChunkManager* ASailingGameMode::GetChunkManager() const
{
	return Cast<AChunkManager>(SpawnedChunkManager);
}

void ASailingGameMode::SaveGame_()
{
	if (SaveGame)
	{
		// Update player location
		if (APlayerController* PC = GetWorld()->GetFirstPlayerController())
		{
			if (APawn* Pawn = PC->GetPawn())
			{
				SaveGame->LastPlayerLocation = Pawn->GetActorLocation();
			}
		}

		UGameplayStatics::SaveGameToSlot(SaveGame, USaveGameSailing::SaveSlotName, 0);
		UE_LOG(LogTemp, Log, TEXT("Spill lagret"));
	}
}
