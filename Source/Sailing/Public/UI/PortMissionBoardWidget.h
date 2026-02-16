#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Data/SailingProgressionTypes.h"
#include "PortMissionBoardWidget.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnPortMissionAcceptRequested, FName, MissionId);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnPortMissionBoardCloseRequested);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnPortRepairRequested);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnPortUpgradePurchaseRequested, FName, UpgradeId);

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
struct SAILING_API FPortUpgradeOfferEntry
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "MissionBoard|Service")
	FName UpgradeId = NAME_None;

	UPROPERTY(BlueprintReadOnly, Category = "MissionBoard|Service")
	FText UpgradeTitle;

	UPROPERTY(BlueprintReadOnly, Category = "MissionBoard|Service")
	int32 CreditCost = 0;

	UPROPERTY(BlueprintReadOnly, Category = "MissionBoard|Service")
	int32 BaseCreditCost = 0;

	UPROPERTY(BlueprintReadOnly, Category = "MissionBoard|Service")
	bool bUnlocked = false;

	UPROPERTY(BlueprintReadOnly, Category = "MissionBoard|Service")
	bool bAffordable = false;

	UPROPERTY(BlueprintReadOnly, Category = "MissionBoard|Service")
	float PriorityWeight = 1.0f;

	UPROPERTY(BlueprintReadOnly, Category = "MissionBoard|Service")
	int32 MinPortVisits = 0;

	UPROPERTY(BlueprintReadOnly, Category = "MissionBoard|Service")
	bool bVisitGateSatisfied = true;
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
	bool bSupportsMissionBoard = true;

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
	int32 PortVisitCount = 0;

	UPROPERTY(BlueprintReadOnly, Category = "MissionBoard")
	bool bAwaitingMissionSwitchConfirmation = false;

	UPROPERTY(BlueprintReadOnly, Category = "MissionBoard")
	FName PendingMissionSwitchId = NAME_None;

	UPROPERTY(BlueprintReadOnly, Category = "MissionBoard")
	FText MissionSwitchConfirmationStatus;

	UPROPERTY(BlueprintReadOnly, Category = "MissionBoard|Service")
	bool bSupportsUpgradeService = false;

	UPROPERTY(BlueprintReadOnly, Category = "MissionBoard|Service")
	TArray<FName> OfferedUpgradeIds;

	UPROPERTY(BlueprintReadOnly, Category = "MissionBoard|Service")
	TArray<FPortUpgradeOfferEntry> OfferedUpgrades;

	UPROPERTY(BlueprintReadOnly, Category = "MissionBoard|Service")
	bool bHasWeightedUpgradeRules = false;

	UPROPERTY(BlueprintReadOnly, Category = "MissionBoard|Service")
	FText UpgradeStatus;

	UPROPERTY(BlueprintReadOnly, Category = "MissionBoard|Service")
	float UpgradeCostMultiplier = 1.0f;

	UPROPERTY(BlueprintReadOnly, Category = "MissionBoard|Service")
	FText UpgradePricingStatus;

	UPROPERTY(BlueprintReadOnly, Category = "MissionBoard|Service")
	bool bAwaitingUpgradePurchaseConfirmation = false;

	UPROPERTY(BlueprintReadOnly, Category = "MissionBoard|Service")
	FName PendingUpgradePurchaseId = NAME_None;

	UPROPERTY(BlueprintReadOnly, Category = "MissionBoard|Service")
	FText UpgradePurchaseConfirmationStatus;

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

	UFUNCTION(BlueprintPure, Category = "MissionBoard|Service")
	static bool IsUpgradePurchaseRequestValid(bool bSupportsUpgradeService, const TArray<FName>& InOfferedUpgradeIds, FName RequestedUpgradeId);

	UFUNCTION(BlueprintPure, Category = "MissionBoard|Service")
	static bool RequiresUpgradePurchaseConfirmation(FName RequestedUpgradeId, FName PendingUpgradePurchaseId);

	UFUNCTION(BlueprintPure, Category = "MissionBoard|Service")
	static FText BuildUpgradePricingStatusText(bool bSupportsUpgradeService, float UpgradeCostMultiplier);

	UFUNCTION(BlueprintCallable, Category = "MissionBoard")
	void RequestCloseBoard();

	UFUNCTION(BlueprintCallable, Category = "MissionBoard|Service")
	void RequestRepairService();

	UFUNCTION(BlueprintCallable, Category = "MissionBoard|Service")
	void RequestPurchaseUpgrade(FName UpgradeId);

	UFUNCTION(BlueprintPure, Category = "MissionBoard")
	const FPortMissionBoardData& GetLastData() const { return LastData; }

	UPROPERTY(BlueprintAssignable, Category = "MissionBoard")
	FOnPortMissionAcceptRequested OnAcceptMissionRequested;

	UPROPERTY(BlueprintAssignable, Category = "MissionBoard")
	FOnPortMissionBoardCloseRequested OnCloseRequested;

	UPROPERTY(BlueprintAssignable, Category = "MissionBoard|Service")
	FOnPortRepairRequested OnRepairRequested;

	UPROPERTY(BlueprintAssignable, Category = "MissionBoard|Service")
	FOnPortUpgradePurchaseRequested OnUpgradePurchaseRequested;

protected:
	UFUNCTION(BlueprintImplementableEvent, Category = "MissionBoard")
	void OnMissionBoardDataUpdated(const FPortMissionBoardData& InData);

private:
	UPROPERTY(BlueprintReadOnly, Category = "MissionBoard", meta = (AllowPrivateAccess = "true"))
	FPortMissionBoardData LastData;
};
