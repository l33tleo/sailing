#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "FjordMapData.h"

class UProceduralMeshComponent;

#include "FjordCoastlineActor.generated.h"

UCLASS()
class SAILING_API AFjordCoastlineActor : public AActor
{
	GENERATED_BODY()

public:
	AFjordCoastlineActor();

	virtual void BeginPlay() override;

	/** Fjord map data (coastline points). If null, loaded from FjordMapDataPath. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Fjord")
	TObjectPtr<UFjordMapData> FjordMapData;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Fjord")
	FSoftObjectPath FjordMapDataPath;

	/** Z height of land (above water). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Fjord")
	float LandZ = 150.0f;

	/** Inward extrusion of coast strip in Unreal units (e.g. 500 = 500 m land from coast). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Fjord", meta = (ClampMin = "50"))
	float CoastStripWidth = 500.0f;

private:
	UPROPERTY()
	TObjectPtr<UProceduralMeshComponent> CoastlineMesh;

	void BuildCoastlineMeshFromPolygon(const TArray<FVector2D>& Points);
};
