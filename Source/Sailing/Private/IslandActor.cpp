#include "IslandActor.h"
#include "SailboatPawn.h"
#include "IslandNameGenerator.h"
#include "FjordGeometry.h"
#include "Sailing.h"
#include "Components/StaticMeshComponent.h"
#include "Components/SphereComponent.h"
#include "ProceduralMeshComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "UObject/ConstructorHelpers.h"

namespace
{
	// Island land surface sits just above water; the actor origin is at water level.
	// world Z = WaterZ + 95 = 195, a few uu above the mainland's LandZ=190 so the island
	// surface always wins the depth test where it overlaps a large coastline ring (r0)
	// and avoids coplanar z-fighting. Still well above the gust wave peak ~169.
	constexpr float IslandTopZLocal = 95.0f;
	constexpr float IslandSkirtDepth = 250.0f;  // wall extends below water so it is not paper-thin
	constexpr float DiscoveryMargin = 400.0f;   // how far from the shore discovery triggers
}

AIslandActor::AIslandActor()
{
	IslandMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("IslandMesh"));
	RootComponent = IslandMesh;

	// Try Blender island model first, fall back to basic cube
	static ConstructorHelpers::FObjectFinder<UStaticMesh> BlenderIsland(
		TEXT("/Game/Models/Island"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> CubeMesh(
		TEXT("/Engine/BasicShapes/Cube"));

	if (BlenderIsland.Succeeded())
	{
		IslandMesh->SetStaticMesh(BlenderIsland.Object);
		bUsingBlenderMesh = true;
	}
	else if (CubeMesh.Succeeded())
	{
		IslandMesh->SetStaticMesh(CubeMesh.Object);
		bUsingBlenderMesh = false;
	}

	IslandMesh->SetCollisionProfileName(TEXT("BlockAll"));

	// Filled polygon land mesh (used for fjord islands with a real outline).
	LandMesh = CreateDefaultSubobject<UProceduralMeshComponent>(TEXT("LandMesh"));
	LandMesh->SetupAttachment(RootComponent);
	LandMesh->SetCollisionProfileName(TEXT("FjordLand"));

	DiscoverySphere = CreateDefaultSubobject<USphereComponent>(TEXT("DiscoverySphere"));
	DiscoverySphere->SetupAttachment(RootComponent);
	DiscoverySphere->SetSphereRadius(500.0f);
	DiscoverySphere->SetCollisionProfileName(TEXT("OverlapAllDynamic"));
	DiscoverySphere->OnComponentBeginOverlap.AddDynamic(this, &AIslandActor::OnDiscoveryOverlap);
}

void AIslandActor::BeginPlay()
{
	Super::BeginPlay();

	if (bUsingBlenderMesh)
	{
		// Blender model has baked-in materials (grass, rock, trunk, canopy, bush)
		// Only override materials on discovery
		if (bDiscovered)
		{
			ApplyDiscoveredMaterial();
		}
		UE_LOG(LogTemp, Log, TEXT("Island: Using Blender island mesh."));
	}
	else
	{
		// Fallback cube: apply single-color material
		if (bDiscovered)
		{
			ApplyDiscoveredMaterial();
		}
		else
		{
			UMaterialInterface* IslandMat = LoadObject<UMaterialInterface>(
				nullptr, TEXT("/Game/Materials/M_Island"));
			if (IslandMat && IslandMesh)
			{
				IslandMesh->SetMaterial(0, IslandMat);
				UE_LOG(LogTemp, Log, TEXT("Island: M_Island loaded (fallback cube)."));
			}
			else
			{
				UE_LOG(LogTemp, Warning, TEXT("Island: M_Island NOT found! Using default."));
			}
		}
	}
}

void AIslandActor::InitializeIsland(FIntPoint InChunkCoord, int32 InIslandIndex, bool bWasDiscovered)
{
	ChunkCoord = InChunkCoord;
	IslandIndex = InIslandIndex;

	// Generate deterministic name
	IslandName = UIslandNameGenerator::GenerateIslandName(ChunkCoord, IslandIndex);

	if (bWasDiscovered)
	{
		SetDiscovered(true);
	}
}

void AIslandActor::InitializeFjordIsland(const FString& InName, int32 InIndex, bool bWasDiscovered)
{
	IslandName = InName;
	ChunkCoord = FIntPoint(-1, -1);
	IslandIndex = InIndex;

	if (bWasDiscovered)
	{
		SetDiscovered(true);
	}
}

void AIslandActor::InitializeFjordIslandPolygon(const FString& InName, int32 InIndex, bool bWasDiscovered,
	const TArray<FVector2D>& InLocalOutline)
{
	IslandName = InName;
	ChunkCoord = FIntPoint(-1, -1);
	IslandIndex = InIndex;
	LocalOutline = InLocalOutline;

	if (LocalOutline.Num() >= 3)
	{
		bUsingPolygon = true;
		if (BakedMesh && IslandMesh)
		{
			// Bakt terreng: z=0 i meshen er middelvannstand, og aktøren står på WaterZ.
			IslandMesh->SetStaticMesh(BakedMesh);
			IslandMesh->SetCollisionProfileName(TEXT("FjordLand"));
			ApplyLandMaterial(IslandMesh);
			UE_LOG(LogTemp, Log, TEXT("Island %s: bakt terreng %s"), *IslandName, *BakedMesh->GetName());
		}
		else
		{
			BuildPolygonMesh();
		}

		// Hide the fallback static mesh; the polygon provides shape and collision.
		if (IslandMesh && !BakedMesh)
		{
			IslandMesh->SetVisibility(false);
			IslandMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		}

		// Size the discovery sphere from the outline bounds so large islands trigger
		// from the shore rather than a fixed 500 uu.
		float MaxDistSq = 0.0f;
		for (const FVector2D& P : LocalOutline)
		{
			MaxDistSq = FMath::Max(MaxDistSq, P.SizeSquared());
		}
		if (DiscoverySphere)
		{
			DiscoverySphere->SetSphereRadius(FMath::Sqrt(MaxDistSq) + DiscoveryMargin);
		}
	}

	if (bWasDiscovered)
	{
		SetDiscovered(true);
	}
}

void AIslandActor::SetTerrainSource(TSharedPtr<FjordGeometry::FHeightGrid> InGrid,
	float InHeightExaggeration, float InDistanceScale)
{
	HeightGrid = InGrid;
	HeightExaggeration = InHeightExaggeration;
	DistanceScale = InDistanceScale;
}

void AIslandActor::BuildPolygonMesh()
{
	if (!LandMesh || LocalOutline.Num() < 3)
	{
		return;
	}

	LandMesh->ClearAllMeshSections();

	const FColor LandColor(51, 128, 51, 255);
	bool bBuilt = false;
	if (HeightGrid.IsValid() && HeightGrid->IsValid())
	{
		const FVector Loc = GetActorLocation();
		const FVector2D WorldOrigin(Loc.X, Loc.Y);
		const float HeightScale = DistanceScale * HeightExaggeration;
		bBuilt = FjordGeometry::BuildTerrainPolygon(LandMesh, LocalOutline, *HeightGrid,
			WorldOrigin, IslandTopZLocal, HeightScale, IslandSkirtDepth, 0, LandColor);
	}
	if (!bBuilt)
	{
		bBuilt = FjordGeometry::BuildFilledPolygon(LandMesh, LocalOutline, IslandTopZLocal, IslandSkirtDepth, 0, LandColor);
	}

	ApplyLandMaterial(LandMesh);
}

void AIslandActor::ApplyLandMaterial(UMeshComponent* Target)
{
	// Prefer the aerial M_Land material (as a dynamic instance so discovery can highlight
	// without replacing the photo). Fall back to the flat green materials if it is absent.
	UMaterialInterface* LandBase = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/Fjord/M_Land.M_Land"));
	if (LandBase)
	{
		LandMID = UMaterialInstanceDynamic::Create(LandBase, this);
		LandMID->SetScalarParameterValue(TEXT("Discovered"), bDiscovered ? 1.0f : 0.0f);
		Target->SetMaterial(0, LandMID);
	}
	else
	{
		UMaterialInterface* Mat = LoadObject<UMaterialInterface>(
			nullptr, bDiscovered ? TEXT("/Game/Materials/M_IslandDiscovered") : TEXT("/Game/Materials/M_Island"));
		if (Mat)
		{
			Target->SetMaterial(0, Mat);
		}
	}
}

void AIslandActor::SetDiscovered(bool bFromSaveGame)
{
	if (bDiscovered)
	{
		return;
	}

	bDiscovered = true;
	ApplyDiscoveredMaterial();

	if (!bFromSaveGame)
	{
		// Broadcast discovery event for HUD and save system
		OnDiscovered.Broadcast(this, IslandName);
		UE_LOG(LogTemp, Warning, TEXT("Oppdaget øy: %s ved %s"), *IslandName, *GetActorLocation().ToString());
	}
}

void AIslandActor::ApplyDiscoveredMaterial()
{
	// Aerial land material: highlight via a parameter, keeping the photo texture.
	if (bUsingPolygon && LandMID)
	{
		LandMID->SetScalarParameterValue(TEXT("Discovered"), 1.0f);
		return;
	}

	UMaterialInterface* DiscoveredMat = LoadObject<UMaterialInterface>(
		nullptr, TEXT("/Game/Materials/M_IslandDiscovered"));
	if (!DiscoveredMat)
	{
		return;
	}

	if (bUsingPolygon && LandMesh && !BakedMesh)
	{
		LandMesh->SetMaterial(0, DiscoveredMat);
		return;
	}

	if (IslandMesh)
	{
		// Apply discovered material to all material slots
		int32 NumSlots = IslandMesh->GetNumMaterials();
		for (int32 i = 0; i < NumSlots; ++i)
		{
			IslandMesh->SetMaterial(i, DiscoveredMat);
		}
	}
}

void AIslandActor::OnDiscoveryOverlap(UPrimitiveComponent* OverlappedComp, AActor* OtherActor,
	UPrimitiveComponent* OtherComp, int32 OtherBodyIndex,
	bool bFromSweep, const FHitResult& SweepResult)
{
	if (!bDiscovered && OtherActor && OtherActor->IsA<ASailboatPawn>())
	{
		SetDiscovered(false);
	}
}
