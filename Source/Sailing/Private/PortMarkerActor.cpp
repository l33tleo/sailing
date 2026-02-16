#include "PortMarkerActor.h"
#include "SailboatPawn.h"
#include "SailingGameMode.h"
#include "SailingHUD.h"
#include "Components/SphereComponent.h"
#include "Components/StaticMeshComponent.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "Materials/MaterialInterface.h"
#include "Systems/SailingCoreSubsystems.h"

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

	DockTrigger = CreateDefaultSubobject<USphereComponent>(TEXT("DockTrigger"));
	DockTrigger->SetupAttachment(RootComponent);
	DockTrigger->SetSphereRadius(1400.0f);
	DockTrigger->SetCollisionProfileName(TEXT("OverlapAllDynamic"));
}

void APortMarkerActor::BeginPlay()
{
	Super::BeginPlay();
	DockTrigger->OnComponentBeginOverlap.AddDynamic(this, &APortMarkerActor::OnDockTriggerOverlap);
}

void APortMarkerActor::OnDockTriggerOverlap(UPrimitiveComponent* OverlappedComp, AActor* OtherActor,
	UPrimitiveComponent* OtherComp, int32 OtherBodyIndex,
	bool bFromSweep, const FHitResult& SweepResult)
{
	if (!OtherActor || !OtherActor->IsA<ASailboatPawn>())
	{
		return;
	}

	int32 GrantedCredits = 0;
	if (UGameInstance* GI = GetGameInstance())
	{
		if (UTelemetrySubsystem* TelemetrySubsystem = GI->GetSubsystem<UTelemetrySubsystem>())
		{
			TelemetrySubsystem->RecordCounterEvent(TEXT("PortVisits"), 1);
		}

		const bool bCanGrantBonus = (!bVisitedInSession || !bGrantOneTimeDockBonus) && DockBonusCredits > 0;
		if (bCanGrantBonus)
		{
			if (UEconomySubsystem* EconomySubsystem = GI->GetSubsystem<UEconomySubsystem>())
			{
				EconomySubsystem->AddCredits(DockBonusCredits);
				GrantedCredits = DockBonusCredits;
			}

			if (UTelemetrySubsystem* TelemetrySubsystem = GI->GetSubsystem<UTelemetrySubsystem>())
			{
				TelemetrySubsystem->RecordCounterEvent(TEXT("CreditsGranted"), GrantedCredits);
			}
		}
	}

	bVisitedInSession = true;

	if (APlayerController* PC = GetWorld()->GetFirstPlayerController())
	{
		if (ASailingHUD* HUD = Cast<ASailingHUD>(PC->GetHUD()))
		{
			const FString PopupText = GrantedCredits > 0
				? FString::Printf(TEXT("%s: Havnebonus +%d"), *PortId.ToString(), GrantedCredits)
				: FString::Printf(TEXT("%s: Havn anløpt"), *PortId.ToString());
			HUD->ShowDiscoveryPopup(PopupText);
		}
	}

	if (ASailingGameMode* GM = Cast<ASailingGameMode>(UGameplayStatics::GetGameMode(this)))
	{
		GM->SaveGame_();
	}
}
