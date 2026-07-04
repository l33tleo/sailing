#pragma once

#include "CoreMinimal.h"

class UProceduralMeshComponent;

/** Shared helpers for building filled land polygons (islands + mainland) from
 *  2D outline rings, used by AIslandActor and AFjordCoastlineActor. */
namespace FjordGeometry
{
	/**
	 * Bilinear elevation sampler backed by a normalized uint16 heightfield produced by
	 * scripts/fetch_oslofjord_terrain.py (Content/Fjord/Terrain/oslofjord_dtm.r16 + .json).
	 * Coordinates are Unreal world units (meters × DistanceScale); sampling maps world XY
	 * into the grid via the stored bbox. Load once and share (TSharedPtr) across actors.
	 */
	struct SAILING_API FHeightGrid
	{
		int32 Width = 0;
		int32 Height = 0;
		float MinMeters = 0.0f;
		float MaxMeters = 0.0f;
		// Bbox in Unreal world units.
		float MinX = 0.0f;
		float MinY = 0.0f;
		float MaxX = 0.0f;
		float MaxY = 0.0f;
		bool bRow0IsNorth = true;
		TArray<uint16> Samples;

		bool IsValid() const { return Width > 1 && Height > 1 && Samples.Num() == Width * Height; }

		/** Elevation in meters at Unreal world (X, Y). Clamped at the edges. */
		float SampleMeters(float WorldX, float WorldY) const;

		/** Load from Content/Fjord/Terrain/<Name>.r16 + .json. Returns null on failure. */
		static TSharedPtr<FHeightGrid> Load(const FString& Name = TEXT("oslofjord_dtm"));
	};

	/**
	 * Triangulate a closed 2D ring and add it as a flat filled section (a top face at
	 * TopZ plus an optional vertical skirt down to TopZ - SkirtDepth so the land is not
	 * paper-thin when seen from the water).
	 *
	 * @param Ring        Closed outline (first point may or may not equal last; duplicates handled).
	 * @param TopZ        Z of the top (land) surface.
	 * @param SkirtDepth  Height of the vertical edge wall (0 = flat, no skirt).
	 * @param Section     Procedural mesh section index to create.
	 * @param Color       Vertex color for the whole section.
	 * @return true if a section was created.
	 */
	SAILING_API bool BuildFilledPolygon(
		UProceduralMeshComponent* Mesh,
		const TArray<FVector2D>& Ring,
		float TopZ,
		float SkirtDepth,
		int32 Section,
		const FColor& Color);

	/**
	 * Like BuildFilledPolygon but displaces the surface using real elevation from a height
	 * grid, giving the land relief. The polygon keeps its exact OSM shore (shore + skirt
	 * follow the ring); each triangle is uniformly subdivided so interior points sample the
	 * DTM too, so real inland hills appear, not just shore height.
	 *
	 * @param Ring          Closed outline in actor-local units.
	 * @param Grid          Elevation source (sampled in world space).
	 * @param WorldOrigin   Actor world location; world sample point = WorldOrigin + local.
	 * @param BaseZ         Local Z corresponding to sea level (0 m elevation).
	 * @param HeightScale   Meters → local Z (typically DistanceScale × HeightExaggeration).
	 * @param SkirtDepth    Vertical wall depth below sea level (relative to BaseZ).
	 * @param Section       Procedural mesh section index.
	 * @param Color         Vertex color.
	 * @return true if a section was created.
	 */
	SAILING_API bool BuildTerrainPolygon(
		UProceduralMeshComponent* Mesh,
		const TArray<FVector2D>& Ring,
		const FHeightGrid& Grid,
		const FVector2D& WorldOrigin,
		float BaseZ,
		float HeightScale,
		float SkirtDepth,
		int32 Section,
		const FColor& Color);
}
