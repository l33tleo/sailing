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

void UPortMissionBoardWidget::RequestCloseBoard()
{
	OnCloseRequested.Broadcast();
}

void UPortMissionBoardWidget::RequestRepairService()
{
	OnRepairRequested.Broadcast();
}
