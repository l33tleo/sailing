#pragma once

#include "CoreMinimal.h"
#include "GameFramework/SaveGame.h"
#include "SaveGameSailing.generated.h"

USTRUCT(BlueprintType)
struct FIslandData
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadWrite, Category = "Island")
	FIntPoint ChunkCoord;

	UPROPERTY(BlueprintReadWrite, Category = "Island")
	int32 IslandIndex = 0;

	UPROPERTY(BlueprintReadWrite, Category = "Island")
	FString IslandName;

	UPROPERTY(BlueprintReadWrite, Category = "Island")
	FVector WorldLocation = FVector::ZeroVector;

	UPROPERTY(BlueprintReadWrite, Category = "Island")
	bool bDiscovered = false;

	UPROPERTY(BlueprintReadWrite, Category = "Island")
	FDateTime DiscoveredTime;

	// Unique key for map storage
	FString GetUniqueKey() const
	{
		return FString::Printf(TEXT("%d_%d_%d"), ChunkCoord.X, ChunkCoord.Y, IslandIndex);
	}
};

UCLASS()
class SAILING_API USaveGameSailing : public USaveGame
{
	GENERATED_BODY()

public:
	USaveGameSailing();

	static const int32 CurrentSaveSchemaVersion = 2;

	UPROPERTY(BlueprintReadWrite, Category = "Save")
	int32 SaveSchemaVersion = CurrentSaveSchemaVersion;

	UPROPERTY(BlueprintReadWrite, Category = "Save")
	TMap<FString, FIslandData> DiscoveredIslands;

	UPROPERTY(BlueprintReadWrite, Category = "Save")
	int32 TotalIslandsDiscovered = 0;

	UPROPERTY(BlueprintReadWrite, Category = "Save")
	FVector LastPlayerLocation = FVector::ZeroVector;

	UPROPERTY(BlueprintReadWrite, Category = "Save|Economy")
	int32 PlayerCredits = 0;

	UPROPERTY(BlueprintReadWrite, Category = "Save|Economy")
	TArray<FName> UnlockedUpgradeIds;

	UPROPERTY(BlueprintReadWrite, Category = "Save|Mission")
	FName ActiveMissionId = NAME_None;

	UPROPERTY(BlueprintReadWrite, Category = "Save|Telemetry")
	TMap<FName, int32> TelemetryCounters;

	// Helper functions
	bool IsIslandDiscovered(FIntPoint ChunkCoord, int32 IslandIndex) const;
	void MarkIslandDiscovered(const FIslandData& IslandData);
	FIslandData* GetIslandData(FIntPoint ChunkCoord, int32 IslandIndex);
	void EnsureCompatibility();

	static const FString SaveSlotName;
};
