#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "SailingGameMode.generated.h"

class USaveGameSailing;
class AChunkManager;

UCLASS()
class SAILING_API ASailingGameMode : public AGameModeBase
{
	GENERATED_BODY()

public:
	ASailingGameMode();

	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	// Get the current save game
	UFUNCTION(BlueprintCallable, Category = "Save")
	USaveGameSailing* GetSaveGame() const { return SaveGame; }

	// Get chunk manager
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
	TObjectPtr<AActor> SpawnedOcean;

	UPROPERTY()
	TObjectPtr<USaveGameSailing> SaveGame;

	void LoadOrCreateSaveGame();
};
