#include "UI/PortMissionBoardWidget.h"

void UPortMissionBoardWidget::PushMissionBoardData(const FPortMissionBoardData& InData)
{
	LastData = InData;
	OnMissionBoardDataUpdated(InData);
}

void UPortMissionBoardWidget::RequestAcceptMission(FName MissionId)
{
	if (MissionId.IsNone())
	{
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

void UPortMissionBoardWidget::RequestCloseBoard()
{
	OnCloseRequested.Broadcast();
}

void UPortMissionBoardWidget::RequestRefreshBoard()
{
	OnRefreshRequested.Broadcast();
}

void UPortMissionBoardWidget::RequestRepairService()
{
	OnRepairRequested.Broadcast();
}

void UPortMissionBoardWidget::RequestPurchaseUpgrade(FName UpgradeId)
{
	if (UpgradeId.IsNone())
	{
		return;
	}

	OnUpgradePurchaseRequested.Broadcast(UpgradeId);
}
