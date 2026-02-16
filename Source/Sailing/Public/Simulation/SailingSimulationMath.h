#pragma once

#include "CoreMinimal.h"
#include "SailingSimulationMath.generated.h"

USTRUCT(BlueprintType)
struct SAILING_API FSailingPolarSettings
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sailing|WindModel")
	float NoGoZoneAngle = 50.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sailing|WindModel")
	float CloseHauledForce = 0.18f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sailing|WindModel")
	float BeamReachForce = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sailing|WindModel")
	float BroadReachForce = 0.5f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sailing|WindModel")
	float RunningForce = 0.22f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sailing|WindModel")
	float PolarSharpness = 1.0f;
};

/**
 * Pure functions for sailing force model. Kept outside actor classes to simplify testing.
 */
UCLASS()
class SAILING_API USailingSimulationMath : public UObject
{
	GENERATED_BODY()

public:
	/** Returns normalized multiplier [0..1+] from angle-to-wind in degrees. */
	UFUNCTION(BlueprintPure, Category = "Sailing|Simulation")
	static float CalculateForceMultiplier(float AngleToWindDegrees, const FSailingPolarSettings& Settings);

	/** Returns final sail force from apparent wind strength and angle-to-wind. */
	UFUNCTION(BlueprintPure, Category = "Sailing|Simulation")
	static float CalculateSailForce(float ApparentWindStrength, float AngleToWindDegrees, const FSailingPolarSettings& Settings);
};
