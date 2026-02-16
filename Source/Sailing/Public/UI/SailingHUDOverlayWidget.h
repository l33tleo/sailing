#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "SailingHUDOverlayWidget.generated.h"

USTRUCT(BlueprintType)
struct SAILING_API FSailingHUDOverlayData
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "HUD")
	int32 DiscoveredIslands = 0;

	UPROPERTY(BlueprintReadOnly, Category = "HUD")
	int32 Credits = 0;

	UPROPERTY(BlueprintReadOnly, Category = "HUD")
	FName ActiveMissionId = NAME_None;

	UPROPERTY(BlueprintReadOnly, Category = "HUD")
	FText ActiveMissionTitle;

	UPROPERTY(BlueprintReadOnly, Category = "HUD")
	float ObjectiveDistanceMeters = -1.0f;

	UPROPERTY(BlueprintReadOnly, Category = "HUD")
	int32 BoatConditionPercent = 100;
};

/**
 * UMG migration scaffolding for the current canvas HUD.
 * Blueprint children can render modern HUD using pushed overlay data.
 */
UCLASS(BlueprintType, Blueprintable)
class SAILING_API USailingHUDOverlayWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category = "HUD")
	void PushOverlayData(const FSailingHUDOverlayData& InOverlayData);

	UFUNCTION(BlueprintPure, Category = "HUD")
	const FSailingHUDOverlayData& GetLastOverlayData() const { return LastOverlayData; }

protected:
	UFUNCTION(BlueprintImplementableEvent, Category = "HUD")
	void OnOverlayDataUpdated(const FSailingHUDOverlayData& InOverlayData);

private:
	UPROPERTY(BlueprintReadOnly, Category = "HUD", meta = (AllowPrivateAccess = "true"))
	FSailingHUDOverlayData LastOverlayData;
};
