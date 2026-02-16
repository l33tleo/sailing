#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "ChunkManager.generated.h"

class AIslandActor;
class USaveGameSailing;

UCLASS()
class SAILING_API AChunkManager : public AActor
{
	GENERATED_BODY()

public:
	AChunkManager();

	virtual void Tick(float DeltaTime) override;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Chunks")
	float ChunkSize = 5000.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Chunks")
	int32 LoadRadius = 3;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Chunks")
	int32 UnloadRadius = 5;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Chunks")
	int32 MaxIslandsPerChunk = 3;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Chunks")
	TSubclassOf<AIslandActor> IslandClass;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Economy", meta = (ClampMin = "0"))
	int32 DiscoveryCreditReward = 25;

	// Set save game reference for persistence
	void SetSaveGame(USaveGameSailing* InSaveGame);

private:
	TMap<FIntPoint, TArray<TWeakObjectPtr<AIslandActor>>> SpawnedChunks;

	UPROPERTY()
	TObjectPtr<USaveGameSailing> SaveGame;

	FIntPoint WorldToChunk(const FVector& Location) const;
	void LoadChunk(FIntPoint ChunkCoord);
	void UnloadDistantChunks(FIntPoint PlayerChunk);

	// Handle island discovery for save system
	UFUNCTION()
	void OnIslandDiscovered(AIslandActor* Island, const FString& IslandName);

	float TimeSinceLastUpdate = 0.0f;
	float UpdateInterval = 0.5f;
};
