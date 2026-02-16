#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "PortDataAsset.generated.h"

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
};
