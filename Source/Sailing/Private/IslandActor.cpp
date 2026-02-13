#include "IslandActor.h"
#include "SailboatPawn.h"
#include "IslandNameGenerator.h"
#include "Components/StaticMeshComponent.h"
#include "Components/SphereComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "UObject/ConstructorHelpers.h"

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
	UMaterialInterface* DiscoveredMat = LoadObject<UMaterialInterface>(
		nullptr, TEXT("/Game/Materials/M_IslandDiscovered"));
	if (DiscoveredMat && IslandMesh)
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
