#include "SailingHUD.h"
#include "WindActor.h"
#include "SailboatPawn.h"
#include "IslandActor.h"
#include "SailingGameMode.h"
#include "SailingPlayerController.h"
#include "SaveGameSailing.h"
#include "Systems/SailingCoreSubsystems.h"
#include "Data/BoatUpgradeDataAsset.h"
#include "Data/PortDataAsset.h"
#include "UI/SailingHUDOverlayWidget.h"
#include "UI/PortMissionBoardWidget.h"
#include "Engine/Canvas.h"
#include "Engine/Font.h"
#include "Kismet/GameplayStatics.h"

namespace
{
void RecordMissionOfferBlockedReasonSummaryTelemetry(UTelemetrySubsystem* TelemetrySubsystem, const FPortMissionBoardData& BoardData)
{
	if (!TelemetrySubsystem)
	{
		return;
	}

	int32 DisabledCount = 0;
	int32 CooldownCount = 0;
	int32 NotOfferedCount = 0;
	int32 AlreadyActiveCount = 0;
	int32 InvalidMissionCount = 0;
	for (const FPortMissionOfferBlockReasonSummaryEntry& SummaryEntry : BoardData.MissionBlockedReasonSummary)
	{
		const int32 SafeCount = FMath::Max(0, SummaryEntry.Count);
		if (SafeCount <= 0)
		{
			continue;
		}

		switch (SummaryEntry.BlockReason)
		{
		case EPortMissionOfferActionBlockReason::MissionBoardDisabled:
			DisabledCount += SafeCount;
			TelemetrySubsystem->RecordCounterEvent(TEXT("MissionBoardBlockedMissionReason_BoardDisabled"), SafeCount);
			break;
		case EPortMissionOfferActionBlockReason::MissionBoardCooldown:
			CooldownCount += SafeCount;
			TelemetrySubsystem->RecordCounterEvent(TEXT("MissionBoardBlockedMissionReason_Cooldown"), SafeCount);
			break;
		case EPortMissionOfferActionBlockReason::NotOfferedAtPort:
			NotOfferedCount += SafeCount;
			TelemetrySubsystem->RecordCounterEvent(TEXT("MissionBoardBlockedMissionReason_NotOffered"), SafeCount);
			break;
		case EPortMissionOfferActionBlockReason::AlreadyActiveMission:
			AlreadyActiveCount += SafeCount;
			TelemetrySubsystem->RecordCounterEvent(TEXT("MissionBoardBlockedMissionReason_AlreadyActive"), SafeCount);
			break;
		case EPortMissionOfferActionBlockReason::InvalidMission:
			InvalidMissionCount += SafeCount;
			TelemetrySubsystem->RecordCounterEvent(TEXT("MissionBoardBlockedMissionReason_InvalidMission"), SafeCount);
			break;
		case EPortMissionOfferActionBlockReason::None:
		default:
			break;
		}
	}

	TelemetrySubsystem->SetCounterValue(TEXT("MissionBoardLastBlockedMissionReason_BoardDisabled"), DisabledCount);
	TelemetrySubsystem->SetCounterValue(TEXT("MissionBoardLastBlockedMissionReason_Cooldown"), CooldownCount);
	TelemetrySubsystem->SetCounterValue(TEXT("MissionBoardLastBlockedMissionReason_NotOffered"), NotOfferedCount);
	TelemetrySubsystem->SetCounterValue(TEXT("MissionBoardLastBlockedMissionReason_AlreadyActive"), AlreadyActiveCount);
	TelemetrySubsystem->SetCounterValue(TEXT("MissionBoardLastBlockedMissionReason_InvalidMission"), InvalidMissionCount);
}

void RecordUpgradeOfferBlockedReasonSummaryTelemetry(UTelemetrySubsystem* TelemetrySubsystem, const FPortMissionBoardData& BoardData)
{
	if (!TelemetrySubsystem)
	{
		return;
	}

	int32 ServiceDisabledCount = 0;
	int32 NotOfferedCount = 0;
	int32 AlreadyUnlockedCount = 0;
	int32 VisitGateCount = 0;
	int32 NoCreditsCount = 0;
	int32 AvailabilityBlockedCount = 0;
	int32 InvalidUpgradeCount = 0;
	for (const FPortUpgradeOfferBlockReasonSummaryEntry& SummaryEntry : BoardData.UpgradeBlockedReasonSummary)
	{
		const int32 SafeCount = FMath::Max(0, SummaryEntry.Count);
		if (SafeCount <= 0)
		{
			continue;
		}

		switch (SummaryEntry.BlockReason)
		{
		case EPortUpgradeOfferActionBlockReason::UpgradeServiceDisabled:
			ServiceDisabledCount += SafeCount;
			TelemetrySubsystem->RecordCounterEvent(TEXT("MissionBoardBlockedUpgradeReason_ServiceDisabled"), SafeCount);
			break;
		case EPortUpgradeOfferActionBlockReason::NotOfferedAtPort:
			NotOfferedCount += SafeCount;
			TelemetrySubsystem->RecordCounterEvent(TEXT("MissionBoardBlockedUpgradeReason_NotOffered"), SafeCount);
			break;
		case EPortUpgradeOfferActionBlockReason::AlreadyUnlocked:
			AlreadyUnlockedCount += SafeCount;
			TelemetrySubsystem->RecordCounterEvent(TEXT("MissionBoardBlockedUpgradeReason_AlreadyUnlocked"), SafeCount);
			break;
		case EPortUpgradeOfferActionBlockReason::VisitRequirementNotMet:
			VisitGateCount += SafeCount;
			TelemetrySubsystem->RecordCounterEvent(TEXT("MissionBoardBlockedUpgradeReason_VisitGate"), SafeCount);
			break;
		case EPortUpgradeOfferActionBlockReason::InsufficientCredits:
			NoCreditsCount += SafeCount;
			TelemetrySubsystem->RecordCounterEvent(TEXT("MissionBoardBlockedUpgradeReason_NoCredits"), SafeCount);
			break;
		case EPortUpgradeOfferActionBlockReason::UpgradeAvailabilityBlocked:
			AvailabilityBlockedCount += SafeCount;
			TelemetrySubsystem->RecordCounterEvent(TEXT("MissionBoardBlockedUpgradeReason_Availability"), SafeCount);
			break;
		case EPortUpgradeOfferActionBlockReason::InvalidUpgrade:
			InvalidUpgradeCount += SafeCount;
			TelemetrySubsystem->RecordCounterEvent(TEXT("MissionBoardBlockedUpgradeReason_InvalidUpgrade"), SafeCount);
			break;
		case EPortUpgradeOfferActionBlockReason::None:
		default:
			break;
		}
	}

	TelemetrySubsystem->SetCounterValue(TEXT("MissionBoardLastBlockedUpgradeReason_ServiceDisabled"), ServiceDisabledCount);
	TelemetrySubsystem->SetCounterValue(TEXT("MissionBoardLastBlockedUpgradeReason_NotOffered"), NotOfferedCount);
	TelemetrySubsystem->SetCounterValue(TEXT("MissionBoardLastBlockedUpgradeReason_AlreadyUnlocked"), AlreadyUnlockedCount);
	TelemetrySubsystem->SetCounterValue(TEXT("MissionBoardLastBlockedUpgradeReason_VisitGate"), VisitGateCount);
	TelemetrySubsystem->SetCounterValue(TEXT("MissionBoardLastBlockedUpgradeReason_NoCredits"), NoCreditsCount);
	TelemetrySubsystem->SetCounterValue(TEXT("MissionBoardLastBlockedUpgradeReason_Availability"), AvailabilityBlockedCount);
	TelemetrySubsystem->SetCounterValue(TEXT("MissionBoardLastBlockedUpgradeReason_InvalidUpgrade"), InvalidUpgradeCount);
}
}

void ASailingHUD::BeginPlay()
{
	Super::BeginPlay();
	EnsureOverlayWidget();
	EnsurePortMissionBoardWidget();

	// Defer binding to allow islands to spawn
	FTimerHandle TimerHandle;
	GetWorldTimerManager().SetTimer(TimerHandle, this, &ASailingHUD::BindToIslandDiscoveries, 1.0f, false);
}

void ASailingHUD::EnsureOverlayWidget()
{
	if (OverlayWidget || !OverlayWidgetClass || !PlayerOwner)
	{
		return;
	}

	OverlayWidget = CreateWidget<USailingHUDOverlayWidget>(PlayerOwner, OverlayWidgetClass);
	if (OverlayWidget)
	{
		OverlayWidget->AddToViewport(1);
	}
}

void ASailingHUD::EnsurePortMissionBoardWidget()
{
	if (PortMissionBoardWidget || !PortMissionBoardWidgetClass || !PlayerOwner)
	{
		return;
	}

	PortMissionBoardWidget = CreateWidget<UPortMissionBoardWidget>(PlayerOwner, PortMissionBoardWidgetClass);
	if (PortMissionBoardWidget)
	{
		PortMissionBoardWidget->OnAcceptMissionRequested.AddDynamic(this, &ASailingHUD::HandleMissionAcceptedRequest);
		PortMissionBoardWidget->OnCloseRequested.AddDynamic(this, &ASailingHUD::HandleMissionBoardCloseRequest);
		PortMissionBoardWidget->OnRefreshRequested.AddDynamic(this, &ASailingHUD::HandleMissionBoardRefreshRequest);
		PortMissionBoardWidget->OnRepairRequested.AddDynamic(this, &ASailingHUD::HandleRepairRequest);
		PortMissionBoardWidget->OnUpgradePurchaseRequested.AddDynamic(this, &ASailingHUD::HandleUpgradePurchaseRequest);
		PortMissionBoardWidget->OnActionBlocked.AddDynamic(this, &ASailingHUD::HandleMissionBoardActionBlocked);
		PortMissionBoardWidget->AddToViewport(2);
		PortMissionBoardWidget->SetVisibility(ESlateVisibility::Hidden);
	}
}

void ASailingHUD::ShowPortMissionBoard(FName PortId, const FText& PortDisplayName,
	bool bOfferMissionBoard,
	const TArray<FName>& OfferedMissionIds, FName CurrentMissionId,
	bool bMissionBoardOnCooldown, float CooldownRemainingSeconds,
	EPortBoardRefreshContext RefreshContext,
	bool bAllowManualBoardRefresh,
	float ManualBoardRefreshCooldownSeconds,
	int32 ManualBoardRefreshCreditCost,
	const TArray<FPortMissionWeightedOffer>& WeightedOfferedMissionRules,
	bool bUsedWeightedMissionRules,
	bool bUsedFallbackMissionOffers,
	int32 EligibleWeightedMissionRuleCount,
	int32 VisitGatedWeightedMissionRuleCount,
	bool bAutoRepairAtPort, int32 RepairCostPerPercentPoint,
	bool bOfferUpgradeService, const TArray<FName>& OfferedUpgradeIds,
	float UpgradeCostMultiplier, int32 CurrentPortVisitCount,
	const TArray<FPortUpgradeWeightedOffer>& WeightedOfferedUpgradeRules,
	bool bUsedWeightedUpgradeRules,
	bool bUsedFallbackUpgradeOffers,
	int32 EligibleWeightedUpgradeRuleCount,
	int32 VisitGatedWeightedUpgradeRuleCount,
	int32 HiddenUnlockedUpgradeOfferCount)
{
	EnsurePortMissionBoardWidget();
	if (!PortMissionBoardWidget)
	{
		return;
	}

	FPortMissionBoardData Data;
	Data.PortId = PortId;
	Data.PortDisplayName = PortDisplayName;
	Data.bSupportsMissionBoard = bOfferMissionBoard;
	Data.OfferedMissionIds = OfferedMissionIds;
	Data.bHasWeightedMissionRules = WeightedOfferedMissionRules.Num() > 0;
	Data.bHasAnyOffers = OfferedMissionIds.Num() > 0;
	Data.CurrentMissionId = CurrentMissionId;
	Data.bMissionBoardOnCooldown = bMissionBoardOnCooldown;
	Data.CooldownRemainingSeconds = FMath::Max(0.0f, CooldownRemainingSeconds);
	Data.PortVisitCount = FMath::Max(0, CurrentPortVisitCount);
	Data.bSupportsManualRefresh = bAllowManualBoardRefresh;
	Data.ManualRefreshCreditCost = FMath::Max(0, ManualBoardRefreshCreditCost);
	const float CurrentTimeSeconds = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0f;
	if (LastMissionBoardPortId != PortId)
	{
		LastMissionBoardManualRefreshNextAvailableTime = 0.0f;
	}
	Data.bManualRefreshOnCooldown = Data.bSupportsManualRefresh && (CurrentTimeSeconds < LastMissionBoardManualRefreshNextAvailableTime);
	Data.ManualRefreshCooldownRemainingSeconds = Data.bManualRefreshOnCooldown
		? (LastMissionBoardManualRefreshNextAvailableTime - CurrentTimeSeconds)
		: 0.0f;
	Data.bCanAffordManualRefresh = Data.ManualRefreshCreditCost <= 0;
	Data.bAwaitingMissionSwitchConfirmation = false;
	Data.PendingMissionSwitchId = NAME_None;
	Data.MissionSwitchConfirmationStatus = FText::GetEmpty();
	Data.bSupportsUpgradeService = bOfferUpgradeService;
	Data.OfferedUpgradeIds = OfferedUpgradeIds;
	Data.bHasWeightedUpgradeRules = WeightedOfferedUpgradeRules.Num() > 0;
	Data.UpgradeOfferSource = UPortMissionBoardWidget::DetermineUpgradeOfferSource(
		bOfferUpgradeService,
		bUsedWeightedUpgradeRules,
		bUsedFallbackUpgradeOffers);
	Data.EligibleWeightedUpgradeRuleCount = FMath::Max(0, EligibleWeightedUpgradeRuleCount);
	Data.VisitGatedWeightedUpgradeRuleCount = FMath::Max(0, VisitGatedWeightedUpgradeRuleCount);
	Data.HiddenUnlockedUpgradeOfferCount = FMath::Max(0, HiddenUnlockedUpgradeOfferCount);
	Data.UpgradeOfferSourceStatus = UPortMissionBoardWidget::BuildUpgradeOfferSourceStatusText(
		Data.UpgradeOfferSource,
		Data.EligibleWeightedUpgradeRuleCount,
		Data.VisitGatedWeightedUpgradeRuleCount,
		Data.HiddenUnlockedUpgradeOfferCount);
	Data.UpgradeCostMultiplier = FMath::Max(0.1f, UpgradeCostMultiplier);
	Data.UpgradeStatus = bOfferUpgradeService
		? FText::FromString(TEXT("Tilgjengelige oppgraderinger i denne havnen."))
		: FText::FromString(TEXT("Ingen oppgraderingsservice i denne havnen."));
	Data.UpgradePricingStatus = UPortMissionBoardWidget::BuildUpgradePricingStatusText(
		bOfferUpgradeService,
		Data.UpgradeCostMultiplier);
	Data.bAwaitingUpgradePurchaseConfirmation = false;
	Data.PendingUpgradePurchaseId = NAME_None;
	Data.UpgradePurchaseConfirmationStatus = FText::GetEmpty();
	Data.AvailabilityReason = UPortMissionBoardWidget::DetermineMissionAvailabilityReason(
		Data.bSupportsMissionBoard, Data.bMissionBoardOnCooldown, Data.bHasAnyOffers);
	Data.MissionOfferSource = UPortMissionBoardWidget::DetermineMissionOfferSource(
		Data.bSupportsMissionBoard,
		bUsedWeightedMissionRules,
		bUsedFallbackMissionOffers);
	Data.EligibleWeightedMissionRuleCount = FMath::Max(0, EligibleWeightedMissionRuleCount);
	Data.VisitGatedWeightedMissionRuleCount = FMath::Max(0, VisitGatedWeightedMissionRuleCount);
	Data.MissionOfferSourceStatus = UPortMissionBoardWidget::BuildMissionOfferSourceStatusText(
		Data.MissionOfferSource,
		Data.EligibleWeightedMissionRuleCount,
		Data.VisitGatedWeightedMissionRuleCount);
	Data.AvailabilityStatus = UPortMissionBoardWidget::BuildMissionAvailabilityStatusText(
		Data.AvailabilityReason, Data.CooldownRemainingSeconds, Data.bSupportsUpgradeService);
	Data.RefreshContext = RefreshContext;
	Data.RefreshContextStatus = UPortMissionBoardWidget::BuildRefreshContextStatusText(RefreshContext);
	if (UGameInstance* GI = GetWorld() ? GetWorld()->GetGameInstance() : nullptr)
	{
		if (UEconomySubsystem* EconomySubsystem = GI->GetSubsystem<UEconomySubsystem>())
		{
			Data.CurrentBoatConditionPercent = EconomySubsystem->GetBoatConditionPercent();
			const int32 MissingCondition = 100 - FMath::Clamp(Data.CurrentBoatConditionPercent, 0, 100);
			const int32 CostPerPoint = FMath::Max(0, RepairCostPerPercentPoint);
			Data.EstimatedRepairCostCredits = MissingCondition * CostPerPoint;
			Data.bSupportsRepairService = !bAutoRepairAtPort;
			Data.bCanAffordRepair = EconomySubsystem->GetCredits() >= Data.EstimatedRepairCostCredits;
			Data.bCanAffordManualRefresh = EconomySubsystem->GetCredits() >= Data.ManualRefreshCreditCost;
			if (!Data.bSupportsRepairService)
			{
				Data.RepairStatus = FText::FromString(TEXT("Reparasjon utføres automatisk ved anløp."));
			}
			else if (MissingCondition <= 0)
			{
				Data.RepairStatus = FText::FromString(TEXT("Båten er allerede i topp stand."));
			}
			else if (Data.bCanAffordRepair)
			{
				Data.RepairStatus = FText::FromString(FString::Printf(TEXT("Reparasjon tilgjengelig (%d kreditter)."), Data.EstimatedRepairCostCredits));
			}
			else
			{
				Data.RepairStatus = FText::FromString(FString::Printf(TEXT("Mangler kreditter til reparasjon (%d)."), Data.EstimatedRepairCostCredits));
			}

			if (Data.bSupportsUpgradeService)
			{
				int32 AffordableUpgradeCount = 0;
				int32 ValidUpgradeOfferCount = 0;
				const float SafeUpgradeCostMultiplier = FMath::Max(0.1f, UpgradeCostMultiplier);
				for (const FName& UpgradeId : OfferedUpgradeIds)
				{
					if (const UBoatUpgradeDataAsset* UpgradeData = EconomySubsystem->GetUpgradeAssetById(UpgradeId))
					{
						FPortUpgradeOfferEntry OfferEntry;
						OfferEntry.UpgradeId = UpgradeData->UpgradeId;
						OfferEntry.UpgradeTitle = UpgradeData->DisplayName.IsEmpty()
							? FText::FromName(UpgradeData->UpgradeId)
							: UpgradeData->DisplayName;
						OfferEntry.BaseCreditCost = FMath::Max(0, UpgradeData->CreditCost);
						OfferEntry.CreditCost = UPortDataAsset::CalculateAdjustedUpgradeCost(
							UpgradeData->CreditCost, SafeUpgradeCostMultiplier);
						OfferEntry.bUnlocked = EconomySubsystem->IsUpgradeUnlocked(UpgradeData->UpgradeId);
						OfferEntry.bAffordable = EconomySubsystem->GetCredits() >= OfferEntry.CreditCost;
						if (const FPortUpgradeWeightedOffer* WeightedRule = WeightedOfferedUpgradeRules.FindByPredicate(
							[UpgradeId](const FPortUpgradeWeightedOffer& Candidate)
							{
								return Candidate.UpgradeId == UpgradeId;
							}))
						{
							OfferEntry.PriorityWeight = FMath::Max(0.0f, WeightedRule->PriorityWeight);
							OfferEntry.MinPortVisits = FMath::Max(0, WeightedRule->MinPortVisits);
						}
						OfferEntry.bVisitGateSatisfied = Data.PortVisitCount >= OfferEntry.MinPortVisits;
						OfferEntry.VisitRequirementStatus = UPortMissionBoardWidget::BuildUpgradeVisitRequirementStatusText(
							OfferEntry.MinPortVisits,
							Data.PortVisitCount);
						Data.OfferedUpgrades.Add(OfferEntry);
						ValidUpgradeOfferCount++;

						if (!OfferEntry.bUnlocked && OfferEntry.bAffordable)
						{
							AffordableUpgradeCount++;
						}
					}
				}
				Data.UpgradeAvailabilityReason = UPortMissionBoardWidget::DetermineUpgradeAvailabilityReason(
					Data.bSupportsUpgradeService, ValidUpgradeOfferCount, AffordableUpgradeCount);
				Data.UpgradeStatus = UPortMissionBoardWidget::BuildUpgradeAvailabilityStatusText(
					Data.UpgradeAvailabilityReason, AffordableUpgradeCount);
			}
			else
			{
				Data.UpgradeAvailabilityReason = UPortMissionBoardWidget::DetermineUpgradeAvailabilityReason(
					Data.bSupportsUpgradeService, 0, 0);
				Data.UpgradeStatus = UPortMissionBoardWidget::BuildUpgradeAvailabilityStatusText(
					Data.UpgradeAvailabilityReason, 0);
			}
		}

		if (UMissionSubsystem* MissionSubsystem = GI->GetSubsystem<UMissionSubsystem>())
		{
			for (const FName& MissionId : OfferedMissionIds)
			{
				FPortMissionOfferEntry Entry;
				Entry.MissionId = MissionId;
				Entry.MissionTitle = MissionSubsystem->GetMissionDisplayNameById(MissionId);
				if (const FPortMissionWeightedOffer* WeightedRule = WeightedOfferedMissionRules.FindByPredicate(
					[MissionId](const FPortMissionWeightedOffer& Candidate)
					{
						return Candidate.MissionId == MissionId;
					}))
				{
					Entry.PriorityWeight = FMath::Max(0.0f, WeightedRule->PriorityWeight);
					Entry.MinPortVisits = FMath::Max(0, WeightedRule->MinPortVisits);
				}
				Entry.bVisitGateSatisfied = Data.PortVisitCount >= Entry.MinPortVisits;
				Entry.VisitRequirementStatus = UPortMissionBoardWidget::BuildMissionVisitRequirementStatusText(
					Entry.MinPortVisits,
					Data.PortVisitCount);
				Data.OfferedMissions.Add(Entry);
			}

			Data.RecentSelections = MissionSubsystem->GetRecentMissionBoardSelectionsForPort(PortId, 5);
		}
	}
	Data.ManualRefreshStatus = UPortMissionBoardWidget::BuildManualRefreshStatusText(
		Data.bSupportsManualRefresh,
		Data.bManualRefreshOnCooldown,
		Data.ManualRefreshCooldownRemainingSeconds,
		Data.ManualRefreshCreditCost,
		Data.bCanAffordManualRefresh);
	LastMissionBoardData = Data;
	PortMissionBoardWidget->PushMissionBoardData(LastMissionBoardData);
	LastMissionBoardData = PortMissionBoardWidget->GetLastData();
	PortMissionBoardWidget->SetVisibility(ESlateVisibility::Visible);

	if (UGameInstance* GI = GetWorld() ? GetWorld()->GetGameInstance() : nullptr)
	{
		if (UTelemetrySubsystem* TelemetrySubsystem = GI->GetSubsystem<UTelemetrySubsystem>())
		{
			TelemetrySubsystem->RecordCounterEvent(TEXT("MissionBoardShown"), 1);
			TelemetrySubsystem->RecordCounterEvent(TEXT("MissionBoardSelectableMissionOffers"), LastMissionBoardData.SelectableMissionOfferCount);
			TelemetrySubsystem->RecordCounterEvent(TEXT("MissionBoardBlockedMissionOffers"), LastMissionBoardData.BlockedMissionOfferCount);
			TelemetrySubsystem->RecordCounterEvent(TEXT("MissionBoardPurchasableUpgradeOffers"), LastMissionBoardData.PurchasableUpgradeOfferCount);
			TelemetrySubsystem->RecordCounterEvent(TEXT("MissionBoardBlockedUpgradeOffers"), LastMissionBoardData.BlockedUpgradeOfferCount);
			RecordMissionBoardTelemetrySnapshot(TelemetrySubsystem, true);
		}
	}

	LastMissionBoardPortId = PortId;
	LastMissionBoardPortDisplayName = PortDisplayName;
	bLastMissionBoardAllowManualRefresh = bAllowManualBoardRefresh;
	LastMissionBoardManualRefreshCooldownSeconds = FMath::Max(0.0f, ManualBoardRefreshCooldownSeconds);
	LastMissionBoardManualRefreshCreditCost = FMath::Max(0, ManualBoardRefreshCreditCost);
	LastMissionBoardRepairCostPerPercentPoint = FMath::Max(0, RepairCostPerPercentPoint);
	LastMissionBoardUpgradeCostMultiplier = FMath::Max(0.1f, UpgradeCostMultiplier);

	GetWorldTimerManager().ClearTimer(MissionBoardHideTimer);
	GetWorldTimerManager().SetTimer(MissionBoardHideTimer, this, &ASailingHUD::HidePortMissionBoard, 5.0f, false);
}

bool ASailingHUD::AcceptMissionFromBoard(FName MissionId)
{
	if (MissionId.IsNone())
	{
		return false;
	}

	UGameInstance* GI = GetWorld() ? GetWorld()->GetGameInstance() : nullptr;
	if (!GI)
	{
		return false;
	}

	UMissionSubsystem* MissionSubsystem = GI->GetSubsystem<UMissionSubsystem>();
	if (!MissionSubsystem)
	{
		return false;
	}

	const FName CurrentMissionId = MissionSubsystem->GetActiveMissionId();
	LastMissionBoardData.CurrentMissionId = CurrentMissionId;
	if (!CurrentMissionId.IsNone() && CurrentMissionId == MissionId)
	{
		if (UTelemetrySubsystem* TelemetrySubsystem = GI->GetSubsystem<UTelemetrySubsystem>())
		{
			TelemetrySubsystem->RecordCounterEvent(TEXT("MissionBoardMissionAcceptAlreadyActive"), 1);
		}
		ShowDiscoveryPopup(FString::Printf(TEXT("Oppdraget '%s' er allerede aktivt."), *MissionId.ToString()));
		CloseMissionBoard();
		return true;
	}

	FText BlockedReason;
	if (!UPortMissionBoardWidget::CanRequestMissionAccept(LastMissionBoardData, MissionId, BlockedReason))
	{
		if (UTelemetrySubsystem* TelemetrySubsystem = GI->GetSubsystem<UTelemetrySubsystem>())
		{
			TelemetrySubsystem->RecordCounterEvent(TEXT("MissionBoardMissionAcceptBlocked"), 1);
			if (!LastMissionBoardData.bSupportsMissionBoard)
			{
				TelemetrySubsystem->RecordCounterEvent(TEXT("MissionBoardMissionAcceptBlocked_Disabled"), 1);
			}
			else if (LastMissionBoardData.bMissionBoardOnCooldown)
			{
				TelemetrySubsystem->RecordCounterEvent(TEXT("MissionBoardMissionAcceptBlocked_Cooldown"), 1);
			}
			else if (LastMissionBoardData.OfferedMissionIds.Num() > 0 && !LastMissionBoardData.OfferedMissionIds.Contains(MissionId))
			{
				TelemetrySubsystem->RecordCounterEvent(TEXT("MissionBoardMissionAcceptBlocked_NotOffered"), 1);
			}
		}
		if (!BlockedReason.IsEmpty())
		{
			LastMissionBoardData.AvailabilityStatus = BlockedReason;
			if (PortMissionBoardWidget)
			{
				PortMissionBoardWidget->PushMissionBoardData(LastMissionBoardData);
				LastMissionBoardData = PortMissionBoardWidget->GetLastData();
			}
			ShowDiscoveryPopup(BlockedReason.ToString());
		}
		return false;
	}

	const bool bRequiresSwitchConfirmation = UPortMissionBoardWidget::RequiresMissionSwitchConfirmation(
		CurrentMissionId, MissionId, LastMissionBoardData.PendingMissionSwitchId);
	if (bRequiresSwitchConfirmation)
	{
		FText RequestedMissionTitle = FText::FromName(MissionId);
		for (const FPortMissionOfferEntry& OfferEntry : LastMissionBoardData.OfferedMissions)
		{
			if (OfferEntry.MissionId == MissionId && !OfferEntry.MissionTitle.IsEmpty())
			{
				RequestedMissionTitle = OfferEntry.MissionTitle;
				break;
			}
		}

		LastMissionBoardData.bAwaitingMissionSwitchConfirmation = true;
		LastMissionBoardData.PendingMissionSwitchId = MissionId;
		LastMissionBoardData.RefreshContext = EPortBoardRefreshContext::MissionSwitchConfirmation;
		LastMissionBoardData.RefreshContextStatus = UPortMissionBoardWidget::BuildRefreshContextStatusText(
			LastMissionBoardData.RefreshContext);
		LastMissionBoardData.MissionSwitchConfirmationStatus = FText::FromString(
			FString::Printf(TEXT("Bekreft bytte til '%s' ved å trykke oppdraget igjen."), *RequestedMissionTitle.ToString()));
		LastMissionBoardData.AvailabilityStatus = LastMissionBoardData.MissionSwitchConfirmationStatus;
		if (PortMissionBoardWidget)
		{
			PortMissionBoardWidget->PushMissionBoardData(LastMissionBoardData);
			LastMissionBoardData = PortMissionBoardWidget->GetLastData();
		}

		ShowDiscoveryPopup(FString::Printf(TEXT("Bekreft bytte til oppdrag: %s"), *RequestedMissionTitle.ToString()));
		return false;
	}

	LastMissionBoardData.bAwaitingMissionSwitchConfirmation = false;
	LastMissionBoardData.PendingMissionSwitchId = NAME_None;
	LastMissionBoardData.MissionSwitchConfirmationStatus = FText::GetEmpty();

	const bool bActivated = MissionSubsystem->ActivateMissionFromCandidates({ MissionId }, false);
	if (bActivated)
	{
		MissionSubsystem->RecordMissionBoardSelection(LastMissionBoardPortId, MissionId);
		if (UTelemetrySubsystem* TelemetrySubsystem = GI->GetSubsystem<UTelemetrySubsystem>())
		{
			TelemetrySubsystem->RecordCounterEvent(TEXT("MissionBoardSelections"), 1);
		}

		ShowDiscoveryPopup(FString::Printf(TEXT("Oppdrag valgt: %s"), *MissionId.ToString()));
		if (ASailingGameMode* GM = Cast<ASailingGameMode>(UGameplayStatics::GetGameMode(GetWorld())))
		{
			GM->SaveGame_();
		}
		CloseMissionBoard();
	}
	else if (PortMissionBoardWidget)
	{
		PortMissionBoardWidget->PushMissionBoardData(LastMissionBoardData);
		LastMissionBoardData = PortMissionBoardWidget->GetLastData();
	}
	return bActivated;
}

bool ASailingHUD::RequestRepairFromBoard()
{
	UGameInstance* GI = GetWorld() ? GetWorld()->GetGameInstance() : nullptr;

	FText BlockedReason;
	if (!UPortMissionBoardWidget::CanRequestRepairService(LastMissionBoardData, BlockedReason))
	{
		if (GI)
		{
			if (UTelemetrySubsystem* TelemetrySubsystem = GI->GetSubsystem<UTelemetrySubsystem>())
			{
				TelemetrySubsystem->RecordCounterEvent(TEXT("MissionBoardRepairBlocked"), 1);
				if (!LastMissionBoardData.bSupportsRepairService)
				{
					TelemetrySubsystem->RecordCounterEvent(TEXT("MissionBoardRepairBlocked_NoService"), 1);
				}
				else if (FMath::Clamp(LastMissionBoardData.CurrentBoatConditionPercent, 0, 100) >= 100)
				{
					TelemetrySubsystem->RecordCounterEvent(TEXT("MissionBoardRepairBlocked_FullCondition"), 1);
				}
				else if (!LastMissionBoardData.bCanAffordRepair)
				{
					TelemetrySubsystem->RecordCounterEvent(TEXT("MissionBoardRepairBlocked_NoCredits"), 1);
				}
			}
		}
		if (!BlockedReason.IsEmpty())
		{
			ShowDiscoveryPopup(BlockedReason.ToString());
		}
		return FMath::Clamp(LastMissionBoardData.CurrentBoatConditionPercent, 0, 100) >= 100;
	}

	if (!GI)
	{
		return false;
	}

	UEconomySubsystem* EconomySubsystem = GI->GetSubsystem<UEconomySubsystem>();
	if (!EconomySubsystem)
	{
		return false;
	}

	const int32 PreviousCondition = EconomySubsystem->GetBoatConditionPercent();
	const int32 MissingCondition = 100 - FMath::Clamp(PreviousCondition, 0, 100);
	if (MissingCondition <= 0)
	{
		ShowDiscoveryPopup(TEXT("Båten er allerede fullstendig reparert."));
		return true;
	}

	const int32 CostPerPoint = FMath::Max(0, LastMissionBoardRepairCostPerPercentPoint);
	const int32 EstimatedCost = MissingCondition * CostPerPoint;
	const bool bRepaired = EconomySubsystem->RepairBoatToFull(CostPerPoint);
	if (!bRepaired)
	{
		ShowDiscoveryPopup(FString::Printf(TEXT("Ikke nok kreditter til reparasjon (%d)."), EstimatedCost));
		return false;
	}

	ShowDiscoveryPopup(FString::Printf(TEXT("Båten reparert for %d kreditter."), EstimatedCost));
	if (UTelemetrySubsystem* TelemetrySubsystem = GI->GetSubsystem<UTelemetrySubsystem>())
	{
		TelemetrySubsystem->RecordCounterEvent(TEXT("PortRepairs"), 1);
	}

	if (ASailingGameMode* GM = Cast<ASailingGameMode>(UGameplayStatics::GetGameMode(GetWorld())))
	{
		GM->SaveGame_();
	}

	CloseMissionBoard();
	return true;
}

bool ASailingHUD::RequestUpgradePurchaseFromBoard(FName UpgradeId)
{
	UGameInstance* GI = GetWorld() ? GetWorld()->GetGameInstance() : nullptr;

	FText BlockedReason;
	if (!UPortMissionBoardWidget::CanRequestUpgradePurchase(LastMissionBoardData, UpgradeId, BlockedReason))
	{
		if (GI)
		{
			if (UTelemetrySubsystem* TelemetrySubsystem = GI->GetSubsystem<UTelemetrySubsystem>())
			{
				TelemetrySubsystem->RecordCounterEvent(TEXT("MissionBoardUpgradePurchaseBlocked"), 1);
				if (!LastMissionBoardData.bSupportsUpgradeService)
				{
					TelemetrySubsystem->RecordCounterEvent(TEXT("MissionBoardUpgradePurchaseBlocked_ServiceDisabled"), 1);
				}
				else if (LastMissionBoardData.OfferedUpgradeIds.Num() > 0 && !LastMissionBoardData.OfferedUpgradeIds.Contains(UpgradeId))
				{
					TelemetrySubsystem->RecordCounterEvent(TEXT("MissionBoardUpgradePurchaseBlocked_NotOffered"), 1);
				}
				else if (const FPortUpgradeOfferEntry* OfferEntry = LastMissionBoardData.OfferedUpgrades.FindByPredicate(
					[UpgradeId](const FPortUpgradeOfferEntry& Entry)
					{
						return Entry.UpgradeId == UpgradeId;
					}))
				{
					if (OfferEntry->bUnlocked)
					{
						TelemetrySubsystem->RecordCounterEvent(TEXT("MissionBoardUpgradePurchaseBlocked_Unlocked"), 1);
					}
					else if (!OfferEntry->bVisitGateSatisfied)
					{
						TelemetrySubsystem->RecordCounterEvent(TEXT("MissionBoardUpgradePurchaseBlocked_VisitGate"), 1);
					}
					else if (!OfferEntry->bAffordable)
					{
						TelemetrySubsystem->RecordCounterEvent(TEXT("MissionBoardUpgradePurchaseBlocked_NoCredits"), 1);
					}
				}
			}
		}
		if (!BlockedReason.IsEmpty())
		{
			ShowDiscoveryPopup(BlockedReason.ToString());
		}
		return false;
	}

	if (!GI)
	{
		return false;
	}

	UEconomySubsystem* EconomySubsystem = GI->GetSubsystem<UEconomySubsystem>();
	if (!EconomySubsystem)
	{
		return false;
	}

	const UBoatUpgradeDataAsset* UpgradeData = EconomySubsystem->GetUpgradeAssetById(UpgradeId);
	if (!UpgradeData)
	{
		ShowDiscoveryPopup(FString::Printf(TEXT("Oppgradering '%s' finnes ikke i registeret."), *UpgradeId.ToString()));
		return false;
	}

	if (EconomySubsystem->IsUpgradeUnlocked(UpgradeData->UpgradeId))
	{
		ShowDiscoveryPopup(FString::Printf(TEXT("Oppgradering allerede låst opp: %s"), *UpgradeId.ToString()));
		return true;
	}

	const bool bRequiresUpgradeConfirmation = UPortMissionBoardWidget::RequiresUpgradePurchaseConfirmation(
		UpgradeId,
		LastMissionBoardData.PendingUpgradePurchaseId);
	if (bRequiresUpgradeConfirmation)
	{
		FText UpgradeTitle = UpgradeData->DisplayName.IsEmpty()
			? FText::FromName(UpgradeId)
			: UpgradeData->DisplayName;
		LastMissionBoardData.bAwaitingUpgradePurchaseConfirmation = true;
		LastMissionBoardData.PendingUpgradePurchaseId = UpgradeId;
		LastMissionBoardData.RefreshContext = EPortBoardRefreshContext::UpgradePurchaseConfirmation;
		LastMissionBoardData.RefreshContextStatus = UPortMissionBoardWidget::BuildRefreshContextStatusText(
			LastMissionBoardData.RefreshContext);
		LastMissionBoardData.UpgradePurchaseConfirmationStatus = FText::FromString(
			FString::Printf(TEXT("Bekreft kjøp av '%s' ved å trykke oppgraderingen igjen."), *UpgradeTitle.ToString()));
		LastMissionBoardData.UpgradeStatus = LastMissionBoardData.UpgradePurchaseConfirmationStatus;
		if (PortMissionBoardWidget)
		{
			PortMissionBoardWidget->PushMissionBoardData(LastMissionBoardData);
			LastMissionBoardData = PortMissionBoardWidget->GetLastData();
		}

		ShowDiscoveryPopup(FString::Printf(TEXT("Bekreft kjøp av oppgradering: %s"), *UpgradeTitle.ToString()));
		return false;
	}

	LastMissionBoardData.bAwaitingUpgradePurchaseConfirmation = false;
	LastMissionBoardData.PendingUpgradePurchaseId = NAME_None;
	LastMissionBoardData.UpgradePurchaseConfirmationStatus = FText::GetEmpty();

	const int32 AdjustedCost = UPortDataAsset::CalculateAdjustedUpgradeCost(
		UpgradeData->CreditCost,
		LastMissionBoardUpgradeCostMultiplier);

	const bool bPurchased = EconomySubsystem->PurchaseUpgradeById(UpgradeId, AdjustedCost);
	if (!bPurchased)
	{
		ShowDiscoveryPopup(FString::Printf(TEXT("Ikke nok kreditter til %s (%d)."), *UpgradeId.ToString(), AdjustedCost));
		return false;
	}

	ShowDiscoveryPopup(FString::Printf(TEXT("Kjøpt oppgradering: %s (%d kreditter)"), *UpgradeId.ToString(), AdjustedCost));
	if (UTelemetrySubsystem* TelemetrySubsystem = GI->GetSubsystem<UTelemetrySubsystem>())
	{
		TelemetrySubsystem->RecordCounterEvent(TEXT("PortUpgradesPurchased"), 1);
		TelemetrySubsystem->RecordCounterEvent(TEXT("PortUpgradeCreditsSpent"), AdjustedCost);
		TelemetrySubsystem->RecordCounterEvent(
			FName(*FString::Printf(TEXT("PortUpgradePurchased_%s"), *UpgradeId.ToString())),
			1);
	}

	if (ASailingGameMode* GM = Cast<ASailingGameMode>(UGameplayStatics::GetGameMode(GetWorld())))
	{
		GM->SaveGame_();
	}

	CloseMissionBoard();
	return true;
}

void ASailingHUD::RefreshCurrentMissionBoard()
{
	if (!PortMissionBoardWidget || PortMissionBoardWidget->GetVisibility() == ESlateVisibility::Hidden)
	{
		return;
	}

	if (LastMissionBoardData.PortId.IsNone())
	{
		return;
	}

	UGameInstance* GI = GetWorld() ? GetWorld()->GetGameInstance() : nullptr;
	FText RefreshBlockedReason;
	if (!UPortMissionBoardWidget::CanRequestManualRefresh(LastMissionBoardData, RefreshBlockedReason))
	{
		if (GI)
		{
			if (UTelemetrySubsystem* TelemetrySubsystem = GI->GetSubsystem<UTelemetrySubsystem>())
			{
				TelemetrySubsystem->RecordCounterEvent(TEXT("MissionBoardManualRefreshBlocked"), 1);
				if (!LastMissionBoardData.bSupportsManualRefresh)
				{
					TelemetrySubsystem->RecordCounterEvent(TEXT("MissionBoardManualRefreshBlocked_Disabled"), 1);
				}
				else if (LastMissionBoardData.bManualRefreshOnCooldown)
				{
					TelemetrySubsystem->RecordCounterEvent(TEXT("MissionBoardManualRefreshBlocked_Cooldown"), 1);
				}
				else if (!LastMissionBoardData.bCanAffordManualRefresh)
				{
					TelemetrySubsystem->RecordCounterEvent(TEXT("MissionBoardManualRefreshBlocked_NoCredits"), 1);
				}
			}
		}
		LastMissionBoardData.ManualRefreshStatus = RefreshBlockedReason;
		if (LastMissionBoardData.bManualRefreshOnCooldown)
		{
			LastMissionBoardData.RefreshContext = EPortBoardRefreshContext::CooldownBlocked;
			LastMissionBoardData.RefreshContextStatus = UPortMissionBoardWidget::BuildRefreshContextStatusText(
				LastMissionBoardData.RefreshContext);
		}
		PortMissionBoardWidget->PushMissionBoardData(LastMissionBoardData);
		LastMissionBoardData = PortMissionBoardWidget->GetLastData();
		if (!RefreshBlockedReason.IsEmpty())
		{
			ShowDiscoveryPopup(RefreshBlockedReason.ToString());
		}
		return;
	}

	const float CurrentTimeSeconds = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0f;
	UEconomySubsystem* EconomySubsystem = GI ? GI->GetSubsystem<UEconomySubsystem>() : nullptr;
	const int32 RefreshCost = FMath::Max(0, LastMissionBoardManualRefreshCreditCost);
	if (RefreshCost > 0 && !EconomySubsystem)
	{
		ShowDiscoveryPopup(TEXT("Kunne ikke belaste kreditter for oppfriskning."));
		return;
	}
	if (RefreshCost > 0 && EconomySubsystem)
	{
		if (!EconomySubsystem->SpendCredits(RefreshCost))
		{
			LastMissionBoardData.bCanAffordManualRefresh = false;
			LastMissionBoardData.ManualRefreshStatus = UPortMissionBoardWidget::BuildManualRefreshStatusText(
				true,
				false,
				0.0f,
				RefreshCost,
				false);
			PortMissionBoardWidget->PushMissionBoardData(LastMissionBoardData);
			LastMissionBoardData = PortMissionBoardWidget->GetLastData();
			ShowDiscoveryPopup(FString::Printf(TEXT("Ikke nok kreditter til oppfriskning (%d)."), RefreshCost));
			return;
		}
	}

	if (LastMissionBoardManualRefreshCooldownSeconds > 0.0f)
	{
		LastMissionBoardManualRefreshNextAvailableTime = CurrentTimeSeconds + LastMissionBoardManualRefreshCooldownSeconds;
	}

	if (EconomySubsystem)
	{
		LastMissionBoardData.CurrentBoatConditionPercent = EconomySubsystem->GetBoatConditionPercent();
		const int32 MissingCondition = 100 - FMath::Clamp(LastMissionBoardData.CurrentBoatConditionPercent, 0, 100);
		LastMissionBoardData.EstimatedRepairCostCredits = MissingCondition * FMath::Max(0, LastMissionBoardRepairCostPerPercentPoint);
		LastMissionBoardData.bCanAffordRepair = EconomySubsystem->GetCredits() >= LastMissionBoardData.EstimatedRepairCostCredits;
		LastMissionBoardData.bCanAffordManualRefresh = EconomySubsystem->GetCredits() >= RefreshCost;

		int32 AffordableUpgradeCount = 0;
		int32 ValidUpgradeOfferCount = 0;
		for (FPortUpgradeOfferEntry& UpgradeEntry : LastMissionBoardData.OfferedUpgrades)
		{
			UpgradeEntry.bUnlocked = EconomySubsystem->IsUpgradeUnlocked(UpgradeEntry.UpgradeId);
			UpgradeEntry.bAffordable = EconomySubsystem->GetCredits() >= UpgradeEntry.CreditCost;
			ValidUpgradeOfferCount++;
			if (!UpgradeEntry.bUnlocked && UpgradeEntry.bAffordable)
			{
				AffordableUpgradeCount++;
			}
		}
		LastMissionBoardData.UpgradeAvailabilityReason = UPortMissionBoardWidget::DetermineUpgradeAvailabilityReason(
			LastMissionBoardData.bSupportsUpgradeService,
			ValidUpgradeOfferCount,
			AffordableUpgradeCount);
		LastMissionBoardData.UpgradeStatus = UPortMissionBoardWidget::BuildUpgradeAvailabilityStatusText(
			LastMissionBoardData.UpgradeAvailabilityReason,
			AffordableUpgradeCount);
	}

	if (GI)
	{
		if (UTelemetrySubsystem* TelemetrySubsystem = GI->GetSubsystem<UTelemetrySubsystem>())
		{
			TelemetrySubsystem->RecordCounterEvent(TEXT("MissionBoardManualRefreshes"), 1);
			if (RefreshCost > 0)
			{
				TelemetrySubsystem->RecordCounterEvent(TEXT("MissionBoardManualRefreshCreditsSpent"), RefreshCost);
			}
		}
		if (RefreshCost > 0)
		{
			if (ASailingGameMode* GM = Cast<ASailingGameMode>(UGameplayStatics::GetGameMode(GetWorld())))
			{
				GM->SaveGame_();
			}
		}
	}

	LastMissionBoardData.bManualRefreshOnCooldown = LastMissionBoardManualRefreshCooldownSeconds > 0.0f;
	LastMissionBoardData.ManualRefreshCooldownRemainingSeconds = LastMissionBoardData.bManualRefreshOnCooldown
		? LastMissionBoardManualRefreshCooldownSeconds
		: 0.0f;
	LastMissionBoardData.ManualRefreshCreditCost = RefreshCost;
	LastMissionBoardData.ManualRefreshStatus = UPortMissionBoardWidget::BuildManualRefreshStatusText(
		true,
		LastMissionBoardData.bManualRefreshOnCooldown,
		LastMissionBoardData.ManualRefreshCooldownRemainingSeconds,
		RefreshCost,
		LastMissionBoardData.bCanAffordManualRefresh);

	LastMissionBoardData.RefreshContext = EPortBoardRefreshContext::ManualRefresh;
	LastMissionBoardData.RefreshContextStatus = UPortMissionBoardWidget::BuildRefreshContextStatusText(
		LastMissionBoardData.RefreshContext);
	PortMissionBoardWidget->PushMissionBoardData(LastMissionBoardData);
	LastMissionBoardData = PortMissionBoardWidget->GetLastData();

	if (GI)
	{
		if (UTelemetrySubsystem* TelemetrySubsystem = GI->GetSubsystem<UTelemetrySubsystem>())
		{
			RecordMissionBoardTelemetrySnapshot(TelemetrySubsystem, false);
		}
	}

	GetWorldTimerManager().ClearTimer(MissionBoardHideTimer);
	GetWorldTimerManager().SetTimer(MissionBoardHideTimer, this, &ASailingHUD::HidePortMissionBoard, 5.0f, false);
}

void ASailingHUD::CloseMissionBoard()
{
	HidePortMissionBoard();
	LastMissionBoardPortId = NAME_None;
	LastMissionBoardPortDisplayName = FText::GetEmpty();
	bLastMissionBoardAllowManualRefresh = true;
	LastMissionBoardManualRefreshCooldownSeconds = 0.0f;
	LastMissionBoardManualRefreshCreditCost = 0;
	LastMissionBoardManualRefreshNextAvailableTime = 0.0f;
	LastMissionBoardRepairCostPerPercentPoint = 1;
	LastMissionBoardUpgradeCostMultiplier = 1.0f;
	LastMissionBoardData = FPortMissionBoardData();
}

void ASailingHUD::HidePortMissionBoard()
{
	if (PortMissionBoardWidget)
	{
		PortMissionBoardWidget->SetVisibility(ESlateVisibility::Hidden);
	}
}

void ASailingHUD::HandleMissionAcceptedRequest(FName MissionId)
{
	AcceptMissionFromBoard(MissionId);
}

void ASailingHUD::HandleMissionBoardCloseRequest()
{
	CloseMissionBoard();
}

void ASailingHUD::HandleMissionBoardRefreshRequest()
{
	RefreshCurrentMissionBoard();
}

void ASailingHUD::HandleRepairRequest()
{
	RequestRepairFromBoard();
}

void ASailingHUD::HandleUpgradePurchaseRequest(FName UpgradeId)
{
	RequestUpgradePurchaseFromBoard(UpgradeId);
}

void ASailingHUD::HandleMissionBoardActionBlocked(EPortBoardActionType ActionType, EPortBoardActionBlockedReason BlockedReasonType, const FText& Reason)
{
	UGameInstance* GI = GetWorld() ? GetWorld()->GetGameInstance() : nullptr;
	if (GI)
	{
		if (UTelemetrySubsystem* TelemetrySubsystem = GI->GetSubsystem<UTelemetrySubsystem>())
		{
			TelemetrySubsystem->RecordCounterEvent(TEXT("MissionBoardWidgetActionBlocked"), 1);
			switch (ActionType)
			{
			case EPortBoardActionType::MissionAccept:
				TelemetrySubsystem->RecordCounterEvent(TEXT("MissionBoardWidgetActionBlocked_MissionAccept"), 1);
				break;
			case EPortBoardActionType::ManualRefresh:
				TelemetrySubsystem->RecordCounterEvent(TEXT("MissionBoardWidgetActionBlocked_ManualRefresh"), 1);
				break;
			case EPortBoardActionType::RepairService:
				TelemetrySubsystem->RecordCounterEvent(TEXT("MissionBoardWidgetActionBlocked_RepairService"), 1);
				break;
			case EPortBoardActionType::UpgradePurchase:
				TelemetrySubsystem->RecordCounterEvent(TEXT("MissionBoardWidgetActionBlocked_UpgradePurchase"), 1);
				break;
			default:
				break;
			}
			switch (BlockedReasonType)
			{
			case EPortBoardActionBlockedReason::MissionInvalid:
				TelemetrySubsystem->RecordCounterEvent(TEXT("MissionBoardWidgetBlockedReason_MissionInvalid"), 1);
				break;
			case EPortBoardActionBlockedReason::MissionBoardDisabled:
				TelemetrySubsystem->RecordCounterEvent(TEXT("MissionBoardWidgetBlockedReason_MissionBoardDisabled"), 1);
				break;
			case EPortBoardActionBlockedReason::MissionBoardCooldown:
				TelemetrySubsystem->RecordCounterEvent(TEXT("MissionBoardWidgetBlockedReason_MissionBoardCooldown"), 1);
				break;
			case EPortBoardActionBlockedReason::MissionNotOffered:
				TelemetrySubsystem->RecordCounterEvent(TEXT("MissionBoardWidgetBlockedReason_MissionNotOffered"), 1);
				break;
			case EPortBoardActionBlockedReason::MissionAlreadyActive:
				TelemetrySubsystem->RecordCounterEvent(TEXT("MissionBoardWidgetBlockedReason_MissionAlreadyActive"), 1);
				break;
			case EPortBoardActionBlockedReason::ManualRefreshDisabled:
				TelemetrySubsystem->RecordCounterEvent(TEXT("MissionBoardWidgetBlockedReason_ManualRefreshDisabled"), 1);
				break;
			case EPortBoardActionBlockedReason::ManualRefreshCooldown:
				TelemetrySubsystem->RecordCounterEvent(TEXT("MissionBoardWidgetBlockedReason_ManualRefreshCooldown"), 1);
				break;
			case EPortBoardActionBlockedReason::ManualRefreshNoCredits:
				TelemetrySubsystem->RecordCounterEvent(TEXT("MissionBoardWidgetBlockedReason_ManualRefreshNoCredits"), 1);
				break;
			case EPortBoardActionBlockedReason::RepairServiceDisabled:
				TelemetrySubsystem->RecordCounterEvent(TEXT("MissionBoardWidgetBlockedReason_RepairServiceDisabled"), 1);
				break;
			case EPortBoardActionBlockedReason::RepairAlreadyFullCondition:
				TelemetrySubsystem->RecordCounterEvent(TEXT("MissionBoardWidgetBlockedReason_RepairAlreadyFull"), 1);
				break;
			case EPortBoardActionBlockedReason::RepairNoCredits:
				TelemetrySubsystem->RecordCounterEvent(TEXT("MissionBoardWidgetBlockedReason_RepairNoCredits"), 1);
				break;
			case EPortBoardActionBlockedReason::UpgradeInvalid:
				TelemetrySubsystem->RecordCounterEvent(TEXT("MissionBoardWidgetBlockedReason_UpgradeInvalid"), 1);
				break;
			case EPortBoardActionBlockedReason::UpgradeServiceDisabled:
				TelemetrySubsystem->RecordCounterEvent(TEXT("MissionBoardWidgetBlockedReason_UpgradeServiceDisabled"), 1);
				break;
			case EPortBoardActionBlockedReason::UpgradeNotOffered:
				TelemetrySubsystem->RecordCounterEvent(TEXT("MissionBoardWidgetBlockedReason_UpgradeNotOffered"), 1);
				break;
			case EPortBoardActionBlockedReason::UpgradeAlreadyUnlocked:
				TelemetrySubsystem->RecordCounterEvent(TEXT("MissionBoardWidgetBlockedReason_UpgradeAlreadyUnlocked"), 1);
				break;
			case EPortBoardActionBlockedReason::UpgradeVisitRequirementNotMet:
				TelemetrySubsystem->RecordCounterEvent(TEXT("MissionBoardWidgetBlockedReason_UpgradeVisitGate"), 1);
				break;
			case EPortBoardActionBlockedReason::UpgradeNoCredits:
				TelemetrySubsystem->RecordCounterEvent(TEXT("MissionBoardWidgetBlockedReason_UpgradeNoCredits"), 1);
				break;
			case EPortBoardActionBlockedReason::UpgradeAvailabilityBlocked:
				TelemetrySubsystem->RecordCounterEvent(TEXT("MissionBoardWidgetBlockedReason_UpgradeAvailabilityBlocked"), 1);
				break;
			case EPortBoardActionBlockedReason::None:
			default:
				break;
			}
		}
	}

	if (!Reason.IsEmpty())
	{
		ShowDiscoveryPopup(Reason.ToString());
	}
}

void ASailingHUD::RecordMissionBoardTelemetrySnapshot(UTelemetrySubsystem* TelemetrySubsystem, bool bRecordPrimaryHintDistribution) const
{
	if (!TelemetrySubsystem)
	{
		return;
	}

	TelemetrySubsystem->SetCounterValue(TEXT("MissionBoardLastSelectableMissionOffers"), LastMissionBoardData.SelectableMissionOfferCount);
	TelemetrySubsystem->SetCounterValue(TEXT("MissionBoardLastBlockedMissionOffers"), LastMissionBoardData.BlockedMissionOfferCount);
	TelemetrySubsystem->SetCounterValue(TEXT("MissionBoardLastPurchasableUpgradeOffers"), LastMissionBoardData.PurchasableUpgradeOfferCount);
	TelemetrySubsystem->SetCounterValue(TEXT("MissionBoardLastBlockedUpgradeOffers"), LastMissionBoardData.BlockedUpgradeOfferCount);
	TelemetrySubsystem->SetCounterValue(TEXT("MissionBoardLastHasSelectableMissions"), LastMissionBoardData.bHasSelectableMissionOffers ? 1 : 0);
	TelemetrySubsystem->SetCounterValue(TEXT("MissionBoardLastHasPurchasableUpgrades"), LastMissionBoardData.bHasPurchasableUpgradeOffers ? 1 : 0);
	TelemetrySubsystem->SetCounterValue(TEXT("MissionBoardLastHasImmediateActions"), LastMissionBoardData.bHasAnyImmediateActions ? 1 : 0);
	TelemetrySubsystem->SetCounterValue(TEXT("MissionBoardLastPrimaryActionHint"), static_cast<int32>(LastMissionBoardData.PrimaryActionHint));
	TelemetrySubsystem->SetCounterValue(TEXT("MissionBoardLastRepairActionBlockReason"), static_cast<int32>(LastMissionBoardData.RepairActionBlockReasonType));
	TelemetrySubsystem->SetCounterValue(TEXT("MissionBoardLastManualRefreshActionBlockReason"), static_cast<int32>(LastMissionBoardData.ManualRefreshActionBlockReasonType));

	if (bRecordPrimaryHintDistribution)
	{
		switch (LastMissionBoardData.PrimaryActionHint)
		{
		case EPortBoardPrimaryActionHint::SelectMission:
			TelemetrySubsystem->RecordCounterEvent(TEXT("MissionBoardPrimaryHint_SelectMission"), 1);
			break;
		case EPortBoardPrimaryActionHint::BuyUpgrade:
			TelemetrySubsystem->RecordCounterEvent(TEXT("MissionBoardPrimaryHint_BuyUpgrade"), 1);
			break;
		case EPortBoardPrimaryActionHint::SelectMissionOrBuyUpgrade:
			TelemetrySubsystem->RecordCounterEvent(TEXT("MissionBoardPrimaryHint_Mixed"), 1);
			break;
		case EPortBoardPrimaryActionHint::None:
		default:
			TelemetrySubsystem->RecordCounterEvent(TEXT("MissionBoardPrimaryHint_None"), 1);
			break;
		}

		switch (LastMissionBoardData.RepairActionBlockReasonType)
		{
		case EPortRepairActionBlockReason::None:
			TelemetrySubsystem->RecordCounterEvent(TEXT("MissionBoardRepairAction_Ready"), 1);
			break;
		case EPortRepairActionBlockReason::RepairServiceDisabled:
			TelemetrySubsystem->RecordCounterEvent(TEXT("MissionBoardRepairAction_Blocked_NoService"), 1);
			break;
		case EPortRepairActionBlockReason::AlreadyAtFullCondition:
			TelemetrySubsystem->RecordCounterEvent(TEXT("MissionBoardRepairAction_Blocked_FullCondition"), 1);
			break;
		case EPortRepairActionBlockReason::InsufficientCredits:
			TelemetrySubsystem->RecordCounterEvent(TEXT("MissionBoardRepairAction_Blocked_NoCredits"), 1);
			break;
		default:
			break;
		}

		switch (LastMissionBoardData.ManualRefreshActionBlockReasonType)
		{
		case EPortManualRefreshActionBlockReason::None:
			TelemetrySubsystem->RecordCounterEvent(TEXT("MissionBoardManualRefreshAction_Ready"), 1);
			break;
		case EPortManualRefreshActionBlockReason::ManualRefreshDisabled:
			TelemetrySubsystem->RecordCounterEvent(TEXT("MissionBoardManualRefreshAction_Blocked_Disabled"), 1);
			break;
		case EPortManualRefreshActionBlockReason::ManualRefreshCooldown:
			TelemetrySubsystem->RecordCounterEvent(TEXT("MissionBoardManualRefreshAction_Blocked_Cooldown"), 1);
			break;
		case EPortManualRefreshActionBlockReason::InsufficientCredits:
			TelemetrySubsystem->RecordCounterEvent(TEXT("MissionBoardManualRefreshAction_Blocked_NoCredits"), 1);
			break;
		default:
			break;
		}
	}

	RecordMissionOfferBlockedReasonSummaryTelemetry(TelemetrySubsystem, LastMissionBoardData);
	RecordUpgradeOfferBlockedReasonSummaryTelemetry(TelemetrySubsystem, LastMissionBoardData);
}

void ASailingHUD::PushOverlayData(int32 DiscoveredIslands, int32 Credits, FName ActiveMissionId,
	const FText& ActiveMissionTitle, float ObjectiveDistanceMeters, int32 BoatConditionPercent,
	float ObjectiveBearingDegrees, FName LastVisitedPortId)
{
	EnsureOverlayWidget();
	if (!OverlayWidget)
	{
		return;
	}

	FSailingHUDOverlayData OverlayData;
	OverlayData.DiscoveredIslands = DiscoveredIslands;
	OverlayData.Credits = Credits;
	OverlayData.ActiveMissionId = ActiveMissionId;
	OverlayData.ActiveMissionTitle = ActiveMissionTitle;
	OverlayData.ObjectiveDistanceMeters = ObjectiveDistanceMeters;
	OverlayData.BoatConditionPercent = BoatConditionPercent;
	OverlayData.ObjectiveBearingDegrees = ObjectiveBearingDegrees;
	OverlayData.LastVisitedPortId = LastVisitedPortId;
	OverlayWidget->PushOverlayData(OverlayData);
}

void ASailingHUD::BindToIslandDiscoveries()
{
	TArray<AActor*> Islands;
	UGameplayStatics::GetAllActorsOfClass(GetWorld(), AIslandActor::StaticClass(), Islands);

	for (AActor* Actor : Islands)
	{
		if (AIslandActor* Island = Cast<AIslandActor>(Actor))
		{
			Island->OnDiscovered.AddDynamic(this, &ASailingHUD::OnIslandDiscovered);
		}
	}

	FTimerHandle TimerHandle;
	GetWorldTimerManager().SetTimer(TimerHandle, this, &ASailingHUD::BindToIslandDiscoveries, 2.0f, false);
}

void ASailingHUD::OnIslandDiscovered(AIslandActor* Island, const FString& IslandName)
{
	ShowDiscoveryPopup(IslandName);
}

void ASailingHUD::ShowDiscoveryPopup(const FString& IslandName)
{
	CurrentDiscoveryName = IslandName;
	DiscoveryPopupTimer = DiscoveryPopupDuration;
	bShowingDiscoveryPopup = true;
}

void ASailingHUD::DrawHUD()
{
	Super::DrawHUD();

	if (!Canvas) return;

	ASailingPlayerController* PC = Cast<ASailingPlayerController>(PlayerOwner);
	if (PC && PC->IsPauseMenuShown())
	{
		DrawPauseMenu();
		return;
	}

	DrawCompass();
	DrawWindAndSpeed();
	DrawOverviewMap();
	DrawDiscoveryCounter();

	float DeltaTime = GetWorld()->GetDeltaSeconds();
	if (bShowingDiscoveryPopup)
	{
		DrawDiscoveryPopup(DeltaTime);
	}
}

void ASailingHUD::DrawCompass()
{
	APawn* Pawn = GetOwningPawn();
	if (!Pawn) return;

	AWindActor* Wind = nullptr;
	TArray<AActor*> Found;
	UGameplayStatics::GetAllActorsOfClass(GetWorld(), AWindActor::StaticClass(), Found);
	if (Found.Num() > 0)
	{
		Wind = Cast<AWindActor>(Found[0]);
	}

	// Compass position and size - BIG and visible
	float CenterX = Canvas->ClipX * 0.5f;
	float CenterY = 90.0f;
	float Radius = 65.0f;

	// Dark background circle
	DrawRect(FLinearColor(0.0f, 0.05f, 0.15f, 0.7f),
		CenterX - Radius - 10, CenterY - Radius - 10,
		(Radius + 10) * 2, (Radius + 10) * 2);

	// Compass ring - thick white circle
	const int32 Segments = 48;
	for (int32 i = 0; i < Segments; ++i)
	{
		float Angle1 = (float)i / Segments * 2.0f * PI;
		float Angle2 = (float)(i + 1) / Segments * 2.0f * PI;
		// Draw 3 lines close together for thickness
		for (float Offset = -1.0f; Offset <= 1.0f; Offset += 1.0f)
		{
			DrawLine(
				CenterX + FMath::Cos(Angle1) * (Radius + Offset),
				CenterY + FMath::Sin(Angle1) * (Radius + Offset),
				CenterX + FMath::Cos(Angle2) * (Radius + Offset),
				CenterY + FMath::Sin(Angle2) * (Radius + Offset),
				FLinearColor(0.7f, 0.8f, 0.9f, 0.9f),
				1.0f);
		}
	}

	// N/S/E/W labels - relative to boat heading
	// Offset by -PI/2 so that "ahead" maps to screen-up (matching the forward triangle)
	float BoatYaw = FMath::DegreesToRadians(Pawn->GetActorRotation().Yaw);

	struct FCardinal { const TCHAR* Label; float WorldAngle; FLinearColor Color; };
	FCardinal Cardinals[] = {
		{ TEXT("N"), 0.0f, FLinearColor::Red },
		{ TEXT("E"), PI * 0.5f, FLinearColor::White },
		{ TEXT("S"), PI, FLinearColor::White },
		{ TEXT("W"), PI * 1.5f, FLinearColor::White }
	};

	for (const auto& C : Cardinals)
	{
		float Angle = C.WorldAngle - BoatYaw - PI * 0.5f;
		float LabelX = CenterX + FMath::Cos(Angle) * (Radius + 18.0f) - 6.0f;
		float LabelY = CenterY + FMath::Sin(Angle) * (Radius + 18.0f) - 8.0f;
		DrawText(C.Label, C.Color, LabelX, LabelY, nullptr, 1.5f);
	}

	// Boat forward indicator - white triangle at top of compass
	{
		float FwdAngle = -PI * 0.5f;
		float FwdX = CenterX + FMath::Cos(FwdAngle) * (Radius + 2.0f);
		float FwdY = CenterY + FMath::Sin(FwdAngle) * (Radius + 2.0f);
		float TriSize = 8.0f;
		// Draw filled triangle pointing inward
		for (float f = 0.0f; f <= 1.0f; f += 0.1f)
		{
			float HalfW = TriSize * (1.0f - f);
			float Y = FwdY - TriSize + f * TriSize * 2.0f;
			DrawRect(FLinearColor::White, FwdX - HalfW, Y, HalfW * 2.0f, 1.5f);
		}
	}

	// Wind direction arrow - large filled yellow arrow
	if (Wind)
	{
		FVector WindDir = Wind->GetWindDirection();
		FVector BoatFwd = Pawn->GetActorForwardVector();
		// Offset by -PI/2 so that "aligned with boat forward" maps to screen-up
		float WindAngle = FMath::Atan2(WindDir.Y, WindDir.X) - FMath::Atan2(BoatFwd.Y, BoatFwd.X) - PI * 0.5f;

		FLinearColor WindColor(1.0f, 0.85f, 0.0f, 1.0f);
		FLinearColor WindColorDark(0.8f, 0.6f, 0.0f, 1.0f);

		// Arrow shaft - draw as filled rectangle along the wind direction
		float ShaftLen = Radius * 0.55f;
		float ShaftHalfWidth = 4.0f;
		float CosA = FMath::Cos(WindAngle);
		float SinA = FMath::Sin(WindAngle);
		float PerpX = -SinA;
		float PerpY = CosA;

		// Draw shaft as many short segments for fill
		int32 ShaftSteps = 20;
		for (int32 s = 0; s < ShaftSteps; ++s)
		{
			float T0 = (float)s / ShaftSteps;
			float T1 = (float)(s + 1) / ShaftSteps;
			float X0 = CenterX + CosA * ShaftLen * T0;
			float Y0 = CenterY + SinA * ShaftLen * T0;
			float X1 = CenterX + CosA * ShaftLen * T1;
			float Y1 = CenterY + SinA * ShaftLen * T1;
			for (float w = -ShaftHalfWidth; w <= ShaftHalfWidth; w += 1.0f)
			{
				DrawLine(X0 + PerpX * w, Y0 + PerpY * w,
					X1 + PerpX * w, Y1 + PerpY * w,
					WindColor, 1.0f);
			}
		}

		// Large filled arrowhead
		float HeadLen = 25.0f;
		float HeadHalfWidth = 14.0f;
		float TipX = CenterX + CosA * (Radius - 5.0f);
		float TipY = CenterY + SinA * (Radius - 5.0f);
		float BaseX = TipX - CosA * HeadLen;
		float BaseY = TipY - SinA * HeadLen;

		// Fill arrowhead with lines from base edges to tip
		int32 HeadSteps = 30;
		for (int32 h = 0; h <= HeadSteps; ++h)
		{
			float T = (float)h / HeadSteps;
			float HalfW = HeadHalfWidth * (1.0f - T);
			float PtX = BaseX + (TipX - BaseX) * T;
			float PtY = BaseY + (TipY - BaseY) * T;
			DrawLine(PtX + PerpX * HalfW, PtY + PerpY * HalfW,
				PtX - PerpX * HalfW, PtY - PerpY * HalfW,
				WindColor, 1.5f);
		}

		// Arrowhead outline for definition
		float LeftX = BaseX + PerpX * HeadHalfWidth;
		float LeftY = BaseY + PerpY * HeadHalfWidth;
		float RightX = BaseX - PerpX * HeadHalfWidth;
		float RightY = BaseY - PerpY * HeadHalfWidth;
		DrawLine(TipX, TipY, LeftX, LeftY, WindColorDark, 2.0f);
		DrawLine(TipX, TipY, RightX, RightY, WindColorDark, 2.0f);
		DrawLine(LeftX, LeftY, RightX, RightY, WindColorDark, 2.0f);

		// "VIND" label at the arrow base
		float LabelX = CenterX - CosA * (Radius * 0.15f) - 14.0f;
		float LabelY = CenterY - SinA * (Radius * 0.15f) - 8.0f;
		DrawText(TEXT("VIND"), WindColor, LabelX, LabelY, nullptr, 1.0f);

		// Small circle at center
		for (int32 i = 0; i < 24; ++i)
		{
			float A1 = (float)i / 24.0f * 2.0f * PI;
			float A2 = (float)(i + 1) / 24.0f * 2.0f * PI;
			float R = 5.0f;
			DrawLine(CenterX + FMath::Cos(A1) * R, CenterY + FMath::Sin(A1) * R,
				CenterX + FMath::Cos(A2) * R, CenterY + FMath::Sin(A2) * R,
				WindColor, 2.0f);
		}
	}

	// Mission objective marker on compass ring (distinct from wind arrow)
	if (UGameInstance* GI = GetWorld()->GetGameInstance())
	{
		if (UMissionSubsystem* MissionSubsystem = GI->GetSubsystem<UMissionSubsystem>())
		{
			FVector ObjectiveLocation = FVector::ZeroVector;
			if (MissionSubsystem->GetActiveMissionObjectiveLocation(ObjectiveLocation))
			{
				const FVector ToObjective = ObjectiveLocation - Pawn->GetActorLocation();
				const FVector ToObjectiveFlat(ToObjective.X, ToObjective.Y, 0.0f);
				if (!ToObjectiveFlat.IsNearlyZero())
				{
					const FVector BoatFwd = Pawn->GetActorForwardVector();
					const float ObjectiveAngle = FMath::Atan2(ToObjectiveFlat.Y, ToObjectiveFlat.X) - FMath::Atan2(BoatFwd.Y, BoatFwd.X) - PI * 0.5f;

					const FLinearColor MissionColor(1.0f, 0.3f, 0.85f, 1.0f);
					const float MarkerRadius = Radius + 2.0f;
					const float MarkerX = CenterX + FMath::Cos(ObjectiveAngle) * MarkerRadius;
					const float MarkerY = CenterY + FMath::Sin(ObjectiveAngle) * MarkerRadius;
					const float MarkerSize = 7.0f;

					// Diamond marker
					DrawLine(MarkerX, MarkerY - MarkerSize, MarkerX + MarkerSize, MarkerY, MissionColor, 2.0f);
					DrawLine(MarkerX + MarkerSize, MarkerY, MarkerX, MarkerY + MarkerSize, MissionColor, 2.0f);
					DrawLine(MarkerX, MarkerY + MarkerSize, MarkerX - MarkerSize, MarkerY, MissionColor, 2.0f);
					DrawLine(MarkerX - MarkerSize, MarkerY, MarkerX, MarkerY - MarkerSize, MissionColor, 2.0f);

					const float DistanceMeters = ToObjectiveFlat.Size() * 0.01f;
					const FString ObjectiveText = FString::Printf(TEXT("MAL %.0fm"), DistanceMeters);
					DrawText(ObjectiveText, MissionColor, MarkerX + 10.0f, MarkerY - 8.0f, nullptr, 0.8f);
				}
			}
		}
	}
}

void ASailingHUD::DrawWindAndSpeed()
{
	APawn* Pawn = GetOwningPawn();
	if (!Pawn) return;

	AWindActor* Wind = nullptr;
	TArray<AActor*> Found;
	UGameplayStatics::GetAllActorsOfClass(GetWorld(), AWindActor::StaticClass(), Found);
	if (Found.Num() > 0)
	{
		Wind = Cast<AWindActor>(Found[0]);
	}

	// Info panel below compass
	float PanelX = Canvas->ClipX * 0.5f - 100.0f;
	float PanelY = 175.0f;
	float PanelW = 200.0f;
	float PanelH = 85.0f;

	// Dark background
	DrawRect(FLinearColor(0.0f, 0.05f, 0.15f, 0.7f), PanelX, PanelY, PanelW, PanelH);

	// Wind strength and point of sail (enheter: m/s og knop)
	const ASailboatPawn* Boat = Cast<ASailboatPawn>(Pawn);
	if (Wind)
	{
		float WindMs = Wind->GetWindStrength() * WindStrengthToMs;
		FString WindText = FString::Printf(TEXT("VIND: %.1f m/s"), WindMs);
		DrawText(WindText, FLinearColor(0.5f, 0.9f, 1.0f, 1.0f),
			PanelX + 15.0f, PanelY + 8.0f, nullptr, 1.3f);

		// Point of sail label
		if (Boat)
		{
			FVector BoatFwd = Pawn->GetActorForwardVector();
			FVector WindDir = Wind->GetWindDirection();
			// WindDir peker mot vindkilden: 0° = mot vind, 180° = med vind
			float CosA = FVector::DotProduct(BoatFwd, WindDir);
			float Angle = FMath::RadiansToDegrees(FMath::Acos(FMath::Clamp(CosA, -1.0f, 1.0f)));

			FString PointOfSail;
			FLinearColor PointColor;
			if (Angle < Boat->NoGoZoneAngle)
			{
				PointOfSail = TEXT("I JERN");
				PointColor = FLinearColor(1.0f, 0.3f, 0.3f, 1.0f);
			}
			else if (Angle < 60.0f)
			{
				PointOfSail = TEXT("BIDEVIND");
				PointColor = FLinearColor(1.0f, 0.7f, 0.3f, 1.0f);
			}
			else if (Angle < 80.0f)
			{
				PointOfSail = TEXT("SLOR");
				PointColor = FLinearColor(1.0f, 1.0f, 0.3f, 1.0f);
			}
			else if (Angle < 100.0f)
			{
				PointOfSail = TEXT("HALV VIND");
				PointColor = FLinearColor(0.2f, 1.0f, 0.2f, 1.0f);
			}
			else if (Angle < 135.0f)
			{
				PointOfSail = TEXT("ROMSKJOETS");
				PointColor = FLinearColor(1.0f, 1.0f, 0.3f, 1.0f);
			}
			else
			{
				PointOfSail = TEXT("LENS");
				PointColor = FLinearColor(1.0f, 0.7f, 0.3f, 1.0f);
			}

			DrawText(PointOfSail, PointColor,
				PanelX + 15.0f, PanelY + 32.0f, nullptr, 1.3f);
		}
	}

	// Speed (knop)
	if (Boat)
	{
		float SpeedKn = Boat->CurrentSpeed * SpeedToKnots;
		FLinearColor SpeedColor;
		if (Boat->CurrentSpeed > 500.0f)
			SpeedColor = FLinearColor(0.2f, 1.0f, 0.2f, 1.0f); // Green = fast
		else if (Boat->CurrentSpeed > 100.0f)
			SpeedColor = FLinearColor(1.0f, 1.0f, 0.3f, 1.0f); // Yellow = medium
		else
			SpeedColor = FLinearColor(1.0f, 0.4f, 0.3f, 1.0f); // Red = slow/stopped

		FString SpeedText = FString::Printf(TEXT("FART: %.1f kn"), SpeedKn);
		DrawText(SpeedText, SpeedColor,
			PanelX + 15.0f, PanelY + 56.0f, nullptr, 1.3f);
	}
}

void ASailingHUD::DrawDiscoveryPopup(float DeltaTime)
{
	DiscoveryPopupTimer -= DeltaTime;
	if (DiscoveryPopupTimer <= 0.0f)
	{
		bShowingDiscoveryPopup = false;
		return;
	}

	float Alpha = 1.0f;
	if (DiscoveryPopupTimer < 1.0f)
	{
		Alpha = DiscoveryPopupTimer;
	}
	float TimeElapsed = DiscoveryPopupDuration - DiscoveryPopupTimer;
	if (TimeElapsed < 0.5f)
	{
		Alpha = TimeElapsed * 2.0f;
	}

	float CenterX = Canvas->ClipX * 0.5f;
	float CenterY = Canvas->ClipY * 0.3f;

	float BoxWidth = 350.0f;
	float BoxHeight = 90.0f;
	FLinearColor BoxColor(0.0f, 0.1f, 0.2f, 0.85f * Alpha);
	DrawRect(BoxColor, CenterX - BoxWidth * 0.5f, CenterY - BoxHeight * 0.5f, BoxWidth, BoxHeight);

	// Gold border
	FLinearColor BorderColor(0.9f, 0.7f, 0.2f, Alpha);
	float BT = 3.0f;
	DrawRect(BorderColor, CenterX - BoxWidth * 0.5f, CenterY - BoxHeight * 0.5f, BoxWidth, BT);
	DrawRect(BorderColor, CenterX - BoxWidth * 0.5f, CenterY + BoxHeight * 0.5f - BT, BoxWidth, BT);
	DrawRect(BorderColor, CenterX - BoxWidth * 0.5f, CenterY - BoxHeight * 0.5f, BT, BoxHeight);
	DrawRect(BorderColor, CenterX + BoxWidth * 0.5f - BT, CenterY - BoxHeight * 0.5f, BT, BoxHeight);

	// Header
	FLinearColor HeaderColor(0.9f, 0.7f, 0.2f, Alpha);
	DrawText(TEXT("NY OY OPPDAGET!"), HeaderColor,
		CenterX - 80.0f, CenterY - 30.0f, nullptr, 1.5f);

	// Island name
	FLinearColor NameColor(1.0f, 1.0f, 1.0f, Alpha);
	float NameOffset = CurrentDiscoveryName.Len() * 5.0f;
	DrawText(CurrentDiscoveryName, NameColor,
		CenterX - NameOffset, CenterY + 5.0f, nullptr, 1.8f);
}

void ASailingHUD::DrawOverviewMap()
{
	APawn* Pawn = GetOwningPawn();
	if (!Pawn || !Canvas) return;

	ASailingGameMode* GM = Cast<ASailingGameMode>(UGameplayStatics::GetGameMode(GetWorld()));
	if (!GM) return;
	USaveGameSailing* SaveGame = GM->GetSaveGame();
	if (!SaveGame) return;

	FVector PlayerLoc = Pawn->GetActorLocation();
	const float PlayerX = PlayerLoc.X;
	const float PlayerY = PlayerLoc.Y;

	// Kartpanel nederst til høyre
	const float MapLeft = Canvas->ClipX - MapOffsetRight - MapSizePixels;
	const float MapTop = Canvas->ClipY - MapOffsetBottom - MapSizePixels;
	const float MapCenterX = MapLeft + MapSizePixels * 0.5f;
	const float MapCenterY = MapTop + MapSizePixels * 0.5f;
	const float Scale = (MapSizePixels * 0.5f) / FMath::Max(1.0f, MapWorldRadius);

	// 1. Bakgrunn (hav)
	DrawRect(MapSeaColor, MapLeft, MapTop, MapSizePixels, MapSizePixels);

	// 2. Oppdagede øyer innenfor radius
	for (const auto& Pair : SaveGame->DiscoveredIslands)
	{
		const FVector& Loc = Pair.Value.WorldLocation;
		float Dx = Loc.X - PlayerX;
		float Dy = Loc.Y - PlayerY;
		if (FMath::Sqrt(Dx * Dx + Dy * Dy) > MapWorldRadius) continue;

		float Px = MapCenterX + Dx * Scale;
		float Py = MapCenterY - Dy * Scale; // nord = +Y i verden = opp på skjerm
		const float DotSize = 4.0f;
		DrawRect(MapIslandColor, Px - DotSize * 0.5f, Py - DotSize * 0.5f, DotSize, DotSize);
	}

	// 2b. Aktivt oppdragsmål (hvis lokasjonsbasert)
	if (UGameInstance* GI = GetWorld()->GetGameInstance())
	{
		// Port markers
		if (UWorldStreamingSubsystem* WorldSubsystem = GI->GetSubsystem<UWorldStreamingSubsystem>())
		{
			for (const FName& PortId : WorldSubsystem->GetRegisteredPortIds())
			{
				FVector PortLoc;
				if (!WorldSubsystem->GetPortLocation(PortId, PortLoc))
				{
					continue;
				}

				const float Dx = PortLoc.X - PlayerX;
				const float Dy = PortLoc.Y - PlayerY;
				if (FMath::Sqrt(Dx * Dx + Dy * Dy) > MapWorldRadius)
				{
					continue;
				}

				const float Px = MapCenterX + Dx * Scale;
				const float Py = MapCenterY - Dy * Scale;
				const float DotSize = 5.0f;
				DrawRect(MapPortColor, Px - DotSize * 0.5f, Py - DotSize * 0.5f, DotSize, DotSize);
			}
		}

		if (UMissionSubsystem* MissionSubsystem = GI->GetSubsystem<UMissionSubsystem>())
		{
			FVector ObjectiveLoc;
			if (MissionSubsystem->GetActiveMissionObjectiveLocation(ObjectiveLoc))
			{
				const float Dx = ObjectiveLoc.X - PlayerX;
				const float Dy = ObjectiveLoc.Y - PlayerY;
				const float Dist = FMath::Sqrt(Dx * Dx + Dy * Dy);
				if (Dist <= MapWorldRadius)
				{
					const float Px = MapCenterX + Dx * Scale;
					const float Py = MapCenterY - Dy * Scale;
					DrawLine(MapCenterX, MapCenterY, Px, Py, FLinearColor(MapMissionColor.R, MapMissionColor.G, MapMissionColor.B, 0.7f), 1.2f);
					const float DotSize = 7.0f;
					DrawRect(MapMissionColor, Px - DotSize * 0.5f, Py - DotSize * 0.5f, DotSize, DotSize);
				}
			}
		}
	}

	// 3. Spiller som liten trekant (retning fra GetActorForwardVector)
	FVector Forward = Pawn->GetActorForwardVector();
	float Angle = FMath::Atan2(Forward.X, Forward.Y); // vinkel fra nord (verden +Y)
	const float TriR = 8.0f;
	const float TriW = 4.0f;
	float TipX = MapCenterX + FMath::Sin(Angle) * TriR;
	float TipY = MapCenterY - FMath::Cos(Angle) * TriR;
	float B1X = MapCenterX - FMath::Sin(Angle) * TriR + FMath::Cos(Angle) * TriW;
	float B1Y = MapCenterY + FMath::Cos(Angle) * TriR + FMath::Sin(Angle) * TriW;
	float B2X = MapCenterX - FMath::Sin(Angle) * TriR - FMath::Cos(Angle) * TriW;
	float B2Y = MapCenterY + FMath::Cos(Angle) * TriR - FMath::Sin(Angle) * TriW;
	DrawLine(TipX, TipY, B1X, B1Y, MapPlayerColor, 2.0f);
	DrawLine(TipX, TipY, B2X, B2Y, MapPlayerColor, 2.0f);
	DrawLine(B1X, B1Y, B2X, B2Y, MapPlayerColor, 2.0f);
}

void ASailingHUD::DrawDiscoveryCounter()
{
	ASailingGameMode* GM = Cast<ASailingGameMode>(UGameplayStatics::GetGameMode(GetWorld()));
	if (!GM) return;

	USaveGameSailing* SaveGame = GM->GetSaveGame();
	if (!SaveGame) return;

	// Top-left with background
	float BoxX = 10.0f;
	float BoxY = 10.0f;
	float BoxW = 320.0f;
	float BoxH = 124.0f;
	DrawRect(FLinearColor(0.0f, 0.05f, 0.15f, 0.7f), BoxX, BoxY, BoxW, BoxH);

	FString CounterText = FString::Printf(TEXT("OYER: %d oppdaget"), SaveGame->TotalIslandsDiscovered);
	DrawText(CounterText, FLinearColor(0.5f, 0.9f, 1.0f, 1.0f),
		BoxX + 10.0f, BoxY + 8.0f, nullptr, 1.2f);

	int32 Credits = 0;
	int32 BoatCondition = 100;
	FName ActiveMissionId = NAME_None;
	FText ActiveMissionTitle = FText::GetEmpty();
	float ObjectiveDistanceMeters = -1.0f;
	float ObjectiveBearingDegrees = -999.0f;
	FName LastVisitedPortId = NAME_None;
	if (UGameInstance* GI = GetWorld()->GetGameInstance())
	{
		if (UEconomySubsystem* EconomySubsystem = GI->GetSubsystem<UEconomySubsystem>())
		{
			Credits = EconomySubsystem->GetCredits();
			BoatCondition = EconomySubsystem->GetBoatConditionPercent();
		}
		if (UMissionSubsystem* MissionSubsystem = GI->GetSubsystem<UMissionSubsystem>())
		{
			ActiveMissionId = MissionSubsystem->GetActiveMissionId();
			ActiveMissionTitle = MissionSubsystem->GetActiveMissionDisplayName();
			FVector ObjectiveLocation;
			if (MissionSubsystem->GetActiveMissionObjectiveLocation(ObjectiveLocation))
			{
				if (APawn* Pawn = GetOwningPawn())
				{
					const FVector ToObjective = ObjectiveLocation - Pawn->GetActorLocation();
					const FVector ToObjectiveFlat(ToObjective.X, ToObjective.Y, 0.0f);
					const float DistCm = ToObjectiveFlat.Size();
					ObjectiveDistanceMeters = DistCm * 0.01f;

					if (!ToObjectiveFlat.IsNearlyZero())
					{
						const FVector BoatFwd = Pawn->GetActorForwardVector();
						const float RelativeBearingRad = FMath::Atan2(ToObjectiveFlat.Y, ToObjectiveFlat.X) - FMath::Atan2(BoatFwd.Y, BoatFwd.X);
						ObjectiveBearingDegrees = FMath::UnwindDegrees(FMath::RadiansToDegrees(RelativeBearingRad));
					}
				}
			}
		}
		if (UWorldStreamingSubsystem* WorldSubsystem = GI->GetSubsystem<UWorldStreamingSubsystem>())
		{
			LastVisitedPortId = WorldSubsystem->GetLastVisitedPortId();
		}
	}

	const FString CreditsText = FString::Printf(TEXT("KREDITTER: %d"), Credits);
	DrawText(CreditsText, FLinearColor(1.0f, 0.9f, 0.35f, 1.0f),
		BoxX + 10.0f, BoxY + 32.0f, nullptr, 1.2f);

	const FString MissionText = ActiveMissionId.IsNone()
		? FString(TEXT("AKTIVT OPPDRAG: Ingen"))
		: FString::Printf(TEXT("AKTIVT OPPDRAG: %s"), *ActiveMissionId.ToString());
	DrawText(MissionText, FLinearColor(0.7f, 1.0f, 0.8f, 1.0f),
		BoxX + 10.0f, BoxY + 56.0f, nullptr, 1.0f);

	if (!ActiveMissionTitle.IsEmpty())
	{
		const FString TitleText = FString::Printf(TEXT("MÅL: %s"), *ActiveMissionTitle.ToString());
		DrawText(TitleText, FLinearColor(0.8f, 0.9f, 1.0f, 1.0f),
			BoxX + 170.0f, BoxY + 56.0f, nullptr, 0.9f);
	}

	if (ObjectiveDistanceMeters >= 0.0f)
	{
		const FString DistText = FString::Printf(TEXT("DIST: %.0f m"), ObjectiveDistanceMeters);
		DrawText(DistText, FLinearColor(1.0f, 0.95f, 0.55f, 1.0f),
			BoxX + 170.0f, BoxY + 32.0f, nullptr, 1.0f);
	}

	if (ObjectiveBearingDegrees > -999.0f && ObjectiveBearingDegrees <= 999.0f)
	{
		const FString BearingText = FString::Printf(TEXT("PEILING: %+0.0f°"), ObjectiveBearingDegrees);
		DrawText(BearingText, FLinearColor(0.95f, 0.75f, 1.0f, 1.0f),
			BoxX + 170.0f, BoxY + 80.0f, nullptr, 0.95f);
	}

	const FLinearColor ConditionColor = BoatCondition > 70
		? FLinearColor(0.3f, 1.0f, 0.4f, 1.0f)
		: (BoatCondition > 35 ? FLinearColor(1.0f, 0.9f, 0.3f, 1.0f) : FLinearColor(1.0f, 0.35f, 0.35f, 1.0f));
	const FString ConditionText = FString::Printf(TEXT("SKROGTILSTAND: %d%%"), BoatCondition);
	DrawText(ConditionText, ConditionColor, BoxX + 10.0f, BoxY + 80.0f, nullptr, 1.0f);

	if (!LastVisitedPortId.IsNone())
	{
		const FString PortText = FString::Printf(TEXT("SISTE HAVN: %s"), *LastVisitedPortId.ToString());
		DrawText(PortText, FLinearColor(0.85f, 0.75f, 1.0f, 1.0f), BoxX + 10.0f, BoxY + 98.0f, nullptr, 0.9f);
	}

	PushOverlayData(SaveGame->TotalIslandsDiscovered, Credits, ActiveMissionId, ActiveMissionTitle,
		ObjectiveDistanceMeters, BoatCondition, ObjectiveBearingDegrees, LastVisitedPortId);
}

bool ASailingHUD::PauseMenuButtonHit(float X, float Y, float Bx, float By, float Bw, float Bh) const
{
	return X >= Bx && X <= Bx + Bw && Y >= By && Y <= By + Bh;
}

void ASailingHUD::DrawPauseMenu()
{
	PauseCenterX = Canvas->ClipX * 0.5f;
	PauseCenterY = Canvas->ClipY * 0.5f;
	const float TotalH = PauseButtonH * 3.0f + PauseButtonSpacing * 2.0f;
	float Y = PauseCenterY - TotalH * 0.5f;
	PauseResumeY = Y;
	PauseSaveY = Y + PauseButtonH + PauseButtonSpacing;
	PauseQuitY = Y + (PauseButtonH + PauseButtonSpacing) * 2.0f;
	const float Bx = PauseCenterX - PauseButtonW * 0.5f;

	// Halvtransparent bakgrunn
	DrawRect(FLinearColor(0.0f, 0.0f, 0.0f, 0.7f), 0.0f, 0.0f, Canvas->ClipX, Canvas->ClipY);
	DrawRect(FLinearColor(0.0f, 0.08f, 0.18f, 0.95f), Bx - 24.0f, PauseResumeY - 24.0f, PauseButtonW + 48.0f, TotalH + 48.0f);

	FLinearColor BtnColor(0.2f, 0.5f, 0.8f, 0.9f);
	DrawRect(BtnColor, Bx, PauseResumeY, PauseButtonW, PauseButtonH);
	DrawText(TEXT("Fortsett"), FLinearColor::White, Bx + 20.0f, PauseResumeY + 10.0f, nullptr, 1.4f);
	DrawRect(BtnColor, Bx, PauseSaveY, PauseButtonW, PauseButtonH);
	DrawText(TEXT("Lagre"), FLinearColor::White, Bx + 20.0f, PauseSaveY + 10.0f, nullptr, 1.4f);
	DrawRect(BtnColor, Bx, PauseQuitY, PauseButtonW, PauseButtonH);
	DrawText(TEXT("Avslutt til meny"), FLinearColor::White, Bx + 20.0f, PauseQuitY + 10.0f, nullptr, 1.4f);
}

void ASailingHUD::OnPauseMenuClick(float X, float Y)
{
	ASailingPlayerController* PC = Cast<ASailingPlayerController>(PlayerOwner);
	if (!PC || !PC->IsPauseMenuShown()) return;

	const float Bx = PauseCenterX - PauseButtonW * 0.5f;

	if (PauseMenuButtonHit(X, Y, Bx, PauseResumeY, PauseButtonW, PauseButtonH))
	{
		PC->ClosePauseMenu();
		return;
	}
	if (PauseMenuButtonHit(X, Y, Bx, PauseSaveY, PauseButtonW, PauseButtonH))
	{
		if (ASailingGameMode* GM = Cast<ASailingGameMode>(UGameplayStatics::GetGameMode(GetWorld())))
		{
			GM->SaveGame_();
		}
		return;
	}
	if (PauseMenuButtonHit(X, Y, Bx, PauseQuitY, PauseButtonW, PauseButtonH))
	{
		PC->ClosePauseMenu();
		UGameplayStatics::OpenLevel(this, FName(TEXT("MainMenu")));
	}
}
