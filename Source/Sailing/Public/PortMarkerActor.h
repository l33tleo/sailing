#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "PortMarkerActor.generated.h"

class UStaticMeshComponent;
class USphereComponent;
class ASailboatPawn;

/**
 * Simple harbor marker actor used as first world-feature registry entry type.
 */
UCLASS()
class SAILING_API APortMarkerActor : public AActor
{
	GENERATED_BODY()

public:
	APortMarkerActor();
	virtual void BeginPlay() override;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UStaticMeshComponent> MarkerMesh;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<USphereComponent> DockTrigger;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Port")
	FName PortId = NAME_None;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Port", meta = (ClampMin = "0"))
	int32 DockBonusCredits = 75;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Port")
	bool bGrantOneTimeDockBonus = true;

private:
	bool bVisitedInSession = false;

	UFUNCTION()
	void OnDockTriggerOverlap(UPrimitiveComponent* OverlappedComp, AActor* OtherActor,
		UPrimitiveComponent* OtherComp, int32 OtherBodyIndex,
		bool bFromSweep, const FHitResult& SweepResult);
};
