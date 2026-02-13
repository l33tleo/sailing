#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "OceanPlaneActor.generated.h"

class UProceduralMeshComponent;
class UMaterialInstanceDynamic;

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

	void GenerateOceanLayers();
	void GenerateLayerMesh(UProceduralMeshComponent* Mesh, float ZOffset, const FLinearColor& Color, int32 Resolution);
	void UpdateSurfaceWaves(float Time);

	bool bGridGenerated = false;
};
