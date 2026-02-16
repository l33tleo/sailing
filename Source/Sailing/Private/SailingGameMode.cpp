#include "SailingGameMode.h"
#include "SailboatPawn.h"
#include "SailingPlayerController.h"
#include "SailingHUD.h"
#include "SailingGameInstance.h"
#include "WindActor.h"
#include "ChunkManager.h"
#include "OceanPlaneActor.h"
#include "Data/SailingMissionDataAsset.h"
#include "SaveGameSailing.h"
#include "Systems/SailingCoreSubsystems.h"
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

	if (SaveGame)
	{
		SaveGame->EnsureCompatibility();
	}

	if (GI)
	{
		if (USaveSubsystem* SaveSubsystem = GI->GetSubsystem<USaveSubsystem>())
		{
			SaveSubsystem->MigrateSaveGame(SaveGame);
		}

		if (UEconomySubsystem* EconomySubsystem = GI->GetSubsystem<UEconomySubsystem>())
		{
			EconomySubsystem->SetCredits(SaveGame ? SaveGame->PlayerCredits : 0);
			EconomySubsystem->SetUnlockedUpgrades(SaveGame ? SaveGame->UnlockedUpgradeIds : TArray<FName>());
		}

		if (UMissionSubsystem* MissionSubsystem = GI->GetSubsystem<UMissionSubsystem>())
		{
			MissionSubsystem->SetActiveMissionId(SaveGame ? SaveGame->ActiveMissionId : NAME_None);

			if (MissionSubsystem->GetActiveMissionId().IsNone() &&
				SaveGame && SaveGame->TotalIslandsDiscovered == 0)
			{
				USailingMissionDataAsset* StarterMission = NewObject<USailingMissionDataAsset>(this);
				StarterMission->MissionId = TEXT("OppdagForsteOy");
				StarterMission->DisplayName = FText::FromString(TEXT("Oppdag første øy"));
				StarterMission->Description = FText::FromString(TEXT("Seil ut og oppdag din første øy."));
				StarterMission->MissionType = ESailingMissionType::NavigationChallenge;
				StarterMission->RewardCredits = 250;
				StarterMission->bRepeatable = false;
				MissionSubsystem->ActivateMissionAsset(StarterMission);
				SaveGame->ActiveMissionId = StarterMission->MissionId;
				UE_LOG(LogTemp, Log, TEXT("SailingGameMode: Aktivert startoppdrag '%s'."), *StarterMission->MissionId.ToString());
			}
		}
	}

	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

	// Spawn wind actor
	SpawnedWind = GetWorld()->SpawnActor<AWindActor>(
		AWindActor::StaticClass(), FVector::ZeroVector, FRotator::ZeroRotator, Params);

	// Spawn chunk manager for procedural islands
	SpawnedChunkManager = GetWorld()->SpawnActor<AChunkManager>(
		AChunkManager::StaticClass(), FVector::ZeroVector, FRotator::ZeroRotator, Params);

	// Pass save game to chunk manager
	if (AChunkManager* CM = Cast<AChunkManager>(SpawnedChunkManager))
	{
		CM->SetSaveGame(SaveGame);
	}

	// Spawn ocean plane
	SpawnedOcean = GetWorld()->SpawnActor<AOceanPlaneActor>(
		AOceanPlaneActor::StaticClass(), FVector::ZeroVector, FRotator::ZeroRotator, Params);

	// Teleporter pawn til siste posisjon ved Fortsett (etter at pawn er spawnet)
	if (SaveGame && SaveGame->LastPlayerLocation != FVector::ZeroVector)
	{
		GetWorld()->GetTimerManager().SetTimerForNextTick([this]()
		{
			if (APlayerController* PC = GetWorld()->GetFirstPlayerController())
			{
				if (APawn* Pawn = PC->GetPawn())
				{
					Pawn->SetActorLocation(SaveGame->LastPlayerLocation);
					UE_LOG(LogTemp, Log, TEXT("SailingGameMode: Spiller flyttet til lagret posisjon"));
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
		USailingGameInstance* GI = Cast<USailingGameInstance>(GetGameInstance());
		if (GI)
		{
			if (UEconomySubsystem* EconomySubsystem = GI->GetSubsystem<UEconomySubsystem>())
			{
				SaveGame->PlayerCredits = EconomySubsystem->GetCredits();
				SaveGame->UnlockedUpgradeIds = EconomySubsystem->GetUnlockedUpgradeIds();
			}

			if (UMissionSubsystem* MissionSubsystem = GI->GetSubsystem<UMissionSubsystem>())
			{
				SaveGame->ActiveMissionId = MissionSubsystem->GetActiveMissionId();
			}
		}

		// Update player location
		if (APlayerController* PC = GetWorld()->GetFirstPlayerController())
		{
			if (APawn* Pawn = PC->GetPawn())
			{
				SaveGame->LastPlayerLocation = Pawn->GetActorLocation();
			}
		}

		SaveGame->EnsureCompatibility();

		UGameplayStatics::SaveGameToSlot(SaveGame, USaveGameSailing::SaveSlotName, 0);
		UE_LOG(LogTemp, Log, TEXT("Spill lagret"));
	}
}
