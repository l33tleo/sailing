#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "FjordMapData.h"
#include "FjordGeometry.h"

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

	/** Z height of land at sea level (above water; kept above the gust wave peak ~169 to avoid z-fighting). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Fjord")
	float LandZ = 190.0f;

	/** Depth of the vertical land edge below the top surface (so land is not paper-thin). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Fjord", meta = (ClampMin = "0"))
	float LandSkirtDepth = 300.0f;

	/** Vertical exaggeration of real DTM elevation (fallback if no FjordMapManager). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Fjord", meta = (ClampMin = "0.1"))
	float HeightExaggeration = 0.4f;

	/** Fallback distance scale (real meters × this = Unreal units) if no FjordMapManager is found. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Fjord", meta = (ClampMin = "0.1"))
	float DistanceScale = 100.0f;

	/** Legacy: inward extrusion of coast strip (only used for old CoastlinePoints data). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Fjord", meta = (ClampMin = "50"))
	float CoastStripWidth = 500.0f;

private:
	UPROPERTY()
	TObjectPtr<UProceduralMeshComponent> CoastlineMesh;

	/** Build filled landmass polygons (rings in real units, scaled by DistanceScale).
	 *  Rings that coincide with a named island are skipped so each island is drawn
	 *  once by its AIslandActor (avoids coplanar z-fighting along shore/island tops). */
	void BuildLandmasses(const TArray<struct FFjordRing>& Landmasses,
	                     const TArray<struct FFjordIslandDef>& Islands,
	                     float InDistanceScale);
	void BuildCoastlineMeshFromPolygon(const TArray<FVector2D>& Points);

	/** Shared elevation grid for relief (null = flat). */
	TSharedPtr<FjordGeometry::FHeightGrid> HeightGrid;
};
