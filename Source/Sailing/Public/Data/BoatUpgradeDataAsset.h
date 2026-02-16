#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "BoatUpgradeDataAsset.generated.h"

/**
 * Economy-driven upgrade definition for progression.
 */
UCLASS(BlueprintType)
class SAILING_API UBoatUpgradeDataAsset : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Upgrade")
	FName UpgradeId = NAME_None;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Upgrade")
	FText DisplayName;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Upgrade")
	FText Description;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Upgrade", meta = (ClampMin = "0"))
	int32 CreditCost = 1000;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Upgrade", meta = (ClampMin = "0.1"))
	float MaxSpeedMultiplier = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Upgrade", meta = (ClampMin = "0.1"))
	float DragMultiplier = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Upgrade", meta = (ClampMin = "0.1"))
	float TurnRateMultiplier = 1.0f;
};
