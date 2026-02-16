#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "SailingMissionDataAsset.generated.h"

UENUM(BlueprintType)
enum class ESailingMissionType : uint8
{
	Delivery,
	TimeTrial,
	Rescue,
	NavigationChallenge
};

/**
 * Data-driven mission definition used by MissionSubsystem.
 */
UCLASS(BlueprintType)
class SAILING_API USailingMissionDataAsset : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Mission")
	FName MissionId = NAME_None;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Mission")
	FText DisplayName;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Mission")
	FText Description;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Mission")
	ESailingMissionType MissionType = ESailingMissionType::Delivery;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Mission")
	int32 RewardCredits = 500;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Mission")
	float TimeLimitSeconds = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Mission")
	FVector StartWorldLocation = FVector::ZeroVector;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Mission")
	FVector EndWorldLocation = FVector::ZeroVector;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Mission")
	bool bRequireLocationMatch = false;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Mission", meta = (ClampMin = "50"))
	float CompletionRadius = 800.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Mission")
	bool bRepeatable = true;
};
