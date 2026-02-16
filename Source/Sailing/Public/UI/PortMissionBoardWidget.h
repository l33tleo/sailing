#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Data/SailingProgressionTypes.h"
#include "PortMissionBoardWidget.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnPortMissionAcceptRequested, FName, MissionId);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnPortMissionBoardCloseRequested);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnPortRepairRequested);

USTRUCT(BlueprintType)
struct SAILING_API FPortMissionOfferEntry
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "MissionBoard")
	FName MissionId = NAME_None;

	UPROPERTY(BlueprintReadOnly, Category = "MissionBoard")
	FText MissionTitle;
};

USTRUCT(BlueprintType)
struct SAILING_API FPortMissionBoardData
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "MissionBoard")
	FName PortId = NAME_None;

	UPROPERTY(BlueprintReadOnly, Category = "MissionBoard")
	FText PortDisplayName;

	UPROPERTY(BlueprintReadOnly, Category = "MissionBoard")
	TArray<FName> OfferedMissionIds;

	UPROPERTY(BlueprintReadOnly, Category = "MissionBoard")
	TArray<FPortMissionOfferEntry> OfferedMissions;

	UPROPERTY(BlueprintReadOnly, Category = "MissionBoard")
	FName CurrentMissionId = NAME_None;

	UPROPERTY(BlueprintReadOnly, Category = "MissionBoard")
	bool bMissionBoardOnCooldown = false;

	UPROPERTY(BlueprintReadOnly, Category = "MissionBoard")
	float CooldownRemainingSeconds = 0.0f;

	UPROPERTY(BlueprintReadOnly, Category = "MissionBoard")
	TArray<FMissionBoardSelectionEntry> RecentSelections;

	UPROPERTY(BlueprintReadOnly, Category = "MissionBoard")
	bool bHasAnyOffers = false;

	UPROPERTY(BlueprintReadOnly, Category = "MissionBoard")
	FText AvailabilityStatus;

	UPROPERTY(BlueprintReadOnly, Category = "MissionBoard")
	bool bAwaitingMissionSwitchConfirmation = false;

	UPROPERTY(BlueprintReadOnly, Category = "MissionBoard")
	FName PendingMissionSwitchId = NAME_None;

	UPROPERTY(BlueprintReadOnly, Category = "MissionBoard")
	FText MissionSwitchConfirmationStatus;

	UPROPERTY(BlueprintReadOnly, Category = "MissionBoard|Service")
	bool bSupportsRepairService = false;

	UPROPERTY(BlueprintReadOnly, Category = "MissionBoard|Service")
	int32 CurrentBoatConditionPercent = 100;

	UPROPERTY(BlueprintReadOnly, Category = "MissionBoard|Service")
	int32 EstimatedRepairCostCredits = 0;

	UPROPERTY(BlueprintReadOnly, Category = "MissionBoard|Service")
	bool bCanAffordRepair = false;

	UPROPERTY(BlueprintReadOnly, Category = "MissionBoard|Service")
	FText RepairStatus;
};

/**
 * Optional UMG widget hook for a lightweight mission board at ports.
 */
UCLASS(BlueprintType, Blueprintable)
class SAILING_API UPortMissionBoardWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category = "MissionBoard")
	void PushMissionBoardData(const FPortMissionBoardData& InData);

	UFUNCTION(BlueprintCallable, Category = "MissionBoard")
	void RequestAcceptMission(FName MissionId);

	UFUNCTION(BlueprintPure, Category = "MissionBoard")
	static bool RequiresMissionSwitchConfirmation(FName CurrentMissionId, FName RequestedMissionId, FName PendingConfirmationMissionId);

	UFUNCTION(BlueprintCallable, Category = "MissionBoard")
	void RequestCloseBoard();

	UFUNCTION(BlueprintCallable, Category = "MissionBoard|Service")
	void RequestRepairService();

	UFUNCTION(BlueprintPure, Category = "MissionBoard")
	const FPortMissionBoardData& GetLastData() const { return LastData; }

	UPROPERTY(BlueprintAssignable, Category = "MissionBoard")
	FOnPortMissionAcceptRequested OnAcceptMissionRequested;

	UPROPERTY(BlueprintAssignable, Category = "MissionBoard")
	FOnPortMissionBoardCloseRequested OnCloseRequested;

	UPROPERTY(BlueprintAssignable, Category = "MissionBoard|Service")
	FOnPortRepairRequested OnRepairRequested;

protected:
	UFUNCTION(BlueprintImplementableEvent, Category = "MissionBoard")
	void OnMissionBoardDataUpdated(const FPortMissionBoardData& InData);

private:
	UPROPERTY(BlueprintReadOnly, Category = "MissionBoard", meta = (AllowPrivateAccess = "true"))
	FPortMissionBoardData LastData;
};
