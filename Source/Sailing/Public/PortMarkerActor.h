#pragma once

#include "CoreMinimal.h"
#include "Data/PortDataAsset.h"
#include "GameFramework/Actor.h"
#include "PortMarkerActor.generated.h"

class UStaticMeshComponent;
class USphereComponent;
class ASailboatPawn;

/**
 * Simple harbor marker actor used as first world-feature registry entry type.
 */
UCLASS()
class SAILING_API APortMarkerActor : public AActor
{
	GENERATED_BODY()

public:
	APortMarkerActor();
	virtual void BeginPlay() override;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UStaticMeshComponent> MarkerMesh;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<USphereComponent> DockTrigger;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Port")
	FName PortId = NAME_None;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Port")
	FText PortDisplayName;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Port", meta = (ClampMin = "0"))
	int32 DockBonusCredits = 75;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Port")
	bool bGrantOneTimeDockBonus = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Port")
	bool bAutoRepairAtPort = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Port", meta = (ClampMin = "0"))
	int32 RepairCostPerPercentPoint = 1;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Port|MissionBoard")
	bool bOfferMissionBoard = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Port|MissionBoard")
	bool bCycleMissionOnDock = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Port|MissionBoard")
	bool bRestrictToOfferedMissions = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Port|MissionBoard")
	TArray<FName> OfferedMissionIds;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Port|MissionBoard")
	TArray<FPortMissionWeightedOffer> WeightedOfferedMissions;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Port|MissionBoard", meta = (ClampMin = "0", ClampMax = "10"))
	int32 MaxOfferedMissionsAtBoard = 3;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Port|MissionBoard", meta = (ClampMin = "0"))
	float MissionBoardCooldownSeconds = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Port|Services")
	bool bOfferUpgradeService = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Port|Services")
	TArray<FName> OfferedUpgradeIds;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Port|Services")
	TArray<FPortUpgradeWeightedOffer> WeightedOfferedUpgrades;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Port|Services", meta = (ClampMin = "0", ClampMax = "10"))
	int32 MaxOfferedUpgrades = 3;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Port|Services")
	bool bRotateUpgradeStockByVisits = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Port|Services", meta = (ClampMin = "0.1", ClampMax = "5.0"))
	float UpgradeCostMultiplier = 1.0f;

private:
	bool bVisitedInSession = false;
	float NextMissionBoardAvailableTime = 0.0f;

	UFUNCTION()
	void OnDockTriggerOverlap(UPrimitiveComponent* OverlappedComp, AActor* OtherActor,
		UPrimitiveComponent* OtherComp, int32 OtherBodyIndex,
		bool bFromSweep, const FHitResult& SweepResult);
};
