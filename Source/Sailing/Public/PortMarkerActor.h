#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "PortMarkerActor.generated.h"

class UStaticMeshComponent;

/**
 * Simple harbor marker actor used as first world-feature registry entry type.
 */
UCLASS()
class SAILING_API APortMarkerActor : public AActor
{
	GENERATED_BODY()

public:
	APortMarkerActor();

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UStaticMeshComponent> MarkerMesh;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Port")
	FName PortId = NAME_None;
};
