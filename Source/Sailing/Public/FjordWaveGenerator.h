#pragma once

#include "CoreMinimal.h"
#include "GerstnerWaterWaves.h"
#include "FjordWaveGenerator.generated.h"

/**
 * Gerstner-bølger for fjorden: vindsjø som går MED vinden, med begrenset retningsspredning, og som
 * kan bytte retning uten at sjøen hopper.
 *
 * Hvorfor egen generator: Gerstner-fasen er forankret i verdens origo (dot(pos, k) − ωt), så en ny
 * retning på et bølgesett flytter hele mønsteret flere meter der båten er (se minnet om
 * RecomputeWaves). Her ligger det i stedet opptil to bølgesett oppå hverandre — det gamle og det nye —
 * med FASTE retninger, og bare vektene endres (overtoning). Vekten skalerer både amplitude og
 * krapphet: den horisontale Gerstner-forskyvningen er Steepness/k, uavhengig av amplituden, så et
 * sett som bare fikk amplituden skrudd ned ville fortsatt dratt vannet sidelengs og «hoppet» når det
 * ble fjernet. Sett med vekt 0 bidrar ingenting og kan fjernes usynlig.
 *
 * Pluginets UGerstnerWaterWaveGeneratorSimple har DirectionAngularSpreadDeg = 1325° som standard:
 * bølger i alle retninger, som gir runde interferensmønstre («spiraler») i stedet for en sjø som
 * går én vei.
 */
UCLASS()
class SAILING_API UFjordWaveGenerator : public UGerstnerWaterWaveGeneratorBase
{
	GENERATED_BODY()

public:
	/** Bølger per sett (under en overtoning er det to sett). */
	UPROPERTY(EditAnywhere, Category = "Waves", meta = (ClampMin = "1", ClampMax = "32"))
	int32 NumWaves = 12;

	UPROPERTY(EditAnywhere, Category = "Waves")
	int32 Seed = 0;

	UPROPERTY(EditAnywhere, Category = "Waves")
	float MinWavelength = 1500.0f;

	UPROPERTY(EditAnywhere, Category = "Waves")
	float MaxWavelength = 7000.0f;

	/** Amplitude før vindskalering (uu). */
	UPROPERTY(EditAnywhere, Category = "Waves")
	float MinAmplitude = 3.0f;

	UPROPERTY(EditAnywhere, Category = "Waves")
	float MaxAmplitude = 20.0f;

	UPROPERTY(EditAnywhere, Category = "Waves")
	float SmallWaveSteepness = 0.3f;

	UPROPERTY(EditAnywhere, Category = "Waves")
	float LargeWaveSteepness = 0.15f;

	/** Største avvik (±grader) fra hovedretningen for bølge 1..N-1 (bølge 0 går rett med vinden). */
	UPROPERTY(EditAnywhere, Category = "Waves", meta = (ClampMin = "0", ClampMax = "90"))
	float DirectionSpreadDeg = 30.0f;

	/** Vindstyrkeskala på amplitudene (endrer ikke fasen). */
	float AmplitudeScale = 1.0f;

	/** Bølgesettene: forplantningsretning (grader, UE-yaw — retningen bølgene GÅR) og vekt 0..1. */
	struct FWaveSet
	{
		float AngleDeg = 0.0f;
		float Weight = 0.0f;
	};
	TArray<FWaveSet, TInlineAllocator<2>> Sets;

	virtual void GenerateGerstnerWaves_Implementation(TArray<FGerstnerWave>& OutWaves) const override;
};
