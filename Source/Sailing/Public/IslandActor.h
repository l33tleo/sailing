#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "IslandActor.generated.h"

class USphereComponent;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnIslandDiscovered, AIslandActor*, Island, const FString&, IslandName);

UCLASS()
class SAILING_API AIslandActor : public AActor
{
	GENERATED_BODY()

public:
	AIslandActor();
	virtual void BeginPlay() override;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UStaticMeshComponent> IslandMesh;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<USphereComponent> DiscoverySphere;

	UPROPERTY(BlueprintReadOnly, Category = "Island")
	bool bDiscovered = false;

	UPROPERTY(BlueprintReadOnly, Category = "Island")
	FString IslandName;

	UPROPERTY(BlueprintReadOnly, Category = "Island")
	FIntPoint ChunkCoord;

	UPROPERTY(BlueprintReadOnly, Category = "Island")
	int32 IslandIndex = 0;

	// Called when island is discovered
	UPROPERTY(BlueprintAssignable, Category = "Island")
	FOnIslandDiscovered OnDiscovered;

	// Initialize island with chunk data
	void InitializeIsland(FIntPoint InChunkCoord, int32 InIslandIndex, bool bWasDiscovered);

	// Mark island as discovered (called from save system or overlap)
	void SetDiscovered(bool bFromSaveGame = false);

	UFUNCTION()
	void OnDiscoveryOverlap(UPrimitiveComponent* OverlappedComp, AActor* OtherActor,
		UPrimitiveComponent* OtherComp, int32 OtherBodyIndex,
		bool bFromSweep, const FHitResult& SweepResult);

private:
	void ApplyDiscoveredMaterial();

	// True if the Blender island model was loaded (materials baked in)
	bool bUsingBlenderMesh = false;
};
