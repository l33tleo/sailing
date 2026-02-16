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
	bool bBoatRepaired = false;
	bool bMissionUpdated = false;
	bool bMissionBoardOnCooldown = false;
	float MissionBoardCooldownRemaining = 0.0f;
	FName NewMissionId = NAME_None;
	FName CurrentMissionId = NAME_None;
	int32 PortVisitCount = 0;
	TArray<FName> EffectiveOfferedMissionIds;
	if (UGameInstance* GI = GetGameInstance())
	{
		if (UWorldStreamingSubsystem* WorldSubsystem = GI->GetSubsystem<UWorldStreamingSubsystem>())
		{
			WorldSubsystem->MarkPortVisited(PortId);
			PortVisitCount = WorldSubsystem->GetPortVisitCounts().FindRef(PortId);
		}

		EffectiveOfferedMissionIds = UPortDataAsset::BuildPrioritizedMissionIds(
			WeightedOfferedMissions, OfferedMissionIds, PortVisitCount, MaxOfferedMissionsAtBoard);

		if (UTelemetrySubsystem* TelemetrySubsystem = GI->GetSubsystem<UTelemetrySubsystem>())
		{
			TelemetrySubsystem->RecordCounterEvent(TEXT("PortVisits"), 1);
			if (!PortId.IsNone())
			{
				TelemetrySubsystem->RecordCounterEvent(FName(*FString::Printf(TEXT("PortVisit_%s"), *PortId.ToString())), 1);
			}
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

		if (bAutoRepairAtPort)
		{
			if (UEconomySubsystem* EconomySubsystem = GI->GetSubsystem<UEconomySubsystem>())
			{
				bBoatRepaired = EconomySubsystem->RepairBoatToFull(RepairCostPerPercentPoint);
			}
		}

		if (bOfferMissionBoard)
		{
			if (UMissionSubsystem* MissionSubsystem = GI->GetSubsystem<UMissionSubsystem>())
			{
				const float CurrentTime = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0f;
				if (CurrentTime < NextMissionBoardAvailableTime)
				{
					bMissionBoardOnCooldown = true;
					MissionBoardCooldownRemaining = NextMissionBoardAvailableTime - CurrentTime;
				}
				else
				{
					if (bRestrictToOfferedMissions && EffectiveOfferedMissionIds.Num() > 0)
					{
						bMissionUpdated = MissionSubsystem->ActivateMissionFromCandidates(EffectiveOfferedMissionIds, bCycleMissionOnDock);
					}
					else if (bCycleMissionOnDock)
					{
						bMissionUpdated = MissionSubsystem->CycleToNextMission();
					}
					else if (MissionSubsystem->GetActiveMissionId().IsNone())
					{
						bMissionUpdated = MissionSubsystem->ActivateFallbackMission();
					}

					if (MissionBoardCooldownSeconds > 0.0f)
					{
						NextMissionBoardAvailableTime = CurrentTime + MissionBoardCooldownSeconds;
					}
				}

				if (bMissionUpdated)
				{
					NewMissionId = MissionSubsystem->GetActiveMissionId();
				}
				CurrentMissionId = MissionSubsystem->GetActiveMissionId();
			}
		}
	}
	else
	{
		EffectiveOfferedMissionIds = UPortDataAsset::BuildPrioritizedMissionIds(
			WeightedOfferedMissions, OfferedMissionIds, PortVisitCount, MaxOfferedMissionsAtBoard);
	}

	bVisitedInSession = true;

	const FString PortNameText = PortDisplayName.IsEmpty() ? PortId.ToString() : PortDisplayName.ToString();

	if (APlayerController* PC = GetWorld()->GetFirstPlayerController())
	{
		if (ASailingHUD* HUD = Cast<ASailingHUD>(PC->GetHUD()))
		{
			FString PopupText = GrantedCredits > 0
				? FString::Printf(TEXT("%s: Havnebonus +%d%s"), *PortNameText, GrantedCredits, bBoatRepaired ? TEXT(" | Båt reparert") : TEXT(""))
				: FString::Printf(TEXT("%s: Havn anløpt%s"), *PortNameText, bBoatRepaired ? TEXT(" | Båt reparert") : TEXT(""));

			if (bMissionUpdated && !NewMissionId.IsNone())
			{
				PopupText += FString::Printf(TEXT(" | Oppdrag: %s"), *NewMissionId.ToString());
			}
			if (bMissionBoardOnCooldown)
			{
				PopupText += FString::Printf(TEXT(" | Tavle klar om %.0fs"), MissionBoardCooldownRemaining);
			}

			HUD->ShowDiscoveryPopup(PopupText);

			if (bOfferMissionBoard)
			{
				HUD->ShowPortMissionBoard(PortId, PortDisplayName, EffectiveOfferedMissionIds, CurrentMissionId,
					bMissionBoardOnCooldown, MissionBoardCooldownRemaining);
			}
		}
	}

	if (ASailingGameMode* GM = Cast<ASailingGameMode>(UGameplayStatics::GetGameMode(this)))
	{
		GM->SaveGame_();
	}
}
