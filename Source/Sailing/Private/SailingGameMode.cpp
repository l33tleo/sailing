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
	RequiredStartupMissionIds = { TEXT("OppdagForsteOy"), TEXT("LeveringTilBoye") };
	RequiredStartupUpgradeIds = { TEXT("SkrogTrimV1"), TEXT("RorResponsV1"), TEXT("RiggeffektivitetV1") };
	RequiredStartupPortIds = { TEXT("HavnNord"), TEXT("HavnVest"), TEXT("HavnSor") };
}

void ASailingGameMode::BeginPlay()
{
	Super::BeginPlay();

	const FVector DeliveryObjectiveLocation(6500.0f, -2500.0f, 120.0f);
	const FName DiscoveryMissionId(TEXT("OppdagForsteOy"));
	const FName DeliveryMissionId(TEXT("LeveringTilBoye"));
	int32 StartupRegisteredMissionCount = 0;
	int32 StartupRegisteredUpgradeCount = 0;
	int32 StartupPortDefinitionCount = 0;
	int32 StartupSpawnedPortCount = 0;
	int32 MissingRequiredStartupMissionCount = 0;
	int32 MissingRequiredStartupUpgradeCount = 0;
	int32 MissingRequiredStartupPortCount = 0;

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
			int32 RuntimeFallbackUpgradeCount = 0;

			auto EnsureUpgrade = [this, EconomySubsystem](
				int32& OutRuntimeFallbackUpgradeCount,
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

				if (!bEnableRuntimeFallbackContent)
				{
					UE_LOG(LogTemp, Warning, TEXT("SailingGameMode: Mangler oppgradering '%s' i innholdsdata og runtime fallback er deaktivert."), *UpgradeId.ToString());
					return static_cast<UBoatUpgradeDataAsset*>(nullptr);
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
				OutRuntimeFallbackUpgradeCount++;
				UE_LOG(LogTemp, Warning, TEXT("SailingGameMode: Bruker runtime fallback for oppgradering '%s'."), *UpgradeId.ToString());
				return RuntimeFallbackUpgrade;
			};

			int32 MissingBootstrapUpgrades = 0;
			MissingBootstrapUpgrades += EnsureUpgrade(
				RuntimeFallbackUpgradeCount,
				TEXT("SkrogTrimV1"),
				450,
				1.06f,
				0.96f,
				1.0f,
				TEXT("Skrogtrim V1"),
				TEXT("Lett skrogtrim som reduserer motstand i moderate hastigheter.")) ? 0 : 1;
			MissingBootstrapUpgrades += EnsureUpgrade(
				RuntimeFallbackUpgradeCount,
				TEXT("RorResponsV1"),
				600,
				1.0f,
				1.0f,
				1.15f,
				TEXT("Rorrespons V1"),
				TEXT("Forbedret rorutslag for raskere vending i trange farvann.")) ? 0 : 1;
			MissingBootstrapUpgrades += EnsureUpgrade(
				RuntimeFallbackUpgradeCount,
				TEXT("RiggeffektivitetV1"),
				850,
				1.1f,
				0.94f,
				1.05f,
				TEXT("Riggeffektivitet V1"),
				TEXT("Optimaliserte seiltrimlinjer for bedre fremdrift i kryss.")) ? 0 : 1;

			if (MissingBootstrapUpgrades > 0)
			{
				UE_LOG(LogTemp, Warning, TEXT("SailingGameMode: %d bootstrap-oppgraderinger mangler."), MissingBootstrapUpgrades);
				if (UTelemetrySubsystem* TelemetrySubsystem = GI->GetSubsystem<UTelemetrySubsystem>())
				{
					TelemetrySubsystem->RecordCounterEvent(TEXT("MissingBootstrapUpgradeContent"), MissingBootstrapUpgrades);
				}
			}
			if (RuntimeFallbackUpgradeCount > 0)
			{
				if (UTelemetrySubsystem* TelemetrySubsystem = GI->GetSubsystem<UTelemetrySubsystem>())
				{
					TelemetrySubsystem->RecordCounterEvent(TEXT("RuntimeFallbackUpgrades"), RuntimeFallbackUpgradeCount);
				}
			}
			StartupRegisteredUpgradeCount = EconomySubsystem->GetRegisteredUpgradeIds().Num();
			if (RequiredStartupUpgradeIds.Num() > 0)
			{
				TSet<FName> RegisteredUpgradeIdSet;
				for (const FName& UpgradeId : EconomySubsystem->GetRegisteredUpgradeIds())
				{
					if (!UpgradeId.IsNone())
					{
						RegisteredUpgradeIdSet.Add(UpgradeId);
					}
				}

				TArray<FString> MissingRequiredUpgradeNames;
				for (const FName& RequiredUpgradeId : RequiredStartupUpgradeIds)
				{
					if (RequiredUpgradeId.IsNone() || RegisteredUpgradeIdSet.Contains(RequiredUpgradeId))
					{
						continue;
					}

					MissingRequiredStartupUpgradeCount++;
					MissingRequiredUpgradeNames.Add(RequiredUpgradeId.ToString());
				}

				if (MissingRequiredUpgradeNames.Num() > 0)
				{
					UE_LOG(LogTemp, Warning,
						TEXT("SailingGameMode: Mangler %d påkrevde startup-oppgraderinger: %s"),
						MissingRequiredStartupUpgradeCount,
						*FString::Join(MissingRequiredUpgradeNames, TEXT(", ")));
				}
			}

			EconomySubsystem->SetCredits(SaveGame ? SaveGame->PlayerCredits : 0);
			EconomySubsystem->SetBoatConditionPercent(SaveGame ? SaveGame->BoatConditionPercent : 100);
			EconomySubsystem->SetUnlockedUpgrades(SaveGame ? SaveGame->UnlockedUpgradeIds : TArray<FName>());
		}

		if (UMissionSubsystem* MissionSubsystem = GI->GetSubsystem<UMissionSubsystem>())
		{
			MissionSubsystem->SetMissionAssetPath(MissionAssetScanPath);
			int32 RuntimeFallbackMissionCount = 0;

			auto EnsureMission = [this, MissionSubsystem](int32& OutRuntimeFallbackMissionCount, FName MissionId, TFunction<void(USailingMissionDataAsset*)> ConfigureFallback)
				-> USailingMissionDataAsset*
			{
				if (const USailingMissionDataAsset* ExistingMission = MissionSubsystem->GetMissionAssetById(MissionId))
				{
					return const_cast<USailingMissionDataAsset*>(ExistingMission);
				}

				if (!bEnableRuntimeFallbackContent)
				{
					UE_LOG(LogTemp, Warning, TEXT("SailingGameMode: Mangler oppdrag '%s' i innholdsdata og runtime fallback er deaktivert."), *MissionId.ToString());
					return static_cast<USailingMissionDataAsset*>(nullptr);
				}

				USailingMissionDataAsset* RuntimeFallbackMission = NewObject<USailingMissionDataAsset>(this);
				RuntimeFallbackMission->MissionId = MissionId;
				ConfigureFallback(RuntimeFallbackMission);
				MissionSubsystem->RegisterMissionAsset(RuntimeFallbackMission);
				OutRuntimeFallbackMissionCount++;
				UE_LOG(LogTemp, Warning, TEXT("SailingGameMode: Bruker runtime fallback for oppdrag '%s'."), *MissionId.ToString());
				return RuntimeFallbackMission;
			};

			const USailingMissionDataAsset* BootstrapDiscoveryMission = EnsureMission(RuntimeFallbackMissionCount, DiscoveryMissionId, [DeliveryMissionId](USailingMissionDataAsset* Mission)
			{
				Mission->DisplayName = FText::FromString(TEXT("Oppdag første øy"));
				Mission->Description = FText::FromString(TEXT("Seil ut og oppdag din første øy."));
				Mission->MissionType = ESailingMissionType::NavigationChallenge;
				Mission->RewardCredits = 250;
				Mission->bRepeatable = false;
				Mission->NextMissionId = DeliveryMissionId;
			});

			const USailingMissionDataAsset* BootstrapDeliveryMission = EnsureMission(RuntimeFallbackMissionCount, DeliveryMissionId, [DeliveryObjectiveLocation](USailingMissionDataAsset* Mission)
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

			const int32 MissingBootstrapMissions =
				(BootstrapDiscoveryMission ? 0 : 1) +
				(BootstrapDeliveryMission ? 0 : 1);
			if (MissingBootstrapMissions > 0)
			{
				UE_LOG(LogTemp, Warning, TEXT("SailingGameMode: %d bootstrap-oppdrag mangler."), MissingBootstrapMissions);
				if (UTelemetrySubsystem* TelemetrySubsystem = GI->GetSubsystem<UTelemetrySubsystem>())
				{
					TelemetrySubsystem->RecordCounterEvent(TEXT("MissingBootstrapMissionContent"), MissingBootstrapMissions);
				}
			}
			if (RuntimeFallbackMissionCount > 0)
			{
				if (UTelemetrySubsystem* TelemetrySubsystem = GI->GetSubsystem<UTelemetrySubsystem>())
				{
					TelemetrySubsystem->RecordCounterEvent(TEXT("RuntimeFallbackMissions"), RuntimeFallbackMissionCount);
				}
			}

			StartupRegisteredMissionCount = MissionSubsystem->GetRegisteredMissionIds().Num();
			if (RequiredStartupMissionIds.Num() > 0)
			{
				TSet<FName> RegisteredMissionIdSet;
				for (const FName& MissionId : MissionSubsystem->GetRegisteredMissionIds())
				{
					if (!MissionId.IsNone())
					{
						RegisteredMissionIdSet.Add(MissionId);
					}
				}

				TArray<FString> MissingRequiredMissionNames;
				for (const FName& RequiredMissionId : RequiredStartupMissionIds)
				{
					if (RequiredMissionId.IsNone() || RegisteredMissionIdSet.Contains(RequiredMissionId))
					{
						continue;
					}

					MissingRequiredStartupMissionCount++;
					MissingRequiredMissionNames.Add(RequiredMissionId.ToString());
				}

				if (MissingRequiredMissionNames.Num() > 0)
				{
					UE_LOG(LogTemp, Warning,
						TEXT("SailingGameMode: Mangler %d påkrevde startup-oppdrag: %s"),
						MissingRequiredStartupMissionCount,
						*FString::Join(MissingRequiredMissionNames, TEXT(", ")));
				}
			}

			MissionSubsystem->SetCompletedMissionIds(SaveGame ? SaveGame->CompletedMissionIds : TArray<FName>());
			MissionSubsystem->SetMissionBoardSelectionHistory(SaveGame ? SaveGame->MissionBoardSelectionHistory : TArray<FMissionBoardSelectionEntry>());
			MissionSubsystem->SetActiveMissionId(SaveGame ? SaveGame->ActiveMissionId : NAME_None);
			if (MissionSubsystem->GetActiveMissionId().IsNone() && SaveGame)
			{
				TArray<FName> BootstrapCandidates;
				if (SaveGame->TotalIslandsDiscovered == 0)
				{
					if (BootstrapDiscoveryMission)
					{
						BootstrapCandidates.Add(DiscoveryMissionId);
					}
					if (BootstrapDeliveryMission)
					{
						BootstrapCandidates.Add(DeliveryMissionId);
					}
				}
				else
				{
					if (BootstrapDeliveryMission)
					{
						BootstrapCandidates.Add(DeliveryMissionId);
					}
					if (BootstrapDiscoveryMission)
					{
						BootstrapCandidates.Add(DiscoveryMissionId);
					}
				}

				if (BootstrapCandidates.Num() > 0)
				{
					MissionSubsystem->ActivateMissionFromCandidates(BootstrapCandidates, false);
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

	// Align objective marker with whichever mission is currently active.
	SyncActiveMissionObjectiveMarker();

	// Spawn simple harbor markers and register with world subsystem
	if (GI)
	{
		if (UWorldStreamingSubsystem* WorldStreamingSubsystem = GI->GetSubsystem<UWorldStreamingSubsystem>())
		{
			WorldStreamingSubsystem->ClearPortPoints();
			SpawnedPortMarkers.Empty();

			TArray<UPortDataAsset*> PortDefinitions;
			int32 RuntimeFallbackPortCount = 0;
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
			if (PortDefinitions.Num() == 0 && bEnableRuntimeFallbackContent)
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
				PortNord->bAllowManualBoardRefresh = true;
				PortNord->ManualBoardRefreshCooldownSeconds = 6.0f;
				PortNord->ManualBoardRefreshCreditCost = 15;
				PortNord->bOfferUpgradeService = true;
				PortNord->OfferedUpgradeIds = { TEXT("SkrogTrimV1"), TEXT("RorResponsV1") };
				{
					FPortUpgradeWeightedOffer WeightedTrim;
					WeightedTrim.UpgradeId = TEXT("SkrogTrimV1");
					WeightedTrim.PriorityWeight = 1.8f;
					PortNord->WeightedOfferedUpgrades.Add(WeightedTrim);

					FPortUpgradeWeightedOffer WeightedRudder;
					WeightedRudder.UpgradeId = TEXT("RorResponsV1");
					WeightedRudder.PriorityWeight = 1.2f;
					WeightedRudder.MinPortVisits = 1;
					PortNord->WeightedOfferedUpgrades.Add(WeightedRudder);
				}
				PortNord->bRotateUpgradeStockByVisits = true;
				PortNord->bHideUnlockedUpgradesOnBoard = true;
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
				PortVest->bAllowManualBoardRefresh = true;
				PortVest->ManualBoardRefreshCooldownSeconds = 4.0f;
				PortVest->ManualBoardRefreshCreditCost = 25;
				PortVest->bOfferUpgradeService = true;
				PortVest->OfferedUpgradeIds = { TEXT("RorResponsV1"), TEXT("RiggeffektivitetV1") };
				{
					FPortUpgradeWeightedOffer WeightedRudder;
					WeightedRudder.UpgradeId = TEXT("RorResponsV1");
					WeightedRudder.PriorityWeight = 1.4f;
					PortVest->WeightedOfferedUpgrades.Add(WeightedRudder);

					FPortUpgradeWeightedOffer WeightedRig;
					WeightedRig.UpgradeId = TEXT("RiggeffektivitetV1");
					WeightedRig.PriorityWeight = 2.0f;
					WeightedRig.MinPortVisits = 2;
					PortVest->WeightedOfferedUpgrades.Add(WeightedRig);
				}
				PortVest->bRotateUpgradeStockByVisits = true;
				PortVest->bHideUnlockedUpgradesOnBoard = false;
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
				PortSor->bAllowManualBoardRefresh = false;
				PortSor->ManualBoardRefreshCooldownSeconds = 0.0f;
				PortSor->ManualBoardRefreshCreditCost = 0;
				PortSor->bOfferUpgradeService = true;
				PortSor->OfferedUpgradeIds = { TEXT("SkrogTrimV1"), TEXT("RiggeffektivitetV1") };
				{
					FPortUpgradeWeightedOffer WeightedRig;
					WeightedRig.UpgradeId = TEXT("RiggeffektivitetV1");
					WeightedRig.PriorityWeight = 2.2f;
					WeightedRig.MinPortVisits = 1;
					PortSor->WeightedOfferedUpgrades.Add(WeightedRig);

					FPortUpgradeWeightedOffer WeightedTrim;
					WeightedTrim.UpgradeId = TEXT("SkrogTrimV1");
					WeightedTrim.PriorityWeight = 1.0f;
					PortSor->WeightedOfferedUpgrades.Add(WeightedTrim);
				}
				PortSor->bRotateUpgradeStockByVisits = false;
				PortSor->bHideUnlockedUpgradesOnBoard = true;
				PortSor->UpgradeCostMultiplier = 1.12f;
				PortDefinitions.Add(PortSor);
				RuntimeFallbackPortCount = PortDefinitions.Num();
			}
			else if (PortDefinitions.Num() == 0)
			{
				UE_LOG(LogTemp, Warning, TEXT("SailingGameMode: Ingen port-data funnet og runtime fallback er deaktivert."));
			}
			StartupPortDefinitionCount = PortDefinitions.Num();

			if (RuntimeFallbackPortCount > 0)
			{
				if (UTelemetrySubsystem* TelemetrySubsystem = GI->GetSubsystem<UTelemetrySubsystem>())
				{
					TelemetrySubsystem->RecordCounterEvent(TEXT("RuntimeFallbackPorts"), RuntimeFallbackPortCount);
				}
			}

			TSet<FName> AllowedMissionIds;
			if (UMissionSubsystem* MissionSubsystem = GI->GetSubsystem<UMissionSubsystem>())
			{
				for (const FName& MissionId : MissionSubsystem->GetRegisteredMissionIds())
				{
					if (!MissionId.IsNone())
					{
						AllowedMissionIds.Add(MissionId);
					}
				}
			}

			TSet<FName> AllowedUpgradeIds;
			if (UEconomySubsystem* EconomySubsystem = GI->GetSubsystem<UEconomySubsystem>())
			{
				for (const FName& UpgradeId : EconomySubsystem->GetRegisteredUpgradeIds())
				{
					if (!UpgradeId.IsNone())
					{
						AllowedUpgradeIds.Add(UpgradeId);
					}
				}
			}

			TSet<FName> SpawnedPortIds;
			int32 InvalidPortDefinitionCount = 0;
			int32 DuplicatePortDefinitionCount = 0;
			int32 RejectedMissionReferenceCount = 0;
			int32 RejectedUpgradeReferenceCount = 0;
			for (const UPortDataAsset* PortData : PortDefinitions)
			{
				if (!PortData || PortData->PortId.IsNone())
				{
					InvalidPortDefinitionCount++;
					continue;
				}

				if (SpawnedPortIds.Contains(PortData->PortId))
				{
					DuplicatePortDefinitionCount++;
					continue;
				}

				SpawnedPortIds.Add(PortData->PortId);
				int32 RejectedMissionOffers = 0;
				const TArray<FName> ValidOfferedMissionIds = UPortDataAsset::FilterIdsByAllowedSet(
					PortData->OfferedMissionIds,
					AllowedMissionIds,
					RejectedMissionOffers);
				int32 RejectedWeightedMissionOffers = 0;
				const TArray<FPortMissionWeightedOffer> ValidWeightedMissionOffers = UPortDataAsset::FilterMissionWeightedOffersByAllowedSet(
					PortData->WeightedOfferedMissions,
					AllowedMissionIds,
					RejectedWeightedMissionOffers);
				RejectedMissionReferenceCount += RejectedMissionOffers + RejectedWeightedMissionOffers;

				int32 RejectedUpgradeOffers = 0;
				const TArray<FName> ValidOfferedUpgradeIds = UPortDataAsset::FilterIdsByAllowedSet(
					PortData->OfferedUpgradeIds,
					AllowedUpgradeIds,
					RejectedUpgradeOffers);
				int32 RejectedWeightedUpgradeOffers = 0;
				const TArray<FPortUpgradeWeightedOffer> ValidWeightedUpgradeOffers = UPortDataAsset::FilterUpgradeWeightedOffersByAllowedSet(
					PortData->WeightedOfferedUpgrades,
					AllowedUpgradeIds,
					RejectedWeightedUpgradeOffers);
				RejectedUpgradeReferenceCount += RejectedUpgradeOffers + RejectedWeightedUpgradeOffers;

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
				PortMarker->OfferedMissionIds = ValidOfferedMissionIds;
				PortMarker->WeightedOfferedMissions = ValidWeightedMissionOffers;
				PortMarker->MaxOfferedMissionsAtBoard = PortData->MaxOfferedMissionsAtBoard;
				PortMarker->MissionBoardCooldownSeconds = PortData->MissionBoardCooldownSeconds;
				PortMarker->bAllowManualBoardRefresh = PortData->bAllowManualBoardRefresh;
				PortMarker->ManualBoardRefreshCooldownSeconds = PortData->ManualBoardRefreshCooldownSeconds;
				PortMarker->ManualBoardRefreshCreditCost = PortData->ManualBoardRefreshCreditCost;
				PortMarker->bOfferUpgradeService = PortData->bOfferUpgradeService;
				PortMarker->OfferedUpgradeIds = ValidOfferedUpgradeIds;
				PortMarker->WeightedOfferedUpgrades = ValidWeightedUpgradeOffers;
				PortMarker->MaxOfferedUpgrades = PortData->MaxOfferedUpgrades;
				PortMarker->bRotateUpgradeStockByVisits = PortData->bRotateUpgradeStockByVisits;
				PortMarker->bHideUnlockedUpgradesOnBoard = PortData->bHideUnlockedUpgradesOnBoard;
				PortMarker->UpgradeCostMultiplier = PortData->UpgradeCostMultiplier;

				SpawnedPortMarkers.Add(PortMarker);
				WorldStreamingSubsystem->RegisterPortPoint(PortData->PortId, PortData->WorldLocation);
			}

			if (InvalidPortDefinitionCount > 0)
			{
				UE_LOG(LogTemp, Warning, TEXT("SailingGameMode: Ignorerte %d ugyldige port-definisjoner (manglende PortId)."), InvalidPortDefinitionCount);
			}
			if (DuplicatePortDefinitionCount > 0)
			{
				UE_LOG(LogTemp, Warning, TEXT("SailingGameMode: Ignorerte %d dupliserte port-definisjoner."), DuplicatePortDefinitionCount);
			}
			if (RejectedMissionReferenceCount > 0)
			{
				UE_LOG(LogTemp, Warning, TEXT("SailingGameMode: Filtrerte bort %d ugyldige oppdragsreferanser fra portinnhold."), RejectedMissionReferenceCount);
			}
			if (RejectedUpgradeReferenceCount > 0)
			{
				UE_LOG(LogTemp, Warning, TEXT("SailingGameMode: Filtrerte bort %d ugyldige oppgraderingsreferanser fra portinnhold."), RejectedUpgradeReferenceCount);
			}
			if (RequiredStartupPortIds.Num() > 0)
			{
				TArray<FString> MissingRequiredPortNames;
				for (const FName& RequiredPortId : RequiredStartupPortIds)
				{
					if (RequiredPortId.IsNone() || SpawnedPortIds.Contains(RequiredPortId))
					{
						continue;
					}

					MissingRequiredStartupPortCount++;
					MissingRequiredPortNames.Add(RequiredPortId.ToString());
				}

				if (MissingRequiredPortNames.Num() > 0)
				{
					UE_LOG(LogTemp, Warning,
						TEXT("SailingGameMode: Mangler %d påkrevde startup-havner: %s"),
						MissingRequiredStartupPortCount,
						*FString::Join(MissingRequiredPortNames, TEXT(", ")));
				}
			}
			if (UTelemetrySubsystem* TelemetrySubsystem = GI->GetSubsystem<UTelemetrySubsystem>())
			{
				if (InvalidPortDefinitionCount > 0)
				{
					TelemetrySubsystem->RecordCounterEvent(TEXT("InvalidPortDefinitions"), InvalidPortDefinitionCount);
				}
				if (DuplicatePortDefinitionCount > 0)
				{
					TelemetrySubsystem->RecordCounterEvent(TEXT("DuplicatePortDefinitions"), DuplicatePortDefinitionCount);
				}
				if (RejectedMissionReferenceCount > 0)
				{
					TelemetrySubsystem->RecordCounterEvent(TEXT("InvalidPortMissionOfferRefs"), RejectedMissionReferenceCount);
				}
				if (RejectedUpgradeReferenceCount > 0)
				{
					TelemetrySubsystem->RecordCounterEvent(TEXT("InvalidPortUpgradeOfferRefs"), RejectedUpgradeReferenceCount);
				}
				if (SpawnedPortMarkers.Num() == 0 && !bEnableRuntimeFallbackContent)
				{
					TelemetrySubsystem->RecordCounterEvent(TEXT("MissingBootstrapPortContent"), 1);
				}
			}

			StartupSpawnedPortCount = SpawnedPortMarkers.Num();
			UE_LOG(LogTemp, Log, TEXT("SailingGameMode: Spawned %d harbor markers."), SpawnedPortMarkers.Num());
		}
	}

	if (GI)
	{
		if (UTelemetrySubsystem* TelemetrySubsystem = GI->GetSubsystem<UTelemetrySubsystem>())
		{
			TelemetrySubsystem->SetCounterValue(TEXT("StartupRegisteredMissions"), FMath::Max(0, StartupRegisteredMissionCount));
			TelemetrySubsystem->SetCounterValue(TEXT("StartupRegisteredUpgrades"), FMath::Max(0, StartupRegisteredUpgradeCount));
			TelemetrySubsystem->SetCounterValue(TEXT("StartupPortDefinitions"), FMath::Max(0, StartupPortDefinitionCount));
			TelemetrySubsystem->SetCounterValue(TEXT("StartupSpawnedPorts"), FMath::Max(0, StartupSpawnedPortCount));
			TelemetrySubsystem->SetCounterValue(TEXT("StartupMissingRequiredMissions"), FMath::Max(0, MissingRequiredStartupMissionCount));
			TelemetrySubsystem->SetCounterValue(TEXT("StartupMissingRequiredUpgrades"), FMath::Max(0, MissingRequiredStartupUpgradeCount));
			TelemetrySubsystem->SetCounterValue(TEXT("StartupMissingRequiredPorts"), FMath::Max(0, MissingRequiredStartupPortCount));
			TelemetrySubsystem->SetCounterValue(TEXT("RuntimeFallbackEnabled"), bEnableRuntimeFallbackContent ? 1 : 0);
			if (!bEnableRuntimeFallbackContent && (StartupRegisteredMissionCount == 0 || MissingRequiredStartupMissionCount > 0))
			{
				TelemetrySubsystem->RecordCounterEvent(TEXT("AssetOnlyStartupMissingMissions"), 1);
			}
			if (!bEnableRuntimeFallbackContent && (StartupRegisteredUpgradeCount == 0 || MissingRequiredStartupUpgradeCount > 0))
			{
				TelemetrySubsystem->RecordCounterEvent(TEXT("AssetOnlyStartupMissingUpgrades"), 1);
			}
			if (!bEnableRuntimeFallbackContent && (StartupSpawnedPortCount == 0 || MissingRequiredStartupPortCount > 0))
			{
				TelemetrySubsystem->RecordCounterEvent(TEXT("AssetOnlyStartupMissingPorts"), 1);
			}
		}
	}

	UE_LOG(LogTemp, Log,
		TEXT("SailingGameMode: Startup content summary Missions=%d (MissingRequired=%d) Upgrades=%d (MissingRequired=%d) PortDefs=%d SpawnedPorts=%d (MissingRequired=%d) RuntimeFallback=%s"),
		StartupRegisteredMissionCount,
		MissingRequiredStartupMissionCount,
		StartupRegisteredUpgradeCount,
		MissingRequiredStartupUpgradeCount,
		StartupPortDefinitionCount,
		StartupSpawnedPortCount,
		MissingRequiredStartupPortCount,
		bEnableRuntimeFallbackContent ? TEXT("ON") : TEXT("OFF"));

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

bool ASailingGameMode::SyncActiveMissionObjectiveMarker()
{
	USailingGameInstance* GI = Cast<USailingGameInstance>(GetGameInstance());
	UMissionSubsystem* MissionSubsystem = GI ? GI->GetSubsystem<UMissionSubsystem>() : nullptr;
	if (!MissionSubsystem)
	{
		if (SpawnedMissionObjective)
		{
			SpawnedMissionObjective->Destroy();
			SpawnedMissionObjective = nullptr;
		}
		if (UTelemetrySubsystem* TelemetrySubsystem = GI ? GI->GetSubsystem<UTelemetrySubsystem>() : nullptr)
		{
			TelemetrySubsystem->SetCounterValue(TEXT("ActiveMissionObjectivePresent"), 0);
		}
		return false;
	}

	FVector ObjectiveLocation = FVector::ZeroVector;
	ESailingMissionType ObjectiveTriggerType = ESailingMissionType::NavigationChallenge;
	const bool bRequiresObjectiveMarker = MissionSubsystem->GetMissionObjectiveMarkerConfig(
		MissionSubsystem->GetActiveMissionId(),
		ObjectiveLocation,
		ObjectiveTriggerType);
	if (!bRequiresObjectiveMarker)
	{
		if (SpawnedMissionObjective)
		{
			SpawnedMissionObjective->Destroy();
			SpawnedMissionObjective = nullptr;
			if (UTelemetrySubsystem* TelemetrySubsystem = GI ? GI->GetSubsystem<UTelemetrySubsystem>() : nullptr)
			{
				TelemetrySubsystem->RecordCounterEvent(TEXT("ActiveMissionObjectiveRemoved"), 1);
				TelemetrySubsystem->SetCounterValue(TEXT("ActiveMissionObjectivePresent"), 0);
				TelemetrySubsystem->RecordCounterEvent(TEXT("DeliveryObjectiveSkipped"), 1);
			}
		}
		else if (UTelemetrySubsystem* TelemetrySubsystem = GI ? GI->GetSubsystem<UTelemetrySubsystem>() : nullptr)
		{
			TelemetrySubsystem->SetCounterValue(TEXT("ActiveMissionObjectivePresent"), 0);
			TelemetrySubsystem->RecordCounterEvent(TEXT("DeliveryObjectiveSkipped"), 1);
		}
		return false;
	}

	if (!SpawnedMissionObjective)
	{
		FActorSpawnParameters Params;
		Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		SpawnedMissionObjective = GetWorld()->SpawnActor<AMissionObjectiveActor>(
			AMissionObjectiveActor::StaticClass(), ObjectiveLocation, FRotator::ZeroRotator, Params);
		if (!SpawnedMissionObjective)
		{
			UE_LOG(LogTemp, Warning, TEXT("SailingGameMode: Klarte ikke å spawne objective actor ved %s."),
				*ObjectiveLocation.ToCompactString());
			if (UTelemetrySubsystem* TelemetrySubsystem = GI ? GI->GetSubsystem<UTelemetrySubsystem>() : nullptr)
			{
				TelemetrySubsystem->RecordCounterEvent(TEXT("DeliveryObjectiveSpawnFailed"), 1);
				TelemetrySubsystem->SetCounterValue(TEXT("ActiveMissionObjectivePresent"), 0);
			}
			return false;
		}

		SpawnedMissionObjective->bDestroyAfterCompletion = false;
		if (UTelemetrySubsystem* TelemetrySubsystem = GI ? GI->GetSubsystem<UTelemetrySubsystem>() : nullptr)
		{
			TelemetrySubsystem->RecordCounterEvent(TEXT("DeliveryObjectiveSpawned"), 1);
			TelemetrySubsystem->RecordCounterEvent(TEXT("ActiveMissionObjectiveSpawned"), 1);
		}
	}

	SpawnedMissionObjective->SetActorLocation(ObjectiveLocation);
	SpawnedMissionObjective->SetTriggerType(ObjectiveTriggerType);
	SpawnedMissionObjective->SetActorHiddenInGame(false);
	SpawnedMissionObjective->SetActorEnableCollision(true);

	if (UTelemetrySubsystem* TelemetrySubsystem = GI ? GI->GetSubsystem<UTelemetrySubsystem>() : nullptr)
	{
		TelemetrySubsystem->SetCounterValue(TEXT("ActiveMissionObjectivePresent"), 1);
		TelemetrySubsystem->SetCounterValue(TEXT("DeliveryObjectiveTriggerType"), static_cast<int32>(ObjectiveTriggerType));
		TelemetrySubsystem->SetCounterValue(TEXT("ActiveMissionObjectiveTriggerType"), static_cast<int32>(ObjectiveTriggerType));
	}

	return true;
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
