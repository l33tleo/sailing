#include "PortMarkerActor.h"
#include "Components/StaticMeshComponent.h"
#include "Materials/MaterialInterface.h"

APortMarkerActor::APortMarkerActor()
{
	PrimaryActorTick.bCanEverTick = false;

	MarkerMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("MarkerMesh"));
	RootComponent = MarkerMesh;
	MarkerMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	if (UStaticMesh* CylinderMesh = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cylinder.Cylinder")))
	{
		MarkerMesh->SetStaticMesh(CylinderMesh);
		MarkerMesh->SetRelativeScale3D(FVector(4.0f, 4.0f, 6.0f));
	}

	if (UMaterialInterface* MarkerMaterial = LoadObject<UMaterialInterface>(nullptr, TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial")))
	{
		MarkerMesh->SetMaterial(0, MarkerMaterial);
	}
}
