#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "OceanWaterSetupActor.generated.h"

class AWaterZone;
class AWaterBodyOcean;
class AWindActor;
class UGerstnerWaterWaves;
class UGerstnerWaterWaveGeneratorSimple;

/**
 * Setter opp og eier UE5 Water System-havet for fjordmodus (erstatning for AOceanPlaneActor,
 * som fortsatt brukes i legacy chunk-modus). Spawner en AWaterZone + AWaterBodyOcean dekkende
 * hele den kjente Oslofjord-utstrekningen, kobler til en Gerstner-bølgegenerator, og synker
 * bølgeretning/-styrke mot AWindActor.
 *
 * Water-pluginets editor-only bekvemmelighetsfunksjoner (SetOceanExtent, FillWaterZoneWithOcean)
 * er ikke tilgjengelige i spillkode. Vi oppnår samme resultat ved å skalere AWaterBodyOcean-
 * aktørens transform, siden den GPU-tesselerte vann-renderingen leser OceanExtents/skala direkte
 * hver frame uten behov for editor-only rebuild-kall.
 */
UCLASS()
class SAILING_API AOceanWaterSetupActor : public AActor
{
	GENERATED_BODY()

public:
	AOceanWaterSetupActor();

	virtual void BeginPlay() override;
	virtual void Tick(float DeltaTime) override;

	/** Senter for vann-sonen i verdensrom (default: midtpunkt av Oslofjord-DTM-bboxen). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sailing|Ocean")
	FVector ZoneCenter = FVector(-684000.0f, -1487000.0f, 0.0f);

	/** Størrelse på vann-sonen (X, Y) i Unreal-enheter. Dekker DTM-bboxen (~20x34,6 km) + margin. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sailing|Ocean")
	FVector2D ZoneExtent = FVector2D(2400000.0f, 4000000.0f);

	/** Om havets render-bounds dekket hele vannsonen rett etter oppsettet (før vannmeshen bygges).
	 *  Leses av vannsjekken (UFjordBenchmarkComponent, -FjordWaterCheck). */
	UPROPERTY(VisibleInstanceOnly, Category = "Sailing|Ocean")
	bool bOceanBoundsCoveredZoneAtSetup = false;

	/** Oppløsning (px per side) på vannsonens water info-tekstur (motorens standard er 512). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sailing|Ocean", meta = (ClampMin = "256", ClampMax = "4096"))
	int32 WaterInfoResolution = 2048;

	/** Vannivå (Z), matcher AOceanPlaneActor::WaterLevel / SailboatPawn::WaterZ (begge 100). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sailing|Ocean")
	float WaterLevel = 100.0f;

	// --- Gerstner-bølgetuning (startpunkt konvertert fra AOceanPlaneActors sinusbølge-frekvenser) ---

	/** Vindsjø i fjordskala (begrenset strøk, ~9 m/s): bølgelengde 4–40 m, dominerende ~10 m og
	 *  amplitude opptil 15 cm. De gamle verdiene (15–70 m, 20 cm) ga lange, trege dønninger som knapt
	 *  syntes, så det virvlende tekstur-normalkartet dominerte bildet («spiraler»). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sailing|Ocean|Waves", meta = (ClampMin = "1", ClampMax = "32"))
	int32 NumWaves = 12;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sailing|Ocean|Waves", meta = (ClampMin = "0"))
	float MinWavelength = 400.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sailing|Ocean|Waves", meta = (ClampMin = "0"))
	float MaxWavelength = 4000.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sailing|Ocean|Waves", meta = (ClampMin = "0"))
	float MinAmplitude = 4.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sailing|Ocean|Waves", meta = (ClampMin = "0"))
	float MaxAmplitude = 15.0f;

	/** Krapphet for de korteste bølgene (motor-default 0.4). Litt roligere fjord-krusning her;
	 *  med disse verdiene stamper en 2,3 m Optimist ca. ±3° (målt i PIE). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sailing|Ocean|Waves", meta = (ClampMin = "0", ClampMax = "1"))
	float SmallWaveSteepness = 0.3f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sailing|Ocean|Waves", meta = (ClampMin = "0", ClampMax = "1"))
	float LargeWaveSteepness = 0.15f;

	/** Hvor mye vindstyrke skalerer bølgeamplituden (erstatter GustChopBoost-idéen, nå global). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sailing|Ocean|Waves", meta = (ClampMin = "0"))
	float WindAmplitudeScale = 1.0f;

	/** Vannets absorpsjons-DISTANSE per kanal (uu; høyere = lyset når lenger = lysere/klarere farge).
	 *  Pluginets default (10,150,350) gir tropisk turkis; kort rød + moderat grønn/blå gir mørkt,
	 *  blågrønt Oslofjord-vann (tunet live i PIE med skjermbilder). A beholdes fra materialet. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sailing|Water|Look")
	FLinearColor WaterAbsorption = FLinearColor(8.0f, 45.0f, 60.0f, 8.0f);

	/** Styrken på vannmaterialets tekstur-detaljnormaler (WaterTextureSurface). De panoreres av fire
	 *  panner-noder i fire motsatte diagonale retninger (pluginets WaterTextureWaves) — uten netto
	 *  retning ser bevegelsen ut som virvler/spiraler. Skrus ned så Gerstner-sjøen (som går med
	 *  vinden) dominerer; materialets standard er 0.25 / 0.3 / 0.25. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sailing|Water|Look", meta = (ClampMin = "0"))
	float NearDetailNormalStrength = 0.12f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sailing|Water|Look", meta = (ClampMin = "0"))
	float DistantDetailNormalStrength = 0.1f;

	/** Spredningsfarge (default hvit = melkeaktig/tropisk). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sailing|Water|Look")
	FLinearColor WaterScattering = FLinearColor(0.25f, 0.4f, 0.45f, 0.5f);

	/** Hvor ofte (sekunder) vind-tilstanden leses og bølgene rebygges. Lav frekvens: unngår hakking. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sailing|Ocean|Waves", meta = (ClampMin = "0.1"))
	float WindSyncInterval = 2.0f;

	/** Vinddreining (grader) som må til før bølgeRETNINGEN synkes på nytt. Hver synk får sjøen til å
	 *  hoppe synlig (fasen er forankret i origo), så 0 = aldri (retningen låses ved spillstart). */
	/** Når middelvinden har dreid så mye (grader) bort fra bølgenes retning, tones et nytt bølgesett
	 *  inn i den nye retningen (UFjordWaveGenerator: ingen fasehopp). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sailing|Ocean|Waves", meta = (ClampMin = "1.0"))
	float WaveDirectionChangeDeg = 20.0f;

	/** Varighet (s) på overtoningen mellom gammel og ny bølgeretning. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sailing|Ocean|Waves", meta = (ClampMin = "1.0"))
	float WaveCrossfadeSeconds = 15.0f;

	/** Oppdateringsintervall (s) for bølgene MENS en overtoning pågår (små, usynlige vektsteg). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sailing|Ocean|Waves", meta = (ClampMin = "0.02"))
	float WaveCrossfadeUpdateInterval = 0.1f;

	/** Største avvik (±grader) fra vindretningen for de enkelte bølgene. Pluginets standard (1325°)
	 *  sender bølger i alle retninger og gir runde interferensmønstre («spiraler»). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sailing|Ocean|Waves", meta = (ClampMin = "0", ClampMax = "90"))
	float WaveDirectionSpreadDeg = 30.0f;

	/** Maks endring i normalisert vindstyrke som legges på bølgeamplituden per synk (myk overgang). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sailing|Ocean|Waves", meta = (ClampMin = "0.001"))
	float MaxAmplitudeStepPerSync = 0.02f;

private:
	UPROPERTY()
	TObjectPtr<AWaterZone> SpawnedZone;

	UPROPERTY()
	TObjectPtr<AWaterBodyOcean> SpawnedOcean;

	UPROPERTY()
	TObjectPtr<UGerstnerWaterWaves> RuntimeWaves;

	UPROPERTY()
	TWeakObjectPtr<AWindActor> CachedWind;

	float TimeSinceWindSync = 0.0f;

	void SpawnWaterZone();
	void SpawnOceanBody();
	void BuildAndAssignWaves();
	void SyncWavesWithWind();
	void ApplyWaterLook();

	/** Sist anvendte vindstyrke på bølgene (amplituden følger vinden i små steg). */
	float AppliedWindStrengthNorm = -1.0f;

	/** Bølgegeneratoren (eid av RuntimeWaves); holder bølgesettene og overtoningen. */
	UPROPERTY()
	TObjectPtr<class UFjordWaveGenerator> WaveGenerator;

	/** Overtoning pågår: sett 1 (nytt) går fra 0 til 1, sett 0 (gammelt) motsatt. */
	bool bWaveCrossfading = false;
	float WaveCrossfadeAlpha = 0.0f;
	float TimeSinceCrossfadeUpdate = 0.0f;

	/** Avanserer overtoningen og regenererer bølgene. */
	void TickWaveCrossfade(float DeltaTime);

	/** Vannflatens høyde (inkl. bølger) ved spillerbåten, eller −1e6 hvis den ikke kan måles. */
	float SampleWaterHeightAtPlayer() const;
	float CrossfadeMaxJumpCm = 0.0f;
	AWindActor* FindWind();
};
