#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "FjordGeometry.h"
#include "IslandActor.generated.h"

class USphereComponent;
class UProceduralMeshComponent;
class UMaterialInstanceDynamic;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnIslandDiscovered, AIslandActor*, Island, const FString&, IslandName);

UCLASS()
class SAILING_API AIslandActor : public AActor
{
	GENERATED_BODY()

public:
	AIslandActor();
	virtual void BeginPlay() override;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UStaticMeshComponent> IslandMesh;

	/** Filled polygon land mesh, used for fjord islands with a real outline. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UProceduralMeshComponent> LandMesh;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<USphereComponent> DiscoverySphere;

	/** Island outline in actor-local space (real units), when built from a polygon. */
	UPROPERTY(BlueprintReadOnly, Category = "Island")
	TArray<FVector2D> LocalOutline;

	UPROPERTY(BlueprintReadOnly, Category = "Island")
	bool bDiscovered = false;

	UPROPERTY(BlueprintReadOnly, Category = "Island")
	FString IslandName;

	UPROPERTY(BlueprintReadOnly, Category = "Island")
	FIntPoint ChunkCoord;

	UPROPERTY(BlueprintReadOnly, Category = "Island")
	int32 IslandIndex = 0;

	// Called when island is discovered
	UPROPERTY(BlueprintAssignable, Category = "Island")
	FOnIslandDiscovered OnDiscovered;

	// Initialize island with chunk data (procedural world)
	void InitializeIsland(FIntPoint InChunkCoord, int32 InIslandIndex, bool bWasDiscovered);

	/** Initialize as fjord island with real name (no procedural name). ChunkCoord set to (-1,-1) for save key. */
	void InitializeFjordIsland(const FString& InName, int32 InIndex, bool bWasDiscovered);

	/** Initialize as fjord island and build a filled polygon land mesh from an outline
	 *  given in actor-local space (real units). Sizes the discovery sphere from bounds. */
	void InitializeFjordIslandPolygon(const FString& InName, int32 InIndex, bool bWasDiscovered,
		const TArray<FVector2D>& InLocalOutline);

	/** Provide a shared elevation grid + tuning so the polygon land mesh gets real relief and
	 *  the aerial (M_Land) material. Call before InitializeFjordIslandPolygon. */
	void SetTerrainSource(TSharedPtr<FjordGeometry::FHeightGrid> InGrid,
		float InHeightExaggeration, float InDistanceScale);

	// Mark island as discovered (called from save system or overlap)
	void SetDiscovered(bool bFromSaveGame = false);

	UFUNCTION()
	void OnDiscoveryOverlap(UPrimitiveComponent* OverlappedComp, AActor* OtherActor,
		UPrimitiveComponent* OtherComp, int32 OtherBodyIndex,
		bool bFromSweep, const FHitResult& SweepResult);

private:
	void ApplyDiscoveredMaterial();
	void BuildPolygonMesh();

	// True if the Blender island model was loaded (materials baked in)
	bool bUsingBlenderMesh = false;

	// True when this island is rendered as a filled polygon (LandMesh) rather than IslandMesh
	bool bUsingPolygon = false;

	// Shared elevation grid for relief (null = flat polygon fallback).
	TSharedPtr<FjordGeometry::FHeightGrid> HeightGrid;
	float HeightExaggeration = 1.0f;
	float DistanceScale = 100.0f;

	// Dynamic instance of M_Land so discovery can highlight without losing the aerial texture.
	UPROPERTY()
	TObjectPtr<UMaterialInstanceDynamic> LandMID;
};
