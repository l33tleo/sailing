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
