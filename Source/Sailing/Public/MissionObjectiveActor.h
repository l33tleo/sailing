#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Data/SailingMissionDataAsset.h"
#include "MissionObjectiveActor.generated.h"

class USphereComponent;

/**
 * World objective marker that can complete the currently active mission on overlap.
 */
UCLASS()
class SAILING_API AMissionObjectiveActor : public AActor
{
	GENERATED_BODY()

public:
	AMissionObjectiveActor();
	virtual void BeginPlay() override;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UStaticMeshComponent> MarkerMesh;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<USphereComponent> ObjectiveTrigger;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mission")
	ESailingMissionType TriggerType = ESailingMissionType::Delivery;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mission")
	bool bDestroyAfterCompletion = true;

	UFUNCTION(BlueprintCallable, Category = "Mission")
	void SetTriggerType(ESailingMissionType InTriggerType) { TriggerType = InTriggerType; }

private:
	UFUNCTION()
	void OnObjectiveOverlap(UPrimitiveComponent* OverlappedComp, AActor* OtherActor,
		UPrimitiveComponent* OtherComp, int32 OtherBodyIndex,
		bool bFromSweep, const FHitResult& SweepResult);
};
