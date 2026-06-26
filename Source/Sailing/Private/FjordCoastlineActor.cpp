#include "FjordCoastlineActor.h"
#include "FjordMapManager.h"
#include "FjordMapData.h"
#include "ProceduralMeshComponent.h"
#include "Materials/MaterialInterface.h"
#include "Kismet/GameplayStatics.h"

AFjordCoastlineActor::AFjordCoastlineActor()
{
	PrimaryActorTick.bCanEverTick = false;

	USceneComponent* Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	RootComponent = Root;

	CoastlineMesh = CreateDefaultSubobject<UProceduralMeshComponent>(TEXT("CoastlineMesh"));
	CoastlineMesh->SetupAttachment(RootComponent);
	CoastlineMesh->SetCollisionProfileName(TEXT("BlockAll"));
}

void AFjordCoastlineActor::BeginPlay()
{
	Super::BeginPlay();

	UFjordMapData* Data = FjordMapData;
	if (!Data && !FjordMapDataPath.IsNull())
	{
		Data = Cast<UFjordMapData>(FjordMapDataPath.TryLoad());
	}
	if (!Data && GetWorld())
	{
		if (AFjordMapManager* FM = Cast<AFjordMapManager>(UGameplayStatics::GetActorOfClass(GetWorld(), AFjordMapManager::StaticClass())))
		{
			Data = FM->FjordMapData;
		}
	}
	if (!Data)
	{
		UE_LOG(LogTemp, Warning, TEXT("FjordCoastlineActor: No FjordMapData. No coastline mesh."));
		return;
	}

	if (Data->CoastlinePoints.Num() >= 3)
	{
		BuildCoastlineMeshFromPolygon(Data->CoastlinePoints);
	}

	UMaterialInterface* LandMat = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/Materials/M_Island.M_Island"));
	if (LandMat && CoastlineMesh)
	{
		CoastlineMesh->SetMaterial(0, LandMat);
	}

	UE_LOG(LogTemp, Log, TEXT("FjordCoastlineActor: Built coastline mesh."));
}

void AFjordCoastlineActor::BuildCoastlineMeshFromPolygon(const TArray<FVector2D>& Points)
{
	if (Points.Num() < 3)
	{
		return;
	}

	TArray<FVector> Vertices;
	TArray<int32> Triangles;
	TArray<FVector> Normals;
	TArray<FVector2D> UVs;
	TArray<FColor> VertexColors;

	const int32 N = Points.Num();
	Vertices.Reserve(N * 2);
	Normals.Reserve(N * 2);
	UVs.Reserve(N * 2);
	VertexColors.Reserve(N * 2);

	const FColor Color(51, 128, 51, 255);

	for (int32 i = 0; i < N; ++i)
	{
		const FVector2D& A = Points[i];
		const FVector2D& B = Points[(i + 1) % N];
		FVector2D Edge = B - A;
		Edge.Normalize();
		// Inward for CCW polygon: perpendicular left = (-Edge.Y, Edge.X)
		FVector2D Inward(-Edge.Y, Edge.X);
		Inward *= CoastStripWidth;

		FVector2D InnerA = A + Inward;
		FVector2D InnerB = B + Inward;

		Vertices.Add(FVector(A.X, A.Y, LandZ));
		Vertices.Add(FVector(InnerA.X, InnerA.Y, LandZ));
		Normals.Add(FVector::UpVector);
		Normals.Add(FVector::UpVector);
		UVs.Add(FVector2D(static_cast<float>(i) / N, 0.0f));
		UVs.Add(FVector2D(static_cast<float>(i) / N, 1.0f));
		VertexColors.Add(Color);
		VertexColors.Add(Color);
	}

	// Two triangles per quad: (i*2, i*2+1, (i+1)*2), (i*2+1, (i+1)*2+1, (i+1)*2)
	for (int32 i = 0; i < N; ++i)
	{
		int32 i0 = i * 2;
		int32 i1 = i * 2 + 1;
		int32 j0 = ((i + 1) % N) * 2;
		int32 j1 = ((i + 1) % N) * 2 + 1;

		Triangles.Add(i0);
		Triangles.Add(i1);
		Triangles.Add(j0);

		Triangles.Add(i1);
		Triangles.Add(j1);
		Triangles.Add(j0);
	}

	const int32 Section = CoastlineMesh->GetNumSections();
	CoastlineMesh->CreateMeshSection(Section, Vertices, Triangles, Normals, UVs, VertexColors,
		TArray<FProcMeshTangent>(), true);
}
