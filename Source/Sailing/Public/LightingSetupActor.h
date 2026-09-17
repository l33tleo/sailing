#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "LightingSetupActor.generated.h"

/**
 * Legger til et globalt PostProcessVolume (mangler helt i MainOcean.umap i dag) og justerer
 * eksisterende DirectionalLight/SkyAtmosphere-aktører allerede plassert i nivået, for en mer
 * fotorealistisk "gyllen time"-sjøfølelse. VolumetricCloud/ExponentialHeightFog-tetthet er bevisst
 * IKKE rørt her — de er svært scene-avhengige og bør tunes visuelt i editoren, ikke blindt i kode.
 * Unntak: skyenes sample-antall (ren ytelsesknapp, målt med ProfileGPU) settes herfra.
 */
UCLASS()
class SAILING_API ALightingSetupActor : public AActor
{
	GENERATED_BODY()

public:
	ALightingSetupActor();

	virtual void BeginPlay() override;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sailing|Lighting")
	float AutoExposureBias = 0.3f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sailing|Lighting")
	float BloomIntensity = 0.5f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sailing|Lighting")
	float VignetteIntensity = 0.3f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sailing|Lighting", meta = (ClampMin = "0", ClampMax = "2"))
	float ColorSaturation = 0.95f;

	/** Sol-intensitet (lux) for lav "gyllen time"-vinkel. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sailing|Lighting")
	float SunIntensityLux = 7.0f;

	/** Sol-fargetemperatur (Kelvin). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sailing|Lighting")
	float SunTemperatureK = 4400.0f;

	/** Sol-høyde over horisonten (grader). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sailing|Lighting")
	float SunElevationDeg = 20.0f;

	/** Luftperspektiv-skalering (dis over lange siktlinjer). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sailing|Lighting")
	float AerialPerspectiveViewDistanceScale = 1.5f;

	/** Skalering av VolumetricCloud ray-march-samples (1.0 = motorens default, ~20 ms GPU på Mac). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sailing|Lighting|Performance", meta = (ClampMin = "0.05", ClampMax = "1"))
	float CloudViewSampleCountScale = 0.25f;

	/** Maks sporingsdistanse (km) for skyenes selvskygge. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sailing|Lighting|Performance")
	float CloudShadowTracingDistanceKm = 5.0f;
};
