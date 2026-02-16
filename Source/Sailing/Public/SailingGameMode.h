#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "SailingGameMode.generated.h"

class USaveGameSailing;
class AChunkManager;
class AMissionObjectiveActor;
class APortMarkerActor;

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
	UPROPERTY(EditAnywhere, Category = "World|Missions")
	FName MissionAssetScanPath = TEXT("/Game");

	UPROPERTY(EditAnywhere, Category = "World|Economy")
	FName UpgradeAssetScanPath = TEXT("/Game");

	UPROPERTY(EditAnywhere, Category = "World|Ports")
	FName PortAssetScanPath = TEXT("/Game");

	UPROPERTY()
	TObjectPtr<AActor> SpawnedWind;

	UPROPERTY()
	TObjectPtr<AActor> SpawnedChunkManager;

	UPROPERTY()
	TObjectPtr<AActor> SpawnedOcean;

	UPROPERTY()
	TObjectPtr<AMissionObjectiveActor> SpawnedMissionObjective;

	UPROPERTY()
	TArray<TObjectPtr<APortMarkerActor>> SpawnedPortMarkers;

	UPROPERTY()
	TObjectPtr<USaveGameSailing> SaveGame;

	void LoadOrCreateSaveGame();
};
