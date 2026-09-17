#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "FjordIslandBakeData.generated.h"

class UStaticMesh;

/** Alle instanser av én mesh på én øy, pakket som 5 float per instans:
 *  X, Y, Z (cm, øyas lokale rom — samme som den bakte terrengmeshen), yaw (grader), uniform skala.
 *  Pakket fordi Python-importen setter hundretusener av instanser; en TArray<float> tildeles i ett. */
USTRUCT(BlueprintType)
struct FFjordInstanceSet
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = "Fjord")
	TSoftObjectPtr<UStaticMesh> Mesh;

	UPROPERTY(EditAnywhere, Category = "Fjord")
	TArray<float> Packed;

	/** Instanser lenger unna enn dette (uu) tegnes ikke. 0 = aldri cull. */
	UPROPERTY(EditAnywhere, Category = "Fjord")
	float CullDistance = 0.0f;

	UPROPERTY(EditAnywhere, Category = "Fjord")
	bool bCastShadow = true;

	static constexpr int32 Stride = 5;
};

/** Offline-bakte data per øy utover terrengmeshen (scripts/bake/bake_vegetation.py →
 *  scripts/bake/import_vegetation.py). Foreløpig vegetasjon; bygg/brygger kommer i samme asset. */
UCLASS(BlueprintType)
class SAILING_API UFjordIslandBakeData : public UDataAsset
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, Category = "Fjord")
	TArray<FFjordInstanceSet> InstanceSets;
};
