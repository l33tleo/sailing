#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "SailingGameMode.generated.h"

class USaveGameSailing;
class AChunkManager;
class AFjordMapManager;
class AFjordCoastlineActor;

UCLASS()
class SAILING_API ASailingGameMode : public AGameModeBase
{
	GENERATED_BODY()

public:
	ASailingGameMode();

	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	/** When true, use fjord map (FjordMapManager + FjordCoastlineActor) instead of procedural ChunkManager. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Fjord")
	bool bUseFjordMap = true;

	/** Start position for new game in fjord mode (e.g. near Oslo). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Fjord")
	FVector FjordStartPosition = FVector(-139600.0f, -150000.0f, 100.0f);

	// Get the current save game
	UFUNCTION(BlueprintCallable, Category = "Save")
	USaveGameSailing* GetSaveGame() const { return SaveGame; }

	// Get chunk manager (null if bUseFjordMap)
	AChunkManager* GetChunkManager() const;

	// Save the game
	UFUNCTION(BlueprintCallable, Category = "Save")
	void SaveGame_();

private:
	UPROPERTY()
	TObjectPtr<AActor> SpawnedWind;

	UPROPERTY()
	TObjectPtr<AActor> SpawnedChunkManager;

	UPROPERTY()
	TObjectPtr<AActor> SpawnedFjordMapManager;

	UPROPERTY()
	TObjectPtr<AActor> SpawnedFjordCoastline;

	UPROPERTY()
	TObjectPtr<AActor> SpawnedOcean;

	UPROPERTY()
	TObjectPtr<USaveGameSailing> SaveGame;

	void LoadOrCreateSaveGame();
};
