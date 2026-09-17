#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "FjordMapData.generated.h"

class UStaticMesh;
class UFjordIslandBakeData;

/** A single closed polygon ring (world X,Y in Unreal units). Wrapper needed because
 *  UPROPERTY does not support TArray<TArray<>> (e.g. a list of landmass rings). */
USTRUCT(BlueprintType)
struct FFjordRing
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Fjord")
	TArray<FVector2D> Points;
};

/** Definition of a single island in the fjord map (real name, position and outline). */
USTRUCT(BlueprintType)
struct FFjordIslandDef
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Fjord")
	FString Name;

	/** Centroid (world X,Y). Used for the map label and as spawn origin. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Fjord")
	FVector2D Position = FVector2D::ZeroVector;

	/** Fallback uniform scale, used only when Outline is empty. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Fjord", meta = (ClampMin = "0.1"))
	float Scale = 3.0f;

	/** Closed outline polygon (world X,Y). When non-empty, the island is built as a
	 *  filled polygon mesh from this ring instead of the uniform static mesh. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Fjord")
	TArray<FVector2D> Outline;

	/** Offline-bakt terreng (Nanite) fra scripts/bake; pivot = Position, z=0 = middelvannstand.
	 *  Settes av scripts/bake/import_baked_land.py. Tom → prosedural polygon som før. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Fjord")
	TSoftObjectPtr<UStaticMesh> BakedMesh;

	/** Offline-bakt vegetasjon m.m. for øya (scripts/bake/import_vegetation.py). Valgfri. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Fjord")
	TSoftObjectPtr<UFjordIslandBakeData> BakeData;
};

/** Data asset for Oslofjord map: coastline polygon(s) and island definitions. */
UCLASS(BlueprintType)
class SAILING_API UFjordMapData : public UDataAsset
{
	GENERATED_BODY()

public:
	/** Legacy coastline polygon (world X,Y). Superseded by Landmasses; kept for
	 *  backward compatibility and no longer populated by the pipeline. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Fjord")
	TArray<FVector2D> CoastlinePoints;

	/** Mainland as filled polygons (one ring per landmass: west/east shore, Nesodden, ...). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Fjord")
	TArray<FFjordRing> Landmasses;

	/** Islands with real names, positions and outlines (world X,Y). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Fjord")
	TArray<FFjordIslandDef> Islands;

	/** World origin offset (e.g. Oslo harbour). Added to all positions when interpreting data. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Fjord")
	FVector2D WorldOrigin = FVector2D::ZeroVector;

	/** Scale: 1.0 = 1 Unreal unit per meter. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Fjord", meta = (ClampMin = "0.1"))
	float MetersPerUnit = 1.0f;
};
