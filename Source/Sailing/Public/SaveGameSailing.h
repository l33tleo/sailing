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

	/** When set, this island is a fjord island; GetUniqueKey() returns "Fjord_" + FjordIslandId. */
	UPROPERTY(BlueprintReadWrite, Category = "Island")
	FString FjordIslandId;

	// Unique key for map storage
	FString GetUniqueKey() const
	{
		if (!FjordIslandId.IsEmpty())
		{
			return FString(TEXT("Fjord_")) + FjordIslandId;
		}
		return FString::Printf(TEXT("%d_%d_%d"), ChunkCoord.X, ChunkCoord.Y, IslandIndex);
	}
};

UCLASS()
class SAILING_API USaveGameSailing : public USaveGame
{
	GENERATED_BODY()

public:
	USaveGameSailing();

	UPROPERTY(BlueprintReadWrite, Category = "Save")
	TMap<FString, FIslandData> DiscoveredIslands;

	UPROPERTY(BlueprintReadWrite, Category = "Save")
	int32 TotalIslandsDiscovered = 0;

	UPROPERTY(BlueprintReadWrite, Category = "Save")
	FVector LastPlayerLocation = FVector::ZeroVector;

	// Helper functions
	bool IsIslandDiscovered(FIntPoint ChunkCoord, int32 IslandIndex) const;
	void MarkIslandDiscovered(const FIslandData& IslandData);
	FIslandData* GetIslandData(FIntPoint ChunkCoord, int32 IslandIndex);

	/** True if the fjord island with the given name is in the save. */
	bool IsFjordIslandDiscovered(const FString& IslandName) const;
	/** Save discovery of a fjord island (uses FjordIslandId in Data as key). */
	void MarkFjordIslandDiscovered(const FIslandData& IslandData);

	static const FString SaveSlotName;
};
