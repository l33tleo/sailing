#include "UI/SailingHUDOverlayWidget.h"

void USailingHUDOverlayWidget::PushOverlayData(const FSailingHUDOverlayData& InOverlayData)
{
	LastOverlayData = InOverlayData;
	OnOverlayDataUpdated(InOverlayData);
}
