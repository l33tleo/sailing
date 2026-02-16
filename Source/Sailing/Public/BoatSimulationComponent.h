#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "BoatSimulationComponent.generated.h"

class ASailboatPawn;
class AWindActor;

/**
 * Handles sailboat movement simulation independent from pawn presentation/input.
 * Keeps simulation responsibilities separated from camera and mesh logic.
 */
UCLASS(ClassGroup = (Sailing), meta = (BlueprintSpawnableComponent))
class SAILING_API UBoatSimulationComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UBoatSimulationComponent();

	/** Runs one simulation step using current pawn tuning/input state. */
	void Simulate(ASailboatPawn* OwnerPawn, float DeltaTime);

	float GetCurrentSailForce() const { return CurrentSailForce; }
	float GetCurrentSpeed() const { return CurrentSpeed; }

	void SetCurrentSpeed(float NewSpeed) { CurrentSpeed = FMath::Max(0.0f, NewSpeed); }

private:
	UPROPERTY()
	TWeakObjectPtr<AWindActor> CachedWind;

	float CurrentSailForce = 0.0f;
	float CurrentSpeed = 0.0f;
	float DebugLogTimer = 0.0f;

	AWindActor* FindWind(UWorld* WorldContext);
};
