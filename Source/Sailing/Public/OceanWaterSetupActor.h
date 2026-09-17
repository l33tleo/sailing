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

	/** Vannivå (Z), matcher AOceanPlaneActor::WaterLevel / SailboatPawn::WaterZ (begge 100). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sailing|Ocean")
	float WaterLevel = 100.0f;

	// --- Gerstner-bølgetuning (startpunkt konvertert fra AOceanPlaneActors sinusbølge-frekvenser) ---

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sailing|Ocean|Waves", meta = (ClampMin = "1", ClampMax = "64"))
	int32 NumWaves = 12;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sailing|Ocean|Waves", meta = (ClampMin = "0"))
	float MinWavelength = 1500.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sailing|Ocean|Waves", meta = (ClampMin = "0"))
	float MaxWavelength = 7000.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sailing|Ocean|Waves", meta = (ClampMin = "0"))
	float MinAmplitude = 3.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sailing|Ocean|Waves", meta = (ClampMin = "0"))
	float MaxAmplitude = 20.0f;

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

	/** Spredningsfarge (default hvit = melkeaktig/tropisk). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sailing|Water|Look")
	FLinearColor WaterScattering = FLinearColor(0.25f, 0.4f, 0.45f, 0.5f);

	/** Hvor ofte (sekunder) vind-tilstanden leses og bølgene rebygges. Lav frekvens: unngår hakking. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sailing|Ocean|Waves", meta = (ClampMin = "0.1"))
	float WindSyncInterval = 2.0f;

	/** Vinddreining (grader) som må til før bølgeRETNINGEN synkes på nytt. Hver synk får sjøen til å
	 *  hoppe synlig (fasen er forankret i origo), så 0 = aldri (retningen låses ved spillstart). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sailing|Ocean|Waves", meta = (ClampMin = "0.0"))
	float WaveDirectionResyncDeg = 0.0f;

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

	/** Sist anvendte vindvinkel/-styrke på bølgene (se SyncWavesWithWind for hvorfor dette strupes). */
	float AppliedWindAngleDeg = 0.0f;
	float AppliedWindStrengthNorm = -1.0f;
	AWindActor* FindWind();
};
