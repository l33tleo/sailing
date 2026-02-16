#include "MissionObjectiveActor.h"
#include "SailboatPawn.h"
#include "SailingGameMode.h"
#include "SailingHUD.h"
#include "Components/SphereComponent.h"
#include "Components/StaticMeshComponent.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "Materials/MaterialInterface.h"
#include "Systems/SailingCoreSubsystems.h"

AMissionObjectiveActor::AMissionObjectiveActor()
{
	PrimaryActorTick.bCanEverTick = false;

	MarkerMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("MarkerMesh"));
	RootComponent = MarkerMesh;
	MarkerMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	if (UStaticMesh* SphereMesh = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Sphere.Sphere")))
	{
		MarkerMesh->SetStaticMesh(SphereMesh);
		MarkerMesh->SetRelativeScale3D(FVector(4.0f, 4.0f, 2.0f));
	}

	if (UMaterialInterface* MarkerMaterial = LoadObject<UMaterialInterface>(nullptr, TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial")))
	{
		MarkerMesh->SetMaterial(0, MarkerMaterial);
	}

	ObjectiveTrigger = CreateDefaultSubobject<USphereComponent>(TEXT("ObjectiveTrigger"));
	ObjectiveTrigger->SetupAttachment(RootComponent);
	ObjectiveTrigger->SetSphereRadius(900.0f);
	ObjectiveTrigger->SetCollisionProfileName(TEXT("OverlapAllDynamic"));
}

void AMissionObjectiveActor::BeginPlay()
{
	Super::BeginPlay();
	ObjectiveTrigger->OnComponentBeginOverlap.AddDynamic(this, &AMissionObjectiveActor::OnObjectiveOverlap);
}

void AMissionObjectiveActor::OnObjectiveOverlap(UPrimitiveComponent* OverlappedComp, AActor* OtherActor,
	UPrimitiveComponent* OtherComp, int32 OtherBodyIndex,
	bool bFromSweep, const FHitResult& SweepResult)
{
	if (!OtherActor || !OtherActor->IsA<ASailboatPawn>())
	{
		return;
	}

	int32 RewardCredits = 0;
	bool bMissionStateAdvanced = false;
	if (UGameInstance* GI = GetGameInstance())
	{
		if (UMissionSubsystem* MissionSubsystem = GI->GetSubsystem<UMissionSubsystem>())
		{
			const FName MissionIdBeforeCompletion = MissionSubsystem->GetActiveMissionId();
			RewardCredits = MissionSubsystem->CompleteActiveMissionAtLocation(TriggerType, GetActorLocation());
			const FName MissionIdAfterCompletion = MissionSubsystem->GetActiveMissionId();
			bMissionStateAdvanced = MissionIdBeforeCompletion != MissionIdAfterCompletion;
		}

		if (RewardCredits > 0)
		{
			if (UEconomySubsystem* EconomySubsystem = GI->GetSubsystem<UEconomySubsystem>())
			{
				EconomySubsystem->AddCredits(RewardCredits);
			}
		}

		if (UTelemetrySubsystem* TelemetrySubsystem = GI->GetSubsystem<UTelemetrySubsystem>())
		{
			if (bMissionStateAdvanced)
			{
				TelemetrySubsystem->RecordCounterEvent(TEXT("MissionCompleted"), 1);
			}
			if (RewardCredits > 0)
			{
				TelemetrySubsystem->RecordCounterEvent(TEXT("CreditsGranted"), RewardCredits);
			}
		}
	}

	if (!bMissionStateAdvanced && RewardCredits <= 0)
	{
		return;
	}

	if (APlayerController* PC = GetWorld()->GetFirstPlayerController())
	{
		if (ASailingHUD* HUD = Cast<ASailingHUD>(PC->GetHUD()))
		{
			HUD->ShowDiscoveryPopup(RewardCredits > 0
				? FString::Printf(TEXT("Oppdrag fullført (+%d)"), RewardCredits)
				: FString(TEXT("Oppdrag fullført")));
		}
	}

	if (ASailingGameMode* GM = Cast<ASailingGameMode>(UGameplayStatics::GetGameMode(this)))
	{
		GM->SaveGame_();
	}

	UE_LOG(LogTemp, Log, TEXT("Mission objective completed. Reward=%d Advanced=%s"),
		RewardCredits,
		bMissionStateAdvanced ? TEXT("true") : TEXT("false"));

	if (bDestroyAfterCompletion && (bMissionStateAdvanced || RewardCredits > 0))
	{
		Destroy();
	}
}
