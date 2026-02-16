#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "FjordMapData.generated.h"

/** Definition of a single island in the fjord map (real name and position). */
USTRUCT(BlueprintType)
struct FFjordIslandDef
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Fjord")
	FString Name;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Fjord")
	FVector2D Position = FVector2D::ZeroVector;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Fjord", meta = (ClampMin = "0.1"))
	float Scale = 3.0f;
};

/** Data asset for Oslofjord map: coastline polygon(s) and island definitions. */
UCLASS(BlueprintType)
class SAILING_API UFjordMapData : public UDataAsset
{
	GENERATED_BODY()

public:
	/** Coastline as closed polygon (world X,Y in Unreal units). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Fjord")
	TArray<FVector2D> CoastlinePoints;

	/** Islands with real names and positions (world X,Y). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Fjord")
	TArray<FFjordIslandDef> Islands;

	/** World origin offset (e.g. Oslo harbour). Added to all positions when interpreting data. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Fjord")
	FVector2D WorldOrigin = FVector2D::ZeroVector;

	/** Scale: 1.0 = 1 Unreal unit per meter. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Fjord", meta = (ClampMin = "0.1"))
	float MetersPerUnit = 1.0f;
};
