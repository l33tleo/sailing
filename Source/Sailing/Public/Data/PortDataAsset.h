#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "PortDataAsset.generated.h"

USTRUCT(BlueprintType)
struct SAILING_API FPortMissionWeightedOffer
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Port|MissionBoard")
	FName MissionId = NAME_None;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Port|MissionBoard", meta = (ClampMin = "0.0"))
	float PriorityWeight = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Port|MissionBoard", meta = (ClampMin = "0"))
	int32 MinPortVisits = 0;
};

USTRUCT(BlueprintType)
struct SAILING_API FPortUpgradeWeightedOffer
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Port|Services")
	FName UpgradeId = NAME_None;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Port|Services", meta = (ClampMin = "0.0"))
	float PriorityWeight = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Port|Services", meta = (ClampMin = "0"))
	int32 MinPortVisits = 0;
};

USTRUCT(BlueprintType)
struct SAILING_API FPortUpgradeOfferSelectionResult
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Port|Services")
	TArray<FName> UpgradeIds;

	UPROPERTY(BlueprintReadOnly, Category = "Port|Services")
	bool bUsedWeightedRules = false;

	UPROPERTY(BlueprintReadOnly, Category = "Port|Services")
	bool bUsedFallbackOffers = false;

	UPROPERTY(BlueprintReadOnly, Category = "Port|Services")
	int32 EligibleWeightedRuleCount = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Port|Services")
	int32 VisitGatedRuleCount = 0;
};

USTRUCT(BlueprintType)
struct SAILING_API FPortMissionOfferSelectionResult
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Port|MissionBoard")
	TArray<FName> MissionIds;

	UPROPERTY(BlueprintReadOnly, Category = "Port|MissionBoard")
	bool bUsedWeightedRules = false;

	UPROPERTY(BlueprintReadOnly, Category = "Port|MissionBoard")
	bool bUsedFallbackOffers = false;

	UPROPERTY(BlueprintReadOnly, Category = "Port|MissionBoard")
	int32 EligibleWeightedRuleCount = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Port|MissionBoard")
	int32 VisitGatedRuleCount = 0;
};

/**
 * Data-driven harbor definition for world feature spawning.
 */
UCLASS(BlueprintType)
class SAILING_API UPortDataAsset : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Port")
	FName PortId = NAME_None;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Port")
	FText DisplayName;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Port")
	FVector WorldLocation = FVector::ZeroVector;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Port")
	int32 DockBonusCredits = 75;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Port")
	bool bGrantOneTimeDockBonus = true;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Port")
	bool bAutoRepairAtPort = true;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Port", meta = (ClampMin = "0"))
	int32 RepairCostPerPercentPoint = 1;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Port|MissionBoard")
	bool bOfferMissionBoard = true;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Port|MissionBoard")
	bool bCycleMissionOnDock = false;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Port|MissionBoard")
	bool bRestrictToOfferedMissions = false;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Port|MissionBoard")
	TArray<FName> OfferedMissionIds;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Port|MissionBoard")
	TArray<FPortMissionWeightedOffer> WeightedOfferedMissions;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Port|MissionBoard", meta = (ClampMin = "0", ClampMax = "10"))
	int32 MaxOfferedMissionsAtBoard = 3;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Port|MissionBoard", meta = (ClampMin = "0"))
	float MissionBoardCooldownSeconds = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Port|MissionBoard")
	bool bAllowManualBoardRefresh = true;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Port|MissionBoard", meta = (ClampMin = "0"))
	float ManualBoardRefreshCooldownSeconds = 4.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Port|MissionBoard", meta = (ClampMin = "0"))
	int32 ManualBoardRefreshCreditCost = 0;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Port|Services")
	bool bOfferUpgradeService = false;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Port|Services")
	TArray<FName> OfferedUpgradeIds;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Port|Services")
	TArray<FPortUpgradeWeightedOffer> WeightedOfferedUpgrades;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Port|Services", meta = (ClampMin = "0", ClampMax = "10"))
	int32 MaxOfferedUpgrades = 3;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Port|Services")
	bool bRotateUpgradeStockByVisits = true;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Port|Services")
	bool bHideUnlockedUpgradesOnBoard = false;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Port|Services", meta = (ClampMin = "0.1", ClampMax = "5.0"))
	float UpgradeCostMultiplier = 1.0f;

	static TArray<FName> FilterIdsByAllowedSet(
		const TArray<FName>& InIds,
		const TSet<FName>& InAllowedIds,
		int32& OutRejectedCount);

	static TArray<FPortMissionWeightedOffer> FilterMissionWeightedOffersByAllowedSet(
		const TArray<FPortMissionWeightedOffer>& InOffers,
		const TSet<FName>& InAllowedMissionIds,
		int32& OutRejectedCount);

	static TArray<FPortUpgradeWeightedOffer> FilterUpgradeWeightedOffersByAllowedSet(
		const TArray<FPortUpgradeWeightedOffer>& InOffers,
		const TSet<FName>& InAllowedUpgradeIds,
		int32& OutRejectedCount);

	static TArray<FName> BuildPrioritizedMissionIds(
		const TArray<FPortMissionWeightedOffer>& InWeightedOffers,
		const TArray<FName>& InFallbackOffers,
		int32 PortVisitCount,
		int32 MaxOffers);

	static FPortMissionOfferSelectionResult BuildPrioritizedMissionSelection(
		const TArray<FPortMissionWeightedOffer>& InWeightedOffers,
		const TArray<FName>& InFallbackOffers,
		int32 PortVisitCount,
		int32 MaxOffers);

	static TArray<FName> BuildRotatedUpgradeIds(
		const TArray<FName>& InOfferedUpgradeIds,
		int32 PortVisitCount,
		int32 MaxOffers,
		bool bRotateByVisits);

	static TArray<FName> BuildPrioritizedUpgradeIds(
		const TArray<FPortUpgradeWeightedOffer>& InWeightedOffers,
		const TArray<FName>& InFallbackOffers,
		int32 PortVisitCount,
		int32 MaxOffers,
		bool bRotateByVisits);

	static FPortUpgradeOfferSelectionResult BuildPrioritizedUpgradeSelection(
		const TArray<FPortUpgradeWeightedOffer>& InWeightedOffers,
		const TArray<FName>& InFallbackOffers,
		int32 PortVisitCount,
		int32 MaxOffers,
		bool bRotateByVisits);

	static TArray<FName> FilterUpgradeIdsByUnlockedState(
		const TArray<FName>& InUpgradeIds,
		const TSet<FName>& InUnlockedUpgradeIds,
		bool bHideUnlockedUpgrades);

	static int32 CalculateAdjustedUpgradeCost(int32 BaseCost, float CostMultiplier);
};
