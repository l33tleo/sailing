#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Data/SailingProgressionTypes.h"
#include "PortMissionBoardWidget.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnPortMissionAcceptRequested, FName, MissionId);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnPortMissionBoardCloseRequested);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnPortRepairRequested);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnPortUpgradePurchaseRequested, FName, UpgradeId);

UENUM(BlueprintType)
enum class EPortMissionAvailabilityReason : uint8
{
	Ready,
	MissionBoardDisabled,
	CooldownActive,
	NoOffers
};

UENUM(BlueprintType)
enum class EPortUpgradeAvailabilityReason : uint8
{
	Ready,
	ServiceUnavailable,
	NoValidOffers,
	NoAffordableOffers
};

UENUM(BlueprintType)
enum class EPortUpgradeOfferSource : uint8
{
	None,
	WeightedRules,
	FallbackList
};

UENUM(BlueprintType)
enum class EPortBoardRefreshContext : uint8
{
	DockArrival,
	CooldownBlocked,
	ServiceOnly,
	MissionSwitchConfirmation,
	UpgradePurchaseConfirmation,
	ManualRefresh
};

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

	UPROPERTY(BlueprintReadOnly, Category = "MissionBoard|Service")
	FText VisitRequirementStatus;
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
	EPortMissionAvailabilityReason AvailabilityReason = EPortMissionAvailabilityReason::Ready;

	UPROPERTY(BlueprintReadOnly, Category = "MissionBoard")
	EPortBoardRefreshContext RefreshContext = EPortBoardRefreshContext::DockArrival;

	UPROPERTY(BlueprintReadOnly, Category = "MissionBoard")
	FText RefreshContextStatus;

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
	EPortUpgradeAvailabilityReason UpgradeAvailabilityReason = EPortUpgradeAvailabilityReason::ServiceUnavailable;

	UPROPERTY(BlueprintReadOnly, Category = "MissionBoard|Service")
	EPortUpgradeOfferSource UpgradeOfferSource = EPortUpgradeOfferSource::None;

	UPROPERTY(BlueprintReadOnly, Category = "MissionBoard|Service")
	int32 EligibleWeightedUpgradeRuleCount = 0;

	UPROPERTY(BlueprintReadOnly, Category = "MissionBoard|Service")
	int32 VisitGatedWeightedUpgradeRuleCount = 0;

	UPROPERTY(BlueprintReadOnly, Category = "MissionBoard|Service")
	int32 HiddenUnlockedUpgradeOfferCount = 0;

	UPROPERTY(BlueprintReadOnly, Category = "MissionBoard|Service")
	FText UpgradeOfferSourceStatus;

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

	UFUNCTION(BlueprintPure, Category = "MissionBoard")
	static EPortMissionAvailabilityReason DetermineMissionAvailabilityReason(
		bool bSupportsMissionBoard,
		bool bMissionBoardOnCooldown,
		bool bHasAnyOffers);

	UFUNCTION(BlueprintPure, Category = "MissionBoard")
	static FText BuildMissionAvailabilityStatusText(
		EPortMissionAvailabilityReason Reason,
		float CooldownRemainingSeconds,
		bool bSupportsUpgradeService);

	UFUNCTION(BlueprintPure, Category = "MissionBoard|Service")
	static EPortUpgradeAvailabilityReason DetermineUpgradeAvailabilityReason(
		bool bSupportsUpgradeService,
		int32 ValidUpgradeOfferCount,
		int32 AffordableUpgradeOfferCount);

	UFUNCTION(BlueprintPure, Category = "MissionBoard|Service")
	static FText BuildUpgradeAvailabilityStatusText(
		EPortUpgradeAvailabilityReason Reason,
		int32 AffordableUpgradeOfferCount);

	UFUNCTION(BlueprintPure, Category = "MissionBoard|Service")
	static EPortUpgradeOfferSource DetermineUpgradeOfferSource(
		bool bSupportsUpgradeService,
		bool bUsedWeightedRules,
		bool bUsedFallbackOffers);

	UFUNCTION(BlueprintPure, Category = "MissionBoard|Service")
	static FText BuildUpgradeOfferSourceStatusText(
		EPortUpgradeOfferSource Source,
		int32 EligibleWeightedRuleCount,
		int32 VisitGatedRuleCount,
		int32 HiddenUnlockedCount);

	UFUNCTION(BlueprintPure, Category = "MissionBoard|Service")
	static FText BuildUpgradeVisitRequirementStatusText(
		int32 MinPortVisits,
		int32 CurrentPortVisitCount);

	UFUNCTION(BlueprintPure, Category = "MissionBoard")
	static FText BuildRefreshContextStatusText(EPortBoardRefreshContext RefreshContext);

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
