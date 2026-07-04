#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "OceanPlaneActor.generated.h"

class UProceduralMeshComponent;
class UMaterialInstanceDynamic;
class AWindActor;

UCLASS()
class SAILING_API AOceanPlaneActor : public AActor
{
	GENERATED_BODY()

public:
	AOceanPlaneActor();

	virtual void BeginPlay() override;
	virtual void Tick(float DeltaTime) override;

	// Surface layer (top, most transparent)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ocean|Surface")
	FLinearColor SurfaceColor = FLinearColor(0.1f, 0.4f, 0.6f, 0.3f);

	// Shallow layer
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ocean|Shallow")
	FLinearColor ShallowColor = FLinearColor(0.02f, 0.2f, 0.5f, 0.6f);

	// Mid layer
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ocean|Mid")
	FLinearColor MidColor = FLinearColor(0.01f, 0.1f, 0.35f, 0.8f);

	// Deep layer (bottom, most opaque)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ocean|Deep")
	FLinearColor DeepColor = FLinearColor(0.005f, 0.03f, 0.12f, 0.95f);

	// Base water level (Z height of surface)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ocean|Layers")
	float WaterLevel = 100.0f;

	// Layer depths (Z offset from surface)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ocean|Layers")
	float ShallowDepth = -50.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ocean|Layers")
	float MidDepth = -150.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ocean|Layers")
	float DeepDepth = -400.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ocean")
	float WaveAmplitude = 15.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ocean")
	float WaveSpeed = 0.8f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ocean")
	float OceanSize = 200000.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ocean")
	int32 GridResolution = 128;

	// --- Vind-kast på overflaten (cat's paws) ---

	/** Hvor mye mørkere vannet blir i et kast (0 = av, 1 = svart). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ocean|Wind", meta = (ClampMin = "0", ClampMax = "1"))
	float GustDarkening = 0.45f;

	/** Ekstra bølgeamplitude i et kast (choppete vann). 0 = ingen ekstra. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ocean|Wind", meta = (ClampMin = "0", ClampMax = "3"))
	float GustChopBoost = 1.2f;

	/** Oppdater overflate-fargene hvert N. frame (1 = hver frame). Høyere sparer CPU. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ocean|Wind", meta = (ClampMin = "1", ClampMax = "8"))
	int32 ColorUpdateInterval = 2;

private:
	// Surface layer mesh (animated waves)
	UPROPERTY()
	TObjectPtr<UProceduralMeshComponent> SurfaceMesh;

	// Deeper layer meshes (static)
	UPROPERTY()
	TObjectPtr<UProceduralMeshComponent> ShallowMesh;

	UPROPERTY()
	TObjectPtr<UProceduralMeshComponent> MidMesh;

	UPROPERTY()
	TObjectPtr<UProceduralMeshComponent> DeepMesh;

	TArray<FVector> BaseVertices;
	TArray<FVector> Vertices;
	TArray<FVector> Normals;
	TArray<FLinearColor> SurfaceColors;

	void GenerateOceanLayers();
	void GenerateLayerMesh(UProceduralMeshComponent* Mesh, float ZOffset, const FLinearColor& Color, int32 Resolution);
	void UpdateSurfaceWaves(float Time);

	AWindActor* FindWind();

	UPROPERTY()
	TWeakObjectPtr<AWindActor> CachedWind;

	bool bGridGenerated = false;
	int32 FrameCounter = 0;
};
