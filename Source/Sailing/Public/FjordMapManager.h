#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "FjordMapData.h"
#include "FjordGeometry.h"
#include "FjordMapManager.generated.h"

class AIslandActor;
class USaveGameSailing;

UCLASS()
class SAILING_API AFjordMapManager : public AActor
{
	GENERATED_BODY()

public:
	AFjordMapManager();

	virtual void BeginPlay() override;

	/** Fjord map data (islands + coastline). If null, loaded from FjordMapDataPath. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Fjord")
	TObjectPtr<UFjordMapData> FjordMapData;

	/** Path to load FjordMapData from if FjordMapData is null (e.g. /Game/Fjord/FjordMapData). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Fjord")
	FSoftObjectPath FjordMapDataPath;

	/** Z height for spawned islands (water level). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Fjord")
	float WaterZ = 100.0f;

	/** Scale factor for island distances. Real meters × this = Unreal units. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Fjord")
	float DistanceScale = 100.0f;

	/** Island actor class to spawn. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Fjord")
	TSubclassOf<AIslandActor> IslandClass;

	/** Vertical exaggeration of real DTM elevation (1.0 = true scale; 0.4 = calmer, still real relief). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Fjord", meta = (ClampMin = "0.1"))
	float HeightExaggeration = 0.4f;

	void SetSaveGame(USaveGameSailing* InSaveGame);

	/** Shared elevation grid (loaded in BeginPlay), reused by the coastline actor. */
	TSharedPtr<FjordGeometry::FHeightGrid> GetHeightGrid() const { return HeightGrid; }

private:
	UPROPERTY()
	TObjectPtr<USaveGameSailing> SaveGame;

	TSharedPtr<FjordGeometry::FHeightGrid> HeightGrid;

	UFUNCTION()
	void OnIslandDiscovered(AIslandActor* Island, const FString& IslandName);
};
