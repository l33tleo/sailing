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

void UPortMissionBoardWidget::RequestCloseBoard()
{
	OnCloseRequested.Broadcast();
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
