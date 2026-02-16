#include "UI/PortMissionBoardWidget.h"

void UPortMissionBoardWidget::PushMissionBoardData(const FPortMissionBoardData& InData)
{
	LastData = InData;
	OnMissionBoardDataUpdated(InData);
}
