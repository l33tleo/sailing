#include "UI/PortMissionBoardWidget.h"

void UPortMissionBoardWidget::PushMissionBoardData(const FPortMissionBoardData& InData)
{
	LastData = BuildActionStateAnnotatedBoardData(InData);
	OnMissionBoardDataUpdated(LastData);
}

void UPortMissionBoardWidget::RequestAcceptMission(FName MissionId)
{
	FText BlockedReason;
	if (!CanRequestMissionAccept(LastData, MissionId, BlockedReason))
	{
		if (!BlockedReason.IsEmpty())
		{
			OnActionBlocked.Broadcast(EPortBoardActionType::MissionAccept, BlockedReason);
		}
		return;
	}

	OnAcceptMissionRequested.Broadcast(MissionId);
}

bool UPortMissionBoardWidget::RequiresMissionSwitchConfirmation(
	FName CurrentMissionId,
	FName RequestedMissionId,
	FName PendingConfirmationMissionId)
{
	if (RequestedMissionId.IsNone() || CurrentMissionId.IsNone() || CurrentMissionId == RequestedMissionId)
	{
		return false;
	}

	return PendingConfirmationMissionId != RequestedMissionId;
}

bool UPortMissionBoardWidget::IsUpgradePurchaseRequestValid(
	bool bSupportsUpgradeService,
	const TArray<FName>& InOfferedUpgradeIds,
	FName RequestedUpgradeId)
{
	if (!bSupportsUpgradeService || RequestedUpgradeId.IsNone())
	{
		return false;
	}

	if (InOfferedUpgradeIds.Num() == 0)
	{
		return true;
	}

	return InOfferedUpgradeIds.Contains(RequestedUpgradeId);
}

bool UPortMissionBoardWidget::RequiresUpgradePurchaseConfirmation(
	FName RequestedUpgradeId,
	FName PendingUpgradePurchaseId)
{
	if (RequestedUpgradeId.IsNone())
	{
		return false;
	}

	return PendingUpgradePurchaseId != RequestedUpgradeId;
}

FText UPortMissionBoardWidget::BuildUpgradePricingStatusText(
	bool bSupportsUpgradeService,
	float UpgradeCostMultiplier)
{
	if (!bSupportsUpgradeService)
	{
		return FText::FromString(TEXT("Ingen oppgraderingspriser tilgjengelig."));
	}

	const float SafeMultiplier = FMath::Max(0.1f, UpgradeCostMultiplier);
	if (FMath::IsNearlyEqual(SafeMultiplier, 1.0f))
	{
		return FText::FromString(TEXT("Standardpriser aktiv."));
	}

	if (SafeMultiplier < 1.0f)
	{
		return FText::FromString(FString::Printf(TEXT("Havnerabatt: x%.2f"), SafeMultiplier));
	}

	return FText::FromString(FString::Printf(TEXT("Havnepåslag: x%.2f"), SafeMultiplier));
}

EPortMissionAvailabilityReason UPortMissionBoardWidget::DetermineMissionAvailabilityReason(
	bool bSupportsMissionBoard,
	bool bMissionBoardOnCooldown,
	bool bHasAnyOffers)
{
	if (!bSupportsMissionBoard)
	{
		return EPortMissionAvailabilityReason::MissionBoardDisabled;
	}

	if (bMissionBoardOnCooldown)
	{
		return EPortMissionAvailabilityReason::CooldownActive;
	}

	if (!bHasAnyOffers)
	{
		return EPortMissionAvailabilityReason::NoOffers;
	}

	return EPortMissionAvailabilityReason::Ready;
}

FText UPortMissionBoardWidget::BuildMissionAvailabilityStatusText(
	EPortMissionAvailabilityReason Reason,
	float CooldownRemainingSeconds,
	bool bSupportsUpgradeService)
{
	switch (Reason)
	{
	case EPortMissionAvailabilityReason::MissionBoardDisabled:
		return bSupportsUpgradeService
			? FText::FromString(TEXT("Oppdragstavlen er stengt, men havneservice er tilgjengelig."))
			: FText::FromString(TEXT("Ingen tavletjenester tilgjengelig i denne havnen."));
	case EPortMissionAvailabilityReason::CooldownActive:
		return FText::FromString(FString::Printf(TEXT("Tavlen oppdateres om %.0f sekunder"), FMath::Max(0.0f, CooldownRemainingSeconds)));
	case EPortMissionAvailabilityReason::NoOffers:
		return bSupportsUpgradeService
			? FText::FromString(TEXT("Ingen tilgjengelige oppdrag – bruk havneservice nedenfor."))
			: FText::FromString(TEXT("Ingen tilgjengelige oppdrag i denne havnen."));
	case EPortMissionAvailabilityReason::Ready:
	default:
		return FText::FromString(TEXT("Velg et oppdrag fra havnetavlen."));
	}
}

EPortMissionOfferSource UPortMissionBoardWidget::DetermineMissionOfferSource(
	bool bSupportsMissionBoard,
	bool bUsedWeightedRules,
	bool bUsedFallbackOffers)
{
	if (!bSupportsMissionBoard)
	{
		return EPortMissionOfferSource::None;
	}

	if (bUsedWeightedRules)
	{
		return EPortMissionOfferSource::WeightedRules;
	}

	if (bUsedFallbackOffers)
	{
		return EPortMissionOfferSource::FallbackList;
	}

	return EPortMissionOfferSource::None;
}

FText UPortMissionBoardWidget::BuildMissionOfferSourceStatusText(
	EPortMissionOfferSource Source,
	int32 EligibleWeightedRuleCount,
	int32 VisitGatedRuleCount)
{
	FString BaseText;
	switch (Source)
	{
	case EPortMissionOfferSource::WeightedRules:
		BaseText = FString::Printf(TEXT("Vektede oppdragsregler aktiv (%d kvalifiserte)."), FMath::Max(0, EligibleWeightedRuleCount));
		break;
	case EPortMissionOfferSource::FallbackList:
		BaseText = TEXT("Standard oppdragsliste brukes.");
		break;
	case EPortMissionOfferSource::None:
	default:
		BaseText = TEXT("Ingen oppdragskilde aktiv.");
		break;
	}

	if (VisitGatedRuleCount > 0)
	{
		BaseText += FString::Printf(TEXT(" %d regel(er) låst av havnebesøk."), FMath::Max(0, VisitGatedRuleCount));
	}

	return FText::FromString(BaseText);
}

FText UPortMissionBoardWidget::BuildMissionVisitRequirementStatusText(
	int32 MinPortVisits,
	int32 CurrentPortVisitCount)
{
	const int32 SafeMinVisits = FMath::Max(0, MinPortVisits);
	if (SafeMinVisits <= 0)
	{
		return FText::FromString(TEXT("Ingen havnekrav."));
	}

	const int32 SafeCurrentVisits = FMath::Max(0, CurrentPortVisitCount);
	if (SafeCurrentVisits >= SafeMinVisits)
	{
		return FText::FromString(FString::Printf(TEXT("Krever %d havnebesøk (oppfylt)."), SafeMinVisits));
	}

	return FText::FromString(FString::Printf(TEXT("Krever %d havnebesøk (mangler %d)."), SafeMinVisits, SafeMinVisits - SafeCurrentVisits));
}

EPortUpgradeAvailabilityReason UPortMissionBoardWidget::DetermineUpgradeAvailabilityReason(
	bool bSupportsUpgradeService,
	int32 ValidUpgradeOfferCount,
	int32 AffordableUpgradeOfferCount)
{
	if (!bSupportsUpgradeService)
	{
		return EPortUpgradeAvailabilityReason::ServiceUnavailable;
	}

	if (ValidUpgradeOfferCount <= 0)
	{
		return EPortUpgradeAvailabilityReason::NoValidOffers;
	}

	if (AffordableUpgradeOfferCount <= 0)
	{
		return EPortUpgradeAvailabilityReason::NoAffordableOffers;
	}

	return EPortUpgradeAvailabilityReason::Ready;
}

FText UPortMissionBoardWidget::BuildUpgradeAvailabilityStatusText(
	EPortUpgradeAvailabilityReason Reason,
	int32 AffordableUpgradeOfferCount)
{
	switch (Reason)
	{
	case EPortUpgradeAvailabilityReason::ServiceUnavailable:
		return FText::FromString(TEXT("Ingen oppgraderingsservice i denne havnen."));
	case EPortUpgradeAvailabilityReason::NoValidOffers:
		return FText::FromString(TEXT("Ingen gyldige oppgraderinger konfigurert for havnen."));
	case EPortUpgradeAvailabilityReason::NoAffordableOffers:
		return FText::FromString(TEXT("Ingen oppgraderinger kan kjøpes akkurat nå."));
	case EPortUpgradeAvailabilityReason::Ready:
	default:
		return FText::FromString(FString::Printf(TEXT("%d oppgradering(er) kan kjøpes nå."), FMath::Max(0, AffordableUpgradeOfferCount)));
	}
}

EPortUpgradeOfferSource UPortMissionBoardWidget::DetermineUpgradeOfferSource(
	bool bSupportsUpgradeService,
	bool bUsedWeightedRules,
	bool bUsedFallbackOffers)
{
	if (!bSupportsUpgradeService)
	{
		return EPortUpgradeOfferSource::None;
	}

	if (bUsedWeightedRules)
	{
		return EPortUpgradeOfferSource::WeightedRules;
	}

	if (bUsedFallbackOffers)
	{
		return EPortUpgradeOfferSource::FallbackList;
	}

	return EPortUpgradeOfferSource::None;
}

FText UPortMissionBoardWidget::BuildUpgradeOfferSourceStatusText(
	EPortUpgradeOfferSource Source,
	int32 EligibleWeightedRuleCount,
	int32 VisitGatedRuleCount,
	int32 HiddenUnlockedCount)
{
	FString BaseText;
	switch (Source)
	{
	case EPortUpgradeOfferSource::WeightedRules:
		BaseText = FString::Printf(TEXT("Vektede oppgraderingsregler aktiv (%d kvalifiserte)."), FMath::Max(0, EligibleWeightedRuleCount));
		break;
	case EPortUpgradeOfferSource::FallbackList:
		BaseText = TEXT("Standard oppgraderingsliste brukes.");
		break;
	case EPortUpgradeOfferSource::None:
	default:
		BaseText = TEXT("Ingen oppgraderingskilde aktiv.");
		break;
	}

	if (VisitGatedRuleCount > 0)
	{
		BaseText += FString::Printf(TEXT(" %d regel(er) låst av havnebesøk."), FMath::Max(0, VisitGatedRuleCount));
	}
	if (HiddenUnlockedCount > 0)
	{
		BaseText += FString::Printf(TEXT(" %d opplåste skjult."), FMath::Max(0, HiddenUnlockedCount));
	}

	return FText::FromString(BaseText);
}

FText UPortMissionBoardWidget::BuildUpgradeVisitRequirementStatusText(
	int32 MinPortVisits,
	int32 CurrentPortVisitCount)
{
	const int32 SafeMinVisits = FMath::Max(0, MinPortVisits);
	if (SafeMinVisits <= 0)
	{
		return FText::FromString(TEXT("Ingen havnekrav."));
	}

	const int32 SafeCurrentVisits = FMath::Max(0, CurrentPortVisitCount);
	if (SafeCurrentVisits >= SafeMinVisits)
	{
		return FText::FromString(FString::Printf(TEXT("Krever %d havnebesøk (oppfylt)."), SafeMinVisits));
	}

	return FText::FromString(FString::Printf(TEXT("Krever %d havnebesøk (mangler %d)."), SafeMinVisits, SafeMinVisits - SafeCurrentVisits));
}

FText UPortMissionBoardWidget::BuildRefreshContextStatusText(EPortBoardRefreshContext RefreshContext)
{
	switch (RefreshContext)
	{
	case EPortBoardRefreshContext::DockArrival:
		return FText::FromString(TEXT("Tavle oppdatert ved anløp."));
	case EPortBoardRefreshContext::CooldownBlocked:
		return FText::FromString(TEXT("Tavle oppdatert mens havnen er i nedkjøling."));
	case EPortBoardRefreshContext::ServiceOnly:
		return FText::FromString(TEXT("Tavle viser kun havneservice."));
	case EPortBoardRefreshContext::MissionSwitchConfirmation:
		return FText::FromString(TEXT("Venter på bekreftelse av oppdragsbytte."));
	case EPortBoardRefreshContext::UpgradePurchaseConfirmation:
		return FText::FromString(TEXT("Venter på bekreftelse av oppgraderingskjøp."));
	case EPortBoardRefreshContext::ManualRefresh:
		return FText::FromString(TEXT("Tavle manuelt oppdatert."));
	default:
		return FText::FromString(TEXT("Tavle oppdatert."));
	}
}

FText UPortMissionBoardWidget::BuildManualRefreshStatusText(
	bool bSupportsManualRefresh,
	bool bManualRefreshOnCooldown,
	float CooldownRemainingSeconds,
	int32 ManualRefreshCreditCost,
	bool bCanAffordManualRefresh)
{
	if (!bSupportsManualRefresh)
	{
		return FText::FromString(TEXT("Manuell oppfriskning er deaktivert i denne havnen."));
	}

	if (bManualRefreshOnCooldown)
	{
		return FText::FromString(FString::Printf(TEXT("Manuell oppfriskning klar om %.0f sekunder."), FMath::Max(0.0f, CooldownRemainingSeconds)));
	}

	const int32 SafeRefreshCost = FMath::Max(0, ManualRefreshCreditCost);
	if (SafeRefreshCost <= 0)
	{
		return FText::FromString(TEXT("Manuell oppfriskning er gratis."));
	}

	if (bCanAffordManualRefresh)
	{
		return FText::FromString(FString::Printf(TEXT("Manuell oppfriskning tilgjengelig (%d kreditter)."), SafeRefreshCost));
	}

	return FText::FromString(FString::Printf(TEXT("Mangler kreditter til manuell oppfriskning (%d)."), SafeRefreshCost));
}

bool UPortMissionBoardWidget::CanRequestMissionAccept(
	const FPortMissionBoardData& BoardData,
	FName RequestedMissionId,
	FText& OutBlockedReason)
{
	const EPortMissionOfferActionBlockReason BlockReason = DetermineMissionOfferActionBlockReason(BoardData, RequestedMissionId);
	OutBlockedReason = BuildMissionOfferActionBlockReasonText(BlockReason, BoardData, RequestedMissionId);
	return BlockReason == EPortMissionOfferActionBlockReason::None;
}

EPortMissionOfferActionBlockReason UPortMissionBoardWidget::DetermineMissionOfferActionBlockReason(
	const FPortMissionBoardData& BoardData,
	FName RequestedMissionId)
{
	if (RequestedMissionId.IsNone())
	{
		return EPortMissionOfferActionBlockReason::InvalidMission;
	}

	if (!BoardData.bSupportsMissionBoard)
	{
		return EPortMissionOfferActionBlockReason::MissionBoardDisabled;
	}

	if (BoardData.bMissionBoardOnCooldown)
	{
		return EPortMissionOfferActionBlockReason::MissionBoardCooldown;
	}

	if (BoardData.OfferedMissionIds.Num() > 0 && !BoardData.OfferedMissionIds.Contains(RequestedMissionId))
	{
		return EPortMissionOfferActionBlockReason::NotOfferedAtPort;
	}

	if (!BoardData.CurrentMissionId.IsNone() && BoardData.CurrentMissionId == RequestedMissionId)
	{
		return EPortMissionOfferActionBlockReason::AlreadyActiveMission;
	}

	return EPortMissionOfferActionBlockReason::None;
}

FText UPortMissionBoardWidget::BuildMissionOfferActionBlockReasonText(
	EPortMissionOfferActionBlockReason BlockReason,
	const FPortMissionBoardData& BoardData,
	FName /*RequestedMissionId*/)
{
	switch (BlockReason)
	{
	case EPortMissionOfferActionBlockReason::None:
		return FText::GetEmpty();
	case EPortMissionOfferActionBlockReason::InvalidMission:
		return FText::FromString(TEXT("Ingen oppdrag valgt."));
	case EPortMissionOfferActionBlockReason::MissionBoardDisabled:
		return BuildMissionAvailabilityStatusText(
			EPortMissionAvailabilityReason::MissionBoardDisabled,
			BoardData.CooldownRemainingSeconds,
			BoardData.bSupportsUpgradeService);
	case EPortMissionOfferActionBlockReason::MissionBoardCooldown:
		return BuildMissionAvailabilityStatusText(
			EPortMissionAvailabilityReason::CooldownActive,
			BoardData.CooldownRemainingSeconds,
			BoardData.bSupportsUpgradeService);
	case EPortMissionOfferActionBlockReason::NotOfferedAtPort:
		return FText::FromString(TEXT("Oppdraget tilbys ikke i denne havnen."));
	case EPortMissionOfferActionBlockReason::AlreadyActiveMission:
		return FText::FromString(TEXT("Oppdraget er allerede aktivt."));
	default:
		return FText::FromString(TEXT("Oppdraget kan ikke velges akkurat nå."));
	}
}

bool UPortMissionBoardWidget::CanRequestUpgradePurchase(
	const FPortMissionBoardData& BoardData,
	FName RequestedUpgradeId,
	FText& OutBlockedReason)
{
	const EPortUpgradeOfferActionBlockReason BlockReason = DetermineUpgradeOfferActionBlockReason(BoardData, RequestedUpgradeId);
	OutBlockedReason = BuildUpgradeOfferActionBlockReasonText(BlockReason, BoardData, RequestedUpgradeId);
	return BlockReason == EPortUpgradeOfferActionBlockReason::None;
}

EPortUpgradeOfferActionBlockReason UPortMissionBoardWidget::DetermineUpgradeOfferActionBlockReason(
	const FPortMissionBoardData& BoardData,
	FName RequestedUpgradeId)
{
	if (!BoardData.bSupportsUpgradeService)
	{
		return EPortUpgradeOfferActionBlockReason::UpgradeServiceDisabled;
	}

	if (RequestedUpgradeId.IsNone())
	{
		return EPortUpgradeOfferActionBlockReason::InvalidUpgrade;
	}

	if (BoardData.OfferedUpgradeIds.Num() > 0 && !BoardData.OfferedUpgradeIds.Contains(RequestedUpgradeId))
	{
		return EPortUpgradeOfferActionBlockReason::NotOfferedAtPort;
	}

	const FPortUpgradeOfferEntry* OfferEntry = BoardData.OfferedUpgrades.FindByPredicate(
		[RequestedUpgradeId](const FPortUpgradeOfferEntry& Entry)
		{
			return Entry.UpgradeId == RequestedUpgradeId;
		});
	if (OfferEntry)
	{
		if (OfferEntry->bUnlocked)
		{
			return EPortUpgradeOfferActionBlockReason::AlreadyUnlocked;
		}

		if (!OfferEntry->bVisitGateSatisfied)
		{
			return EPortUpgradeOfferActionBlockReason::VisitRequirementNotMet;
		}

		if (!OfferEntry->bAffordable)
		{
			return EPortUpgradeOfferActionBlockReason::InsufficientCredits;
		}
	}

	if (BoardData.UpgradeAvailabilityReason != EPortUpgradeAvailabilityReason::Ready)
	{
		return EPortUpgradeOfferActionBlockReason::UpgradeAvailabilityBlocked;
	}

	return EPortUpgradeOfferActionBlockReason::None;
}

FText UPortMissionBoardWidget::BuildUpgradeOfferActionBlockReasonText(
	EPortUpgradeOfferActionBlockReason BlockReason,
	const FPortMissionBoardData& BoardData,
	FName RequestedUpgradeId)
{
	const FPortUpgradeOfferEntry* OfferEntry = BoardData.OfferedUpgrades.FindByPredicate(
		[RequestedUpgradeId](const FPortUpgradeOfferEntry& Entry)
		{
			return Entry.UpgradeId == RequestedUpgradeId;
		});

	switch (BlockReason)
	{
	case EPortUpgradeOfferActionBlockReason::None:
		return FText::GetEmpty();
	case EPortUpgradeOfferActionBlockReason::InvalidUpgrade:
		return FText::FromString(TEXT("Ingen oppgradering valgt."));
	case EPortUpgradeOfferActionBlockReason::UpgradeServiceDisabled:
		return BuildUpgradeAvailabilityStatusText(EPortUpgradeAvailabilityReason::ServiceUnavailable, 0);
	case EPortUpgradeOfferActionBlockReason::NotOfferedAtPort:
		return FText::FromString(TEXT("Oppgraderingen tilbys ikke i denne havnen."));
	case EPortUpgradeOfferActionBlockReason::AlreadyUnlocked:
		return FText::FromString(TEXT("Oppgraderingen er allerede opplåst."));
	case EPortUpgradeOfferActionBlockReason::VisitRequirementNotMet:
		if (OfferEntry && !OfferEntry->VisitRequirementStatus.IsEmpty())
		{
			return OfferEntry->VisitRequirementStatus;
		}
		return FText::FromString(TEXT("Portbesøkskravet er ikke oppfylt ennå."));
	case EPortUpgradeOfferActionBlockReason::InsufficientCredits:
		return FText::FromString(FString::Printf(
			TEXT("Ikke nok kreditter (%d kreves)."),
			FMath::Max(0, OfferEntry ? OfferEntry->CreditCost : 0)));
	case EPortUpgradeOfferActionBlockReason::UpgradeAvailabilityBlocked:
		return BuildUpgradeAvailabilityStatusText(BoardData.UpgradeAvailabilityReason, 0);
	default:
		return FText::FromString(TEXT("Oppgraderingen kan ikke kjøpes akkurat nå."));
	}
}

bool UPortMissionBoardWidget::CanRequestRepairService(
	const FPortMissionBoardData& BoardData,
	FText& OutBlockedReason)
{
	if (!BoardData.bSupportsRepairService)
	{
		OutBlockedReason = BoardData.RepairStatus.IsEmpty()
			? FText::FromString(TEXT("Reparasjon er ikke tilgjengelig i denne havnen."))
			: BoardData.RepairStatus;
		return false;
	}

	if (FMath::Clamp(BoardData.CurrentBoatConditionPercent, 0, 100) >= 100)
	{
		OutBlockedReason = FText::FromString(TEXT("Båten er allerede i topp stand."));
		return false;
	}

	if (!BoardData.bCanAffordRepair)
	{
		OutBlockedReason = BoardData.RepairStatus.IsEmpty()
			? FText::FromString(TEXT("Ikke nok kreditter til reparasjon."))
			: BoardData.RepairStatus;
		return false;
	}

	OutBlockedReason = FText::GetEmpty();
	return true;
}

bool UPortMissionBoardWidget::CanRequestManualRefresh(
	const FPortMissionBoardData& BoardData,
	FText& OutBlockedReason)
{
	if (!BoardData.bSupportsManualRefresh)
	{
		OutBlockedReason = BuildManualRefreshStatusText(false, false, 0.0f, BoardData.ManualRefreshCreditCost, true);
		return false;
	}

	if (BoardData.bManualRefreshOnCooldown)
	{
		OutBlockedReason = BuildManualRefreshStatusText(
			true,
			true,
			BoardData.ManualRefreshCooldownRemainingSeconds,
			BoardData.ManualRefreshCreditCost,
			BoardData.bCanAffordManualRefresh);
		return false;
	}

	if (!BoardData.bCanAffordManualRefresh)
	{
		OutBlockedReason = BuildManualRefreshStatusText(
			true,
			false,
			0.0f,
			BoardData.ManualRefreshCreditCost,
			false);
		return false;
	}

	OutBlockedReason = FText::GetEmpty();
	return true;
}

FPortMissionBoardData UPortMissionBoardWidget::BuildActionStateAnnotatedBoardData(const FPortMissionBoardData& BoardData)
{
	FPortMissionBoardData Result = BoardData;
	Result.SelectableMissionOfferCount = 0;
	Result.BlockedMissionOfferCount = 0;
	Result.PurchasableUpgradeOfferCount = 0;
	Result.BlockedUpgradeOfferCount = 0;

	for (FPortMissionOfferEntry& OfferEntry : Result.OfferedMissions)
	{
		OfferEntry.SelectionBlockedReasonType = DetermineMissionOfferActionBlockReason(Result, OfferEntry.MissionId);
		OfferEntry.bSelectable = OfferEntry.SelectionBlockedReasonType == EPortMissionOfferActionBlockReason::None;
		OfferEntry.SelectionBlockedReason = BuildMissionOfferActionBlockReasonText(
			OfferEntry.SelectionBlockedReasonType,
			Result,
			OfferEntry.MissionId);
		if (OfferEntry.bSelectable)
		{
			Result.SelectableMissionOfferCount++;
		}
		else
		{
			Result.BlockedMissionOfferCount++;
		}
	}

	for (FPortUpgradeOfferEntry& OfferEntry : Result.OfferedUpgrades)
	{
		OfferEntry.PurchaseBlockedReasonType = DetermineUpgradeOfferActionBlockReason(Result, OfferEntry.UpgradeId);
		OfferEntry.bPurchasable = OfferEntry.PurchaseBlockedReasonType == EPortUpgradeOfferActionBlockReason::None;
		OfferEntry.PurchaseBlockedReason = BuildUpgradeOfferActionBlockReasonText(
			OfferEntry.PurchaseBlockedReasonType,
			Result,
			OfferEntry.UpgradeId);
		if (OfferEntry.bPurchasable)
		{
			Result.PurchasableUpgradeOfferCount++;
		}
		else
		{
			Result.BlockedUpgradeOfferCount++;
		}
	}

	Result.bHasSelectableMissionOffers = Result.SelectableMissionOfferCount > 0;
	Result.bHasPurchasableUpgradeOffers = Result.PurchasableUpgradeOfferCount > 0;
	Result.bHasAnyImmediateActions = Result.bHasSelectableMissionOffers || Result.bHasPurchasableUpgradeOffers;
	Result.OfferActionSummaryStatus = BuildOfferActionSummaryStatusText(Result);
	Result.PrimaryActionHint = DeterminePrimaryActionHint(Result);
	Result.PrimaryActionHintStatus = BuildPrimaryActionHintStatusText(Result.PrimaryActionHint);
	return Result;
}

FText UPortMissionBoardWidget::BuildOfferActionSummaryStatusText(const FPortMissionBoardData& BoardData)
{
	const int32 SelectableMissionCount = FMath::Max(0, BoardData.SelectableMissionOfferCount);
	const int32 BlockedMissionCount = FMath::Max(0, BoardData.BlockedMissionOfferCount);
	const int32 PurchasableUpgradeCount = FMath::Max(0, BoardData.PurchasableUpgradeOfferCount);
	const int32 BlockedUpgradeCount = FMath::Max(0, BoardData.BlockedUpgradeOfferCount);

	if (BoardData.bSupportsMissionBoard && BoardData.bSupportsUpgradeService)
	{
		return FText::FromString(FString::Printf(
			TEXT("Valgbare oppdrag: %d (%d blokkert) | Kjøpbare oppgraderinger: %d (%d blokkert)"),
			SelectableMissionCount,
			BlockedMissionCount,
			PurchasableUpgradeCount,
			BlockedUpgradeCount));
	}

	if (BoardData.bSupportsMissionBoard)
	{
		return FText::FromString(FString::Printf(
			TEXT("Valgbare oppdrag: %d (%d blokkert)"),
			SelectableMissionCount,
			BlockedMissionCount));
	}

	if (BoardData.bSupportsUpgradeService)
	{
		return FText::FromString(FString::Printf(
			TEXT("Kjøpbare oppgraderinger: %d (%d blokkert)"),
			PurchasableUpgradeCount,
			BlockedUpgradeCount));
	}

	return FText::FromString(TEXT("Ingen handlinger tilgjengelig i denne havnen."));
}

EPortBoardPrimaryActionHint UPortMissionBoardWidget::DeterminePrimaryActionHint(const FPortMissionBoardData& BoardData)
{
	const bool bHasSelectableMissionOffers = BoardData.bHasSelectableMissionOffers || BoardData.SelectableMissionOfferCount > 0;
	const bool bHasPurchasableUpgradeOffers = BoardData.bHasPurchasableUpgradeOffers || BoardData.PurchasableUpgradeOfferCount > 0;

	if (bHasSelectableMissionOffers && bHasPurchasableUpgradeOffers)
	{
		return EPortBoardPrimaryActionHint::SelectMissionOrBuyUpgrade;
	}

	if (bHasSelectableMissionOffers)
	{
		return EPortBoardPrimaryActionHint::SelectMission;
	}

	if (bHasPurchasableUpgradeOffers)
	{
		return EPortBoardPrimaryActionHint::BuyUpgrade;
	}

	return EPortBoardPrimaryActionHint::None;
}

FText UPortMissionBoardWidget::BuildPrimaryActionHintStatusText(EPortBoardPrimaryActionHint Hint)
{
	switch (Hint)
	{
	case EPortBoardPrimaryActionHint::SelectMission:
		return FText::FromString(TEXT("Anbefalt neste steg: Velg et oppdrag."));
	case EPortBoardPrimaryActionHint::BuyUpgrade:
		return FText::FromString(TEXT("Anbefalt neste steg: Kjøp en oppgradering."));
	case EPortBoardPrimaryActionHint::SelectMissionOrBuyUpgrade:
		return FText::FromString(TEXT("Anbefalt neste steg: Velg oppdrag eller kjøp oppgradering."));
	case EPortBoardPrimaryActionHint::None:
	default:
		return FText::FromString(TEXT("Ingen umiddelbare handlinger tilgjengelig."));
	}
}

void UPortMissionBoardWidget::RequestCloseBoard()
{
	OnCloseRequested.Broadcast();
}

void UPortMissionBoardWidget::RequestRefreshBoard()
{
	FText BlockedReason;
	if (!CanRequestManualRefresh(LastData, BlockedReason))
	{
		if (!BlockedReason.IsEmpty())
		{
			OnActionBlocked.Broadcast(EPortBoardActionType::ManualRefresh, BlockedReason);
		}
		return;
	}

	OnRefreshRequested.Broadcast();
}

void UPortMissionBoardWidget::RequestRepairService()
{
	FText BlockedReason;
	if (!CanRequestRepairService(LastData, BlockedReason))
	{
		if (!BlockedReason.IsEmpty())
		{
			OnActionBlocked.Broadcast(EPortBoardActionType::RepairService, BlockedReason);
		}
		return;
	}

	OnRepairRequested.Broadcast();
}

void UPortMissionBoardWidget::RequestPurchaseUpgrade(FName UpgradeId)
{
	FText BlockedReason;
	if (!CanRequestUpgradePurchase(LastData, UpgradeId, BlockedReason))
	{
		if (!BlockedReason.IsEmpty())
		{
			OnActionBlocked.Broadcast(EPortBoardActionType::UpgradePurchase, BlockedReason);
		}
		return;
	}

	OnUpgradePurchaseRequested.Broadcast(UpgradeId);
}
