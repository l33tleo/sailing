#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "WindActor.generated.h"

UCLASS()
class SAILING_API AWindActor : public AActor
{
	GENERATED_BODY()

public:
	AWindActor();

	virtual void Tick(float DeltaTime) override;

	UFUNCTION(BlueprintPure, Category = "Wind")
	FVector GetWindDirection() const;

	UFUNCTION(BlueprintPure, Category = "Wind")
	float GetWindStrength() const;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wind")
	FVector WindDirection = FVector(1.0f, 0.0f, 0.0f);

	/** Basestyrke (gjennomsnittlig vind). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wind", meta = (ClampMin = "0"))
	float BaseWindStrength = 1000.0f;

	/** Amplitude for kast (0 = ingen variasjon). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wind|Gusts", meta = (ClampMin = "0"))
	float GustStrength = 0.0f;

	/** Frekvens for kast (sykluser per sekund). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wind|Gusts", meta = (ClampMin = "0.01", ClampMax = "2"))
	float GustFrequency = 0.2f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wind")
	float WindRotationRate = 5.0f;

	/** Svak retningsvariasjon: amplitude i grader. 0 = av. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wind|Oscillation", meta = (ClampMin = "0", ClampMax = "30"))
	float DirectionOscillationAngle = 0.0f;

	/** Frekvens for retningsoscillasjon (sykluser per sekund). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wind|Oscillation", meta = (ClampMin = "0.01", ClampMax = "1"))
	float DirectionOscillationFrequency = 0.1f;
};
