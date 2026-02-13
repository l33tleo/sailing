#include "OceanPlaneActor.h"
#include "ProceduralMeshComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/Material.h"
#include "UObject/ConstructorHelpers.h"
#include "Kismet/GameplayStatics.h"

AOceanPlaneActor::AOceanPlaneActor()
{
	PrimaryActorTick.bCanEverTick = true;

	// Create root component
	USceneComponent* Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	RootComponent = Root;

	// Create mesh components for each layer (deep to surface for proper transparency sorting)
	DeepMesh = CreateDefaultSubobject<UProceduralMeshComponent>(TEXT("DeepMesh"));
	DeepMesh->SetupAttachment(RootComponent);
	DeepMesh->CastShadow = false;
	DeepMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	MidMesh = CreateDefaultSubobject<UProceduralMeshComponent>(TEXT("MidMesh"));
	MidMesh->SetupAttachment(RootComponent);
	MidMesh->CastShadow = false;
	MidMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	ShallowMesh = CreateDefaultSubobject<UProceduralMeshComponent>(TEXT("ShallowMesh"));
	ShallowMesh->SetupAttachment(RootComponent);
	ShallowMesh->CastShadow = false;
	ShallowMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	SurfaceMesh = CreateDefaultSubobject<UProceduralMeshComponent>(TEXT("SurfaceMesh"));
	SurfaceMesh->SetupAttachment(RootComponent);
	SurfaceMesh->CastShadow = false;
	SurfaceMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
}

void AOceanPlaneActor::GenerateLayerMesh(UProceduralMeshComponent* Mesh, float ZOffset, const FLinearColor& Color, int32 Resolution)
{
	TArray<FVector> LayerVertices;
	TArray<FVector> LayerNormals;
	TArray<int32> Triangles;
	TArray<FVector2D> UVs;
	TArray<FLinearColor> VertexColors;

	const int32 NumVerts = Resolution + 1;
	const float CellSize = OceanSize / Resolution;
	const float HalfSize = OceanSize * 0.5f;

	for (int32 Y = 0; Y <= Resolution; ++Y)
	{
		for (int32 X = 0; X <= Resolution; ++X)
		{
			float PosX = X * CellSize - HalfSize;
			float PosY = Y * CellSize - HalfSize;

			LayerVertices.Add(FVector(PosX, PosY, ZOffset));
			LayerNormals.Add(FVector(0.0f, 0.0f, 1.0f));
			UVs.Add(FVector2D((float)X / Resolution, (float)Y / Resolution));
			VertexColors.Add(Color);
		}
	}

	for (int32 Y = 0; Y < Resolution; ++Y)
	{
		for (int32 X = 0; X < Resolution; ++X)
		{
			int32 TL = Y * NumVerts + X;
			int32 TR = TL + 1;
			int32 BL = TL + NumVerts;
			int32 BR = BL + 1;

			Triangles.Add(TL);
			Triangles.Add(BL);
			Triangles.Add(TR);
			Triangles.Add(TR);
			Triangles.Add(BL);
			Triangles.Add(BR);
		}
	}

	Mesh->CreateMeshSection_LinearColor(0, LayerVertices, Triangles, LayerNormals, UVs,
		VertexColors, TArray<FProcMeshTangent>(), false);
}

void AOceanPlaneActor::GenerateOceanLayers()
{
	// Generate deeper layers with lower resolution (they're less visible)
	// All depths are relative to WaterLevel
	GenerateLayerMesh(DeepMesh, WaterLevel + DeepDepth, DeepColor, GridResolution / 4);
	GenerateLayerMesh(MidMesh, WaterLevel + MidDepth, MidColor, GridResolution / 2);
	GenerateLayerMesh(ShallowMesh, WaterLevel + ShallowDepth, ShallowColor, GridResolution / 2);

	// Surface layer - full resolution with wave animation
	BaseVertices.Empty();
	Vertices.Empty();
	Normals.Empty();
	TArray<int32> Triangles;
	TArray<FVector2D> UVs;
	TArray<FLinearColor> VertexColors;

	const int32 NumVerts = GridResolution + 1;
	const float CellSize = OceanSize / GridResolution;
	const float HalfSize = OceanSize * 0.5f;

	for (int32 Y = 0; Y <= GridResolution; ++Y)
	{
		for (int32 X = 0; X <= GridResolution; ++X)
		{
			float PosX = X * CellSize - HalfSize;
			float PosY = Y * CellSize - HalfSize;

			BaseVertices.Add(FVector(PosX, PosY, WaterLevel));
			Vertices.Add(FVector(PosX, PosY, WaterLevel));
			Normals.Add(FVector(0.0f, 0.0f, 1.0f));
			UVs.Add(FVector2D((float)X / GridResolution, (float)Y / GridResolution));
			VertexColors.Add(SurfaceColor);
		}
	}

	for (int32 Y = 0; Y < GridResolution; ++Y)
	{
		for (int32 X = 0; X < GridResolution; ++X)
		{
			int32 TL = Y * NumVerts + X;
			int32 TR = TL + 1;
			int32 BL = TL + NumVerts;
			int32 BR = BL + 1;

			Triangles.Add(TL);
			Triangles.Add(BL);
			Triangles.Add(TR);
			Triangles.Add(TR);
			Triangles.Add(BL);
			Triangles.Add(BR);
		}
	}

	SurfaceMesh->CreateMeshSection_LinearColor(0, Vertices, Triangles, Normals, UVs,
		VertexColors, TArray<FProcMeshTangent>(), false);
}

void AOceanPlaneActor::UpdateSurfaceWaves(float Time)
{
	FVector ActorLoc = GetActorLocation();

	for (int32 i = 0; i < Vertices.Num(); ++i)
	{
		float X = BaseVertices[i].X + ActorLoc.X;
		float Y = BaseVertices[i].Y + ActorLoc.Y;

		float Z = 0.0f;
		Z += FMath::Sin(X * 0.002f + Time * WaveSpeed) * WaveAmplitude;
		Z += FMath::Sin(Y * 0.003f + Time * WaveSpeed * 0.7f) * WaveAmplitude * 0.6f;
		Z += FMath::Sin((X + Y) * 0.001f + Time * WaveSpeed * 1.3f) * WaveAmplitude * 0.3f;
		Z += FMath::Sin(X * 0.005f - Time * WaveSpeed * 0.5f) * WaveAmplitude * 0.2f;

		Vertices[i].Z = WaterLevel + Z;

		float dZdX = 0.002f * FMath::Cos(X * 0.002f + Time * WaveSpeed) * WaveAmplitude
			+ 0.001f * FMath::Cos((X + Y) * 0.001f + Time * WaveSpeed * 1.3f) * WaveAmplitude * 0.3f;
		float dZdY = 0.003f * FMath::Cos(Y * 0.003f + Time * WaveSpeed * 0.7f) * WaveAmplitude * 0.6f
			+ 0.001f * FMath::Cos((X + Y) * 0.001f + Time * WaveSpeed * 1.3f) * WaveAmplitude * 0.3f;

		Normals[i] = FVector(-dZdX, -dZdY, 1.0f).GetSafeNormal();
	}

	SurfaceMesh->UpdateMeshSection_LinearColor(0, Vertices, Normals, TArray<FVector2D>(),
		TArray<FLinearColor>(), TArray<FProcMeshTangent>());
}

void AOceanPlaneActor::BeginPlay()
{
	Super::BeginPlay();

	GenerateOceanLayers();
	bGridGenerated = true;

	// Try M_OceanVC material for all layers
	UMaterialInterface* OceanMat = LoadObject<UMaterialInterface>(
		nullptr, TEXT("/Game/Materials/M_OceanVC"));

	if (OceanMat)
	{
		SurfaceMesh->SetMaterial(0, OceanMat);
		ShallowMesh->SetMaterial(0, OceanMat);
		MidMesh->SetMaterial(0, OceanMat);
		DeepMesh->SetMaterial(0, OceanMat);
		UE_LOG(LogTemp, Log, TEXT("Ocean: M_OceanVC loaded for all layers."));
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("Ocean: M_OceanVC not found!"));
	}
}

void AOceanPlaneActor::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	if (bGridGenerated)
	{
		// Follow the player snapped to grid cells
		if (APlayerController* PC = GetWorld()->GetFirstPlayerController())
		{
			if (APawn* PlayerPawn = PC->GetPawn())
			{
				FVector PlayerLoc = PlayerPawn->GetActorLocation();
				float CellSize = OceanSize / GridResolution;
				float SnappedX = FMath::RoundToFloat(PlayerLoc.X / CellSize) * CellSize;
				float SnappedY = FMath::RoundToFloat(PlayerLoc.Y / CellSize) * CellSize;
				FVector OceanLoc = GetActorLocation();
				OceanLoc.X = SnappedX;
				OceanLoc.Y = SnappedY;
				SetActorLocation(OceanLoc);
			}
		}

		UpdateSurfaceWaves(GetWorld()->GetTimeSeconds());
	}
}
