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
#include "Data/BoatUpgradeDataAsset.h"
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
			EconomySubsystem->SetUpgradeAssetPath(UpgradeAssetScanPath);

			auto EnsureUpgrade = [this, EconomySubsystem](
				FName UpgradeId,
				int32 CreditCost,
				float MaxSpeedMultiplier,
				float DragMultiplier,
				float TurnRateMultiplier,
				const TCHAR* DisplayNameText,
				const TCHAR* DescriptionText)
				-> UBoatUpgradeDataAsset*
			{
				if (const UBoatUpgradeDataAsset* ExistingUpgrade = EconomySubsystem->GetUpgradeAssetById(UpgradeId))
				{
					return const_cast<UBoatUpgradeDataAsset*>(ExistingUpgrade);
				}

				UBoatUpgradeDataAsset* RuntimeFallbackUpgrade = NewObject<UBoatUpgradeDataAsset>(this);
				RuntimeFallbackUpgrade->UpgradeId = UpgradeId;
				RuntimeFallbackUpgrade->DisplayName = FText::FromString(DisplayNameText);
				RuntimeFallbackUpgrade->Description = FText::FromString(DescriptionText);
				RuntimeFallbackUpgrade->CreditCost = FMath::Max(0, CreditCost);
				RuntimeFallbackUpgrade->MaxSpeedMultiplier = FMath::Max(0.1f, MaxSpeedMultiplier);
				RuntimeFallbackUpgrade->DragMultiplier = FMath::Max(0.1f, DragMultiplier);
				RuntimeFallbackUpgrade->TurnRateMultiplier = FMath::Max(0.1f, TurnRateMultiplier);
				EconomySubsystem->RegisterUpgradeAsset(RuntimeFallbackUpgrade);
				UE_LOG(LogTemp, Warning, TEXT("SailingGameMode: Bruker runtime fallback for oppgradering '%s'."), *UpgradeId.ToString());
				return RuntimeFallbackUpgrade;
			};

			EnsureUpgrade(
				TEXT("SkrogTrimV1"),
				450,
				1.06f,
				0.96f,
				1.0f,
				TEXT("Skrogtrim V1"),
				TEXT("Lett skrogtrim som reduserer motstand i moderate hastigheter."));
			EnsureUpgrade(
				TEXT("RorResponsV1"),
				600,
				1.0f,
				1.0f,
				1.15f,
				TEXT("Rorrespons V1"),
				TEXT("Forbedret rorutslag for raskere vending i trange farvann."));
			EnsureUpgrade(
				TEXT("RiggeffektivitetV1"),
				850,
				1.1f,
				0.94f,
				1.05f,
				TEXT("Riggeffektivitet V1"),
				TEXT("Optimaliserte seiltrimlinjer for bedre fremdrift i kryss."));

			EconomySubsystem->SetCredits(SaveGame ? SaveGame->PlayerCredits : 0);
			EconomySubsystem->SetBoatConditionPercent(SaveGame ? SaveGame->BoatConditionPercent : 100);
			EconomySubsystem->SetUnlockedUpgrades(SaveGame ? SaveGame->UnlockedUpgradeIds : TArray<FName>());
		}

		if (UMissionSubsystem* MissionSubsystem = GI->GetSubsystem<UMissionSubsystem>())
		{
			MissionSubsystem->SetMissionAssetPath(MissionAssetScanPath);

			const FName DiscoveryMissionId(TEXT("OppdagForsteOy"));
			const FName DeliveryMissionId(TEXT("LeveringTilBoye"));

			auto EnsureMission = [this, MissionSubsystem](FName MissionId, TFunction<void(USailingMissionDataAsset*)> ConfigureFallback)
				-> USailingMissionDataAsset*
			{
				if (const USailingMissionDataAsset* ExistingMission = MissionSubsystem->GetMissionAssetById(MissionId))
				{
					return const_cast<USailingMissionDataAsset*>(ExistingMission);
				}

				USailingMissionDataAsset* RuntimeFallbackMission = NewObject<USailingMissionDataAsset>(this);
				RuntimeFallbackMission->MissionId = MissionId;
				ConfigureFallback(RuntimeFallbackMission);
				MissionSubsystem->RegisterMissionAsset(RuntimeFallbackMission);
				UE_LOG(LogTemp, Warning, TEXT("SailingGameMode: Bruker runtime fallback for oppdrag '%s'."), *MissionId.ToString());
				return RuntimeFallbackMission;
			};

			EnsureMission(DiscoveryMissionId, [DeliveryMissionId](USailingMissionDataAsset* Mission)
			{
				Mission->DisplayName = FText::FromString(TEXT("Oppdag første øy"));
				Mission->Description = FText::FromString(TEXT("Seil ut og oppdag din første øy."));
				Mission->MissionType = ESailingMissionType::NavigationChallenge;
				Mission->RewardCredits = 250;
				Mission->bRepeatable = false;
				Mission->NextMissionId = DeliveryMissionId;
			});

			EnsureMission(DeliveryMissionId, [DeliveryObjectiveLocation](USailingMissionDataAsset* Mission)
			{
				Mission->DisplayName = FText::FromString(TEXT("Levering til bøye"));
				Mission->Description = FText::FromString(TEXT("Seil til leveringsbøyen og fullfør leveransen."));
				Mission->MissionType = ESailingMissionType::Delivery;
				Mission->RewardCredits = 400;
				Mission->StartWorldLocation = FVector::ZeroVector;
				Mission->EndWorldLocation = DeliveryObjectiveLocation;
				Mission->bRequireLocationMatch = true;
				Mission->CompletionRadius = 1200.0f;
				Mission->bRepeatable = true;
			});

			MissionSubsystem->SetCompletedMissionIds(SaveGame ? SaveGame->CompletedMissionIds : TArray<FName>());
			MissionSubsystem->SetMissionBoardSelectionHistory(SaveGame ? SaveGame->MissionBoardSelectionHistory : TArray<FMissionBoardSelectionEntry>());
			MissionSubsystem->SetActiveMissionId(SaveGame ? SaveGame->ActiveMissionId : NAME_None);
			if (MissionSubsystem->GetActiveMissionId().IsNone() && SaveGame)
			{
				if (SaveGame->TotalIslandsDiscovered == 0)
				{
					MissionSubsystem->ActivateMissionFromCandidates({ DiscoveryMissionId }, false);
				}
				else
				{
					MissionSubsystem->ActivateMissionFromCandidates({ DeliveryMissionId }, false);
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
				SaveGame->MissionBoardSelectionHistory = MissionSubsystem->GetMissionBoardSelectionHistory();
			}

			UE_LOG(LogTemp, Log, TEXT("SailingGameMode: Aktivt oppdrag '%s'."), *MissionSubsystem->GetActiveMissionId().ToString());
		}

		if (UTelemetrySubsystem* TelemetrySubsystem = GI->GetSubsystem<UTelemetrySubsystem>())
		{
			TelemetrySubsystem->SetAllCounters(SaveGame ? SaveGame->TelemetryCounters : TMap<FName, int32>());
		}

		if (UWorldStreamingSubsystem* WorldStreamingSubsystem = GI->GetSubsystem<UWorldStreamingSubsystem>())
		{
			WorldStreamingSubsystem->SetPortVisitStats(
				SaveGame ? SaveGame->LastVisitedPortId : NAME_None,
				SaveGame ? SaveGame->PortVisitCounts : TMap<FName, int32>());
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
				PortNord->bRestrictToOfferedMissions = true;
				PortNord->OfferedMissionIds = { TEXT("LeveringTilBoye"), TEXT("OppdagForsteOy") };
				{
					FPortMissionWeightedOffer WeightedDeliveryOffer;
					WeightedDeliveryOffer.MissionId = TEXT("LeveringTilBoye");
					WeightedDeliveryOffer.PriorityWeight = 2.0f;
					PortNord->WeightedOfferedMissions.Add(WeightedDeliveryOffer);

					FPortMissionWeightedOffer WeightedDiscoveryOffer;
					WeightedDiscoveryOffer.MissionId = TEXT("OppdagForsteOy");
					WeightedDiscoveryOffer.PriorityWeight = 1.0f;
					PortNord->WeightedOfferedMissions.Add(WeightedDiscoveryOffer);
				}
				PortNord->bCycleMissionOnDock = true;
				PortNord->MissionBoardCooldownSeconds = 12.0f;
				PortNord->bOfferUpgradeService = true;
				PortNord->OfferedUpgradeIds = { TEXT("SkrogTrimV1"), TEXT("RorResponsV1") };
				PortNord->bRotateUpgradeStockByVisits = true;
				PortNord->UpgradeCostMultiplier = 0.95f;
				PortDefinitions.Add(PortNord);

				UPortDataAsset* PortVest = NewObject<UPortDataAsset>(this);
				PortVest->PortId = TEXT("HavnVest");
				PortVest->DisplayName = FText::FromString(TEXT("Vesthavn"));
				PortVest->WorldLocation = FVector(-7000.0f, 1500.0f, 100.0f);
				PortVest->bRestrictToOfferedMissions = true;
				PortVest->OfferedMissionIds = { TEXT("LeveringTilBoye") };
				{
					FPortMissionWeightedOffer WeightedDeliveryOffer;
					WeightedDeliveryOffer.MissionId = TEXT("LeveringTilBoye");
					WeightedDeliveryOffer.PriorityWeight = 1.5f;
					PortVest->WeightedOfferedMissions.Add(WeightedDeliveryOffer);
				}
				PortVest->bCycleMissionOnDock = true;
				PortVest->MissionBoardCooldownSeconds = 12.0f;
				PortVest->bOfferUpgradeService = true;
				PortVest->OfferedUpgradeIds = { TEXT("RorResponsV1"), TEXT("RiggeffektivitetV1") };
				PortVest->bRotateUpgradeStockByVisits = true;
				PortVest->UpgradeCostMultiplier = 1.05f;
				PortDefinitions.Add(PortVest);

				UPortDataAsset* PortSor = NewObject<UPortDataAsset>(this);
				PortSor->PortId = TEXT("HavnSor");
				PortSor->DisplayName = FText::FromString(TEXT("Sorhavn"));
				PortSor->WorldLocation = FVector(8500.0f, 4500.0f, 100.0f);
				PortSor->bRestrictToOfferedMissions = true;
				PortSor->OfferedMissionIds = { TEXT("LeveringTilBoye") };
				{
					FPortMissionWeightedOffer WeightedDeliveryOffer;
					WeightedDeliveryOffer.MissionId = TEXT("LeveringTilBoye");
					WeightedDeliveryOffer.PriorityWeight = 1.0f;
					WeightedDeliveryOffer.MinPortVisits = 1;
					PortSor->WeightedOfferedMissions.Add(WeightedDeliveryOffer);
				}
				PortSor->bCycleMissionOnDock = true;
				PortSor->MissionBoardCooldownSeconds = 12.0f;
				PortSor->bOfferUpgradeService = true;
				PortSor->OfferedUpgradeIds = { TEXT("SkrogTrimV1"), TEXT("RiggeffektivitetV1") };
				PortSor->bRotateUpgradeStockByVisits = false;
				PortSor->UpgradeCostMultiplier = 1.12f;
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
				PortMarker->bOfferMissionBoard = PortData->bOfferMissionBoard;
				PortMarker->bCycleMissionOnDock = PortData->bCycleMissionOnDock;
				PortMarker->bRestrictToOfferedMissions = PortData->bRestrictToOfferedMissions;
				PortMarker->OfferedMissionIds = PortData->OfferedMissionIds;
				PortMarker->WeightedOfferedMissions = PortData->WeightedOfferedMissions;
				PortMarker->MaxOfferedMissionsAtBoard = PortData->MaxOfferedMissionsAtBoard;
				PortMarker->MissionBoardCooldownSeconds = PortData->MissionBoardCooldownSeconds;
				PortMarker->bOfferUpgradeService = PortData->bOfferUpgradeService;
				PortMarker->OfferedUpgradeIds = PortData->OfferedUpgradeIds;
				PortMarker->MaxOfferedUpgrades = PortData->MaxOfferedUpgrades;
				PortMarker->bRotateUpgradeStockByVisits = PortData->bRotateUpgradeStockByVisits;
				PortMarker->UpgradeCostMultiplier = PortData->UpgradeCostMultiplier;

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
				SaveGame->MissionBoardSelectionHistory = MissionSubsystem->GetMissionBoardSelectionHistory();
			}

			if (UTelemetrySubsystem* TelemetrySubsystem = GI->GetSubsystem<UTelemetrySubsystem>())
			{
				SaveGame->TelemetryCounters = TelemetrySubsystem->GetAllCounters();
			}

			if (UWorldStreamingSubsystem* WorldStreamingSubsystem = GI->GetSubsystem<UWorldStreamingSubsystem>())
			{
				SaveGame->LastVisitedPortId = WorldStreamingSubsystem->GetLastVisitedPortId();
				SaveGame->PortVisitCounts = WorldStreamingSubsystem->GetPortVisitCounts();
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
