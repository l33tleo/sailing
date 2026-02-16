#include "SailingGameMode.h"
#include "SailboatPawn.h"
#include "SailingPlayerController.h"
#include "SailingHUD.h"
#include "SailingGameInstance.h"
#include "WindActor.h"
#include "ChunkManager.h"
#include "OceanPlaneActor.h"
#include "MissionObjectiveActor.h"
#include "PortMarkerActor.h"
#include "Data/PortDataAsset.h"
#include "Data/SailingMissionDataAsset.h"
#include "SaveGameSailing.h"
#include "Systems/SailingCoreSubsystems.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "Kismet/GameplayStatics.h"
#include "Modules/ModuleManager.h"
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

	const FVector DeliveryObjectiveLocation(6500.0f, -2500.0f, 120.0f);

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
			EconomySubsystem->SetBoatConditionPercent(SaveGame ? SaveGame->BoatConditionPercent : 100);
			EconomySubsystem->SetUnlockedUpgrades(SaveGame ? SaveGame->UnlockedUpgradeIds : TArray<FName>());
		}

		if (UMissionSubsystem* MissionSubsystem = GI->GetSubsystem<UMissionSubsystem>())
		{
			USailingMissionDataAsset* StarterDiscoveryMission = NewObject<USailingMissionDataAsset>(this);
			StarterDiscoveryMission->MissionId = TEXT("OppdagForsteOy");
			StarterDiscoveryMission->DisplayName = FText::FromString(TEXT("Oppdag første øy"));
			StarterDiscoveryMission->Description = FText::FromString(TEXT("Seil ut og oppdag din første øy."));
			StarterDiscoveryMission->MissionType = ESailingMissionType::NavigationChallenge;
			StarterDiscoveryMission->RewardCredits = 250;
			StarterDiscoveryMission->bRepeatable = false;
			StarterDiscoveryMission->NextMissionId = TEXT("LeveringTilBoye");
			MissionSubsystem->RegisterMissionAsset(StarterDiscoveryMission);

			USailingMissionDataAsset* StarterDeliveryMission = NewObject<USailingMissionDataAsset>(this);
			StarterDeliveryMission->MissionId = TEXT("LeveringTilBoye");
			StarterDeliveryMission->DisplayName = FText::FromString(TEXT("Levering til bøye"));
			StarterDeliveryMission->Description = FText::FromString(TEXT("Seil til leveringsbøyen og fullfør leveransen."));
			StarterDeliveryMission->MissionType = ESailingMissionType::Delivery;
			StarterDeliveryMission->RewardCredits = 400;
			StarterDeliveryMission->StartWorldLocation = FVector::ZeroVector;
			StarterDeliveryMission->EndWorldLocation = DeliveryObjectiveLocation;
			StarterDeliveryMission->bRequireLocationMatch = true;
			StarterDeliveryMission->CompletionRadius = 1200.0f;
			StarterDeliveryMission->bRepeatable = true;
			MissionSubsystem->RegisterMissionAsset(StarterDeliveryMission);

			MissionSubsystem->SetCompletedMissionIds(SaveGame ? SaveGame->CompletedMissionIds : TArray<FName>());
			MissionSubsystem->SetActiveMissionId(SaveGame ? SaveGame->ActiveMissionId : NAME_None);
			if (MissionSubsystem->GetActiveMissionId().IsNone() && SaveGame)
			{
				if (SaveGame->TotalIslandsDiscovered == 0)
				{
					MissionSubsystem->ActivateMissionAsset(StarterDiscoveryMission);
				}
				else
				{
					MissionSubsystem->ActivateMissionAsset(StarterDeliveryMission);
				}
			}

			if (MissionSubsystem->GetActiveMissionId().IsNone())
			{
				MissionSubsystem->ActivateFallbackMission();
			}

			if (SaveGame)
			{
				SaveGame->ActiveMissionId = MissionSubsystem->GetActiveMissionId();
				SaveGame->CompletedMissionIds = MissionSubsystem->GetCompletedMissionIds();
			}

			UE_LOG(LogTemp, Log, TEXT("SailingGameMode: Aktivt oppdrag '%s'."), *MissionSubsystem->GetActiveMissionId().ToString());
		}

		if (UTelemetrySubsystem* TelemetrySubsystem = GI->GetSubsystem<UTelemetrySubsystem>())
		{
			TelemetrySubsystem->SetAllCounters(SaveGame ? SaveGame->TelemetryCounters : TMap<FName, int32>());
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

	// Spawn concrete delivery mission objective marker
	SpawnedMissionObjective = GetWorld()->SpawnActor<AMissionObjectiveActor>(
		AMissionObjectiveActor::StaticClass(), DeliveryObjectiveLocation, FRotator::ZeroRotator, Params);
	if (SpawnedMissionObjective)
	{
		SpawnedMissionObjective->SetTriggerType(ESailingMissionType::Delivery);
	}

	// Spawn simple harbor markers and register with world subsystem
	if (GI)
	{
		if (UWorldStreamingSubsystem* WorldStreamingSubsystem = GI->GetSubsystem<UWorldStreamingSubsystem>())
		{
			WorldStreamingSubsystem->ClearPortPoints();
			SpawnedPortMarkers.Empty();

			TArray<UPortDataAsset*> PortDefinitions;
			{
				FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry"));
				FARFilter Filter;
				Filter.PackagePaths.Add(PortAssetScanPath.IsNone() ? FName(TEXT("/Game")) : PortAssetScanPath);
				Filter.ClassPaths.Add(UPortDataAsset::StaticClass()->GetClassPathName());
				Filter.bRecursivePaths = true;

				TArray<FAssetData> PortAssets;
				AssetRegistryModule.Get().GetAssets(Filter, PortAssets);
				for (const FAssetData& AssetData : PortAssets)
				{
					if (UPortDataAsset* PortData = Cast<UPortDataAsset>(AssetData.GetAsset()))
					{
						if (!PortData->PortId.IsNone())
						{
							PortDefinitions.Add(PortData);
						}
					}
				}
			}

			// Fallback defaults if no data assets exist yet
			if (PortDefinitions.Num() == 0)
			{
				UPortDataAsset* PortNord = NewObject<UPortDataAsset>(this);
				PortNord->PortId = TEXT("HavnNord");
				PortNord->DisplayName = FText::FromString(TEXT("Nordhavn"));
				PortNord->WorldLocation = FVector(2000.0f, -6000.0f, 100.0f);
				PortDefinitions.Add(PortNord);

				UPortDataAsset* PortVest = NewObject<UPortDataAsset>(this);
				PortVest->PortId = TEXT("HavnVest");
				PortVest->DisplayName = FText::FromString(TEXT("Vesthavn"));
				PortVest->WorldLocation = FVector(-7000.0f, 1500.0f, 100.0f);
				PortDefinitions.Add(PortVest);

				UPortDataAsset* PortSor = NewObject<UPortDataAsset>(this);
				PortSor->PortId = TEXT("HavnSor");
				PortSor->DisplayName = FText::FromString(TEXT("Sorhavn"));
				PortSor->WorldLocation = FVector(8500.0f, 4500.0f, 100.0f);
				PortDefinitions.Add(PortSor);
			}

			for (const UPortDataAsset* PortData : PortDefinitions)
			{
				APortMarkerActor* PortMarker = GetWorld()->SpawnActor<APortMarkerActor>(
					APortMarkerActor::StaticClass(), PortData->WorldLocation, FRotator::ZeroRotator, Params);
				if (!PortMarker)
				{
					continue;
				}

				PortMarker->PortId = PortData->PortId;
				PortMarker->PortDisplayName = PortData->DisplayName;
				PortMarker->DockBonusCredits = PortData->DockBonusCredits;
				PortMarker->bGrantOneTimeDockBonus = PortData->bGrantOneTimeDockBonus;
				PortMarker->bAutoRepairAtPort = PortData->bAutoRepairAtPort;
				PortMarker->RepairCostPerPercentPoint = PortData->RepairCostPerPercentPoint;

				SpawnedPortMarkers.Add(PortMarker);
				WorldStreamingSubsystem->RegisterPortPoint(PortData->PortId, PortData->WorldLocation);
			}

			UE_LOG(LogTemp, Log, TEXT("SailingGameMode: Spawned %d harbor markers."), SpawnedPortMarkers.Num());
		}
	}

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
				SaveGame->BoatConditionPercent = EconomySubsystem->GetBoatConditionPercent();
				SaveGame->UnlockedUpgradeIds = EconomySubsystem->GetUnlockedUpgradeIds();
			}

			if (UMissionSubsystem* MissionSubsystem = GI->GetSubsystem<UMissionSubsystem>())
			{
				SaveGame->ActiveMissionId = MissionSubsystem->GetActiveMissionId();
				SaveGame->CompletedMissionIds = MissionSubsystem->GetCompletedMissionIds();
			}

			if (UTelemetrySubsystem* TelemetrySubsystem = GI->GetSubsystem<UTelemetrySubsystem>())
			{
				SaveGame->TelemetryCounters = TelemetrySubsystem->GetAllCounters();
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
