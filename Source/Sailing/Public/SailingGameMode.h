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

	// Synchronize objective marker with active mission objective policy.
	UFUNCTION(BlueprintCallable, Category = "Mission")
	bool SyncActiveMissionObjectiveMarker();

	// Save the game
	UFUNCTION(BlueprintCallable, Category = "Save")
	void SaveGame_();

private:
	UPROPERTY(EditAnywhere, Category = "World|Content")
	bool bEnableRuntimeFallbackContent = true;

	UPROPERTY(EditAnywhere, Category = "World|Missions")
	FName MissionAssetScanPath = TEXT("/Game");

	UPROPERTY(EditAnywhere, Category = "World|Economy")
	FName UpgradeAssetScanPath = TEXT("/Game");

	UPROPERTY(EditAnywhere, Category = "World|Ports")
	FName PortAssetScanPath = TEXT("/Game");

	UPROPERTY(EditAnywhere, Category = "World|ContentValidation")
	TArray<FName> RequiredStartupMissionIds;

	UPROPERTY(EditAnywhere, Category = "World|ContentValidation")
	TArray<FName> RequiredStartupUpgradeIds;

	UPROPERTY(EditAnywhere, Category = "World|ContentValidation")
	TArray<FName> RequiredStartupPortIds;

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
