#include "FjordCoastlineActor.h"
#include "FjordMapManager.h"
#include "FjordMapData.h"
#include "FjordGeometry.h"
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
	CoastlineMesh->SetCollisionProfileName(TEXT("FjordLand"));

	// Default to the generated Oslofjord asset; falls back to old behaviour if absent.
	FjordMapDataPath = FSoftObjectPath(TEXT("/Game/Fjord/OslofjordMapData.OslofjordMapData"));
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

	// Match the island DistanceScale (islands are placed at Position × DistanceScale) and
	// reuse the manager's shared elevation grid so mainland gets the same relief.
	float Scale = DistanceScale;
	if (AFjordMapManager* FM = Cast<AFjordMapManager>(UGameplayStatics::GetActorOfClass(GetWorld(), AFjordMapManager::StaticClass())))
	{
		Scale = FM->DistanceScale;
		HeightGrid = FM->GetHeightGrid();
		HeightExaggeration = FM->HeightExaggeration;
	}

	if (Data->Landmasses.Num() > 0)
	{
		BuildLandmasses(Data->Landmasses, Data->Islands, Scale);
	}
	else if (Data->CoastlinePoints.Num() >= 3)
	{
		// Legacy fallback: thin extruded strip along a single polygon.
		BuildCoastlineMeshFromPolygon(Data->CoastlinePoints);
	}

	// Prefer the aerial M_Land material; fall back to flat green M_Island.
	UMaterialInterface* LandMat = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/Fjord/M_Land.M_Land"));
	if (!LandMat)
	{
		LandMat = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/Materials/M_Island.M_Island"));
	}
	if (LandMat && CoastlineMesh)
	{
		const int32 NumSections = CoastlineMesh->GetNumSections();
		for (int32 s = 0; s < NumSections; ++s)
		{
			CoastlineMesh->SetMaterial(s, LandMat);
		}
	}

	UE_LOG(LogTemp, Log, TEXT("FjordCoastlineActor: Built %d land section(s)."), CoastlineMesh ? CoastlineMesh->GetNumSections() : 0);
}

void AFjordCoastlineActor::BuildLandmasses(const TArray<FFjordRing>& Landmasses,
                                           const TArray<FFjordIslandDef>& Islands,
                                           float InDistanceScale)
{
	const FColor LandColor(51, 128, 51, 255);
	const FVector Loc = GetActorLocation();
	const FVector2D WorldOrigin(Loc.X, Loc.Y);
	const float HeightScale = InDistanceScale * HeightExaggeration;
	const bool bHaveGrid = HeightGrid.IsValid() && HeightGrid->IsValid();

	// A named island's outline is also present in Landmasses (the OSM pipeline fetches
	// islands via both natural=coastline and natural=island). Drawing both produces
	// coincident opaque surfaces -> z-fighting. Skip landmass rings that coincide with
	// an island so each island is drawn once by its AIslandActor. Ring.Points and
	// Islands[].Position are both in pre-scale meters, so centroids compare directly.
	const float IslandMatchTolSq = 50.0f * 50.0f; // 50 m, in pre-scale meters

	int32 Section = 0;
	int32 SkippedIslandRings = 0;
	for (const FFjordRing& Ring : Landmasses)
	{
		if (Ring.Points.Num() < 3)
		{
			continue;
		}

		FVector2D Centroid = FVector2D::ZeroVector;
		for (const FVector2D& P : Ring.Points)
		{
			Centroid += P;
		}
		Centroid /= Ring.Points.Num();

		bool bIsIsland = false;
		for (const FFjordIslandDef& Island : Islands)
		{
			if (FVector2D::DistSquared(Centroid, Island.Position) <= IslandMatchTolSq)
			{
				bIsIsland = true;
				break;
			}
		}
		if (bIsIsland)
		{
			++SkippedIslandRings;
			continue;
		}

		TArray<FVector2D> Scaled;
		Scaled.Reserve(Ring.Points.Num());
		for (const FVector2D& P : Ring.Points)
		{
			Scaled.Add(P * InDistanceScale);
		}

		bool bBuilt = false;
		if (bHaveGrid)
		{
			bBuilt = FjordGeometry::BuildTerrainPolygon(CoastlineMesh, Scaled, *HeightGrid,
				WorldOrigin, LandZ, HeightScale, LandSkirtDepth, Section, LandColor);
		}
		if (!bBuilt)
		{
			bBuilt = FjordGeometry::BuildFilledPolygon(CoastlineMesh, Scaled, LandZ, LandSkirtDepth, Section, LandColor);
		}
		if (bBuilt)
		{
			++Section;
		}
	}

	UE_LOG(LogTemp, Log, TEXT("FjordCoastlineActor: Skipped %d landmass ring(s) that coincide with named islands."), SkippedIslandRings);
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
