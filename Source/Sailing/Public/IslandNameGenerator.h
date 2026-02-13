#pragma once

#include "CoreMinimal.h"
#include "IslandNameGenerator.generated.h"

UCLASS()
class SAILING_API UIslandNameGenerator : public UObject
{
	GENERATED_BODY()

public:
	// Generate a deterministic Nordic-style island name based on coordinates
	UFUNCTION(BlueprintCallable, Category = "Island")
	static FString GenerateIslandName(FIntPoint ChunkCoord, int32 IslandIndex);

private:
	static const TArray<FString>& GetPrefixes();
	static const TArray<FString>& GetMiddles();
	static const TArray<FString>& GetSuffixes();
};
