#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Math/RandomStream.h"
#include "WindActor.generated.h"

/**
 * Global vindkilde med en «levende» vindmodell.
 *
 *  - Hovedretningen står i ro innenfor et segment (ingen konstant spinning), men vinden vugger
 *    frem og tilbake rundt den (Perlin-oscillasjon + jitter).
 *  - Med jevne mellomrom (~MeanChangeInterval) velges en NY tilfeldig hovedretning som vinden
 *    dreier sakte mot (TransitionSpeed).
 *  - Hvert segment kan tilfeldig bli «ustabilt»: da vandrer vinden mer (større oscillasjon) og
 *    tiden til neste retningsskifte blir kortere.
 *  - Styrke varierer via en langsom konvolutt, og kast er et romlig felt som driver nedvinds.
 *
 * Segment-valgene styres av en FRandomStream seedet med WindSeed → reproduserbart per seed.
 */
UCLASS()
class SAILING_API AWindActor : public AActor
{
	GENERATED_BODY()

public:
	AWindActor();

	virtual void Tick(float DeltaTime) override;

	/** Ambient (global) vindretning som enhetsvektor, inkl. oscillasjon. Brukes av HUD. */
	UFUNCTION(BlueprintPure, Category = "Wind")
	FVector GetWindDirection() const;

	/** Ambient (global) vindstyrke uten lokale kast. Brukes av HUD. */
	UFUNCTION(BlueprintPure, Category = "Wind")
	float GetWindStrength() const;

	/** Kast-intensitet 0..1 på en gitt verdensposisjon (0 = ingen kast, 1 = fullt kast). */
	UFUNCTION(BlueprintPure, Category = "Wind")
	float GetGustFactorAt(const FVector& WorldPos) const;

	/** Full lokal vindvektor (retning × styrke inkl. lokalt kast) på en verdensposisjon. */
	UFUNCTION(BlueprintPure, Category = "Wind")
	FVector GetWindVelocityAt(const FVector& WorldPos) const;

	/** Nåværende hovedretning (uten oscillasjon). Referanse for vri-indikator. */
	UFUNCTION(BlueprintPure, Category = "Wind")
	FVector GetMeanWindDirection() const;

	/** Signert vri (grader) av nåværende retning relativt til hovedretningen: + = høyre, - = venstre. */
	UFUNCTION(BlueprintPure, Category = "Wind")
	float GetWindShiftDeg() const;

	/** Hvor ustabilt nåværende segment er (0 = rolig, 1 = svært ustabil). */
	UFUNCTION(BlueprintPure, Category = "Wind")
	float GetInstability() const { return CurrentInstability; }

	// --- Grunnleggende ---

	/** Start-hovedretning (normaliseres). Endres deretter av segment-systemet. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wind")
	FVector WindDirection = FVector(1.0f, 0.0f, 0.0f);

	/** Basestyrke (gjennomsnittlig vind, spill-enheter). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wind", meta = (ClampMin = "0"))
	float BaseWindStrength = 1000.0f;

	/** Seed for hele vindfeltet (styrke, kast og segment-valg). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wind")
	int32 WindSeed = 1337;

	// --- Styrke-variasjon (lulls & builds) ---

	/** Amplitude på den langsomme styrke-konvolutten (spill-enheter). 0 = konstant base. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wind|Strength", meta = (ClampMin = "0"))
	float StrengthNoiseAmplitude = 350.0f;

	/** Tidsskala for styrke-konvolutten (lavere = langsommere bygging/avtaging). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wind|Strength", meta = (ClampMin = "0.001", ClampMax = "1"))
	float StrengthNoiseTimeScale = 0.05f;

	// --- Retning: oscillasjon (vugging) ---

	/** Amplitude på vuggingen rundt hovedretningen (grader). Dette er frem-og-tilbake-bevegelsen. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wind|Direction", meta = (ClampMin = "0", ClampMax = "60"))
	float ShiftAmplitudeDeg = 12.0f;

	/** Tidsskala for vuggingen (lavere = lengre, roligere svingninger). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wind|Direction", meta = (ClampMin = "0.001", ClampMax = "1"))
	float ShiftTimeScale = 0.06f;

	/** Amplitude på rask retnings-jitter (grader). Liten, kjapp ujevnhet. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wind|Direction", meta = (ClampMin = "0", ClampMax = "30"))
	float JitterAmplitudeDeg = 3.0f;

	/** Tidsskala for jitter (høyere = raskere wobble). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wind|Direction", meta = (ClampMin = "0.01", ClampMax = "5"))
	float JitterTimeScale = 0.5f;

	// --- Retning: hovedskifte (nytt segment) ---

	/** Korteste tid (s) mellom skifte av hovedretning (rolig vind). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wind|DirectionShift", meta = (ClampMin = "5"))
	float MeanChangeIntervalMin = 50.0f;

	/** Lengste tid (s) mellom skifte av hovedretning (rolig vind). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wind|DirectionShift", meta = (ClampMin = "5"))
	float MeanChangeIntervalMax = 90.0f;

	/** Minste vinkelendring (grader) ved et hovedskifte. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wind|DirectionShift", meta = (ClampMin = "0", ClampMax = "180"))
	float DirectionChangeMinDeg = 30.0f;

	/** Største vinkelendring (grader) ved et hovedskifte. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wind|DirectionShift", meta = (ClampMin = "0", ClampMax = "180"))
	float DirectionChangeMaxDeg = 120.0f;

	/** Hvor sakte vinden dreier mot ny hovedretning (grader/sekund). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wind|DirectionShift", meta = (ClampMin = "0.1", ClampMax = "30"))
	float TransitionSpeedDeg = 3.0f;

	// --- Ustabilitet ---

	/** Sannsynlighet (0..1) for at et nytt segment blir ustabilt. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wind|Instability", meta = (ClampMin = "0", ClampMax = "1"))
	float UnstableChance = 0.4f;

	/** Hvor mye oscillasjonen forsterkes ved full ustabilitet (2 = opptil 3× vugging). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wind|Instability", meta = (ClampMin = "0", ClampMax = "5"))
	float UnstableOscillationBoost = 2.0f;

	/** Faktor på segment-tiden når ustabilt (0.4 = ~40 % av vanlig tid → skifter oftere). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wind|Instability", meta = (ClampMin = "0.05", ClampMax = "1"))
	float UnstableIntervalScale = 0.4f;

	// --- Kast (bevegelige flekker) ---

	/** Hvor mye et fullt kast øker den lokale styrken (0.5 = +50%). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wind|Gusts", meta = (ClampMin = "0", ClampMax = "3"))
	float GustAmplitude = 0.5f;

	/** Romlig skala for kast-flekker (1/størrelse i verdensenheter). Lavere = større flekker. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wind|Gusts", meta = (ClampMin = "0.000001", ClampMax = "0.01"))
	float GustSpatialScale = 0.000067f;

	/** Tidsskala for hvor raskt kast-feltet utvikler seg (forming/oppløsning). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wind|Gusts", meta = (ClampMin = "0.001", ClampMax = "2"))
	float GustTimeScale = 0.1f;

	/** Hvor fort kast-flekkene driver nedvinds (verdensenheter/sekund). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wind|Gusts", meta = (ClampMin = "0"))
	float GustTravelSpeed = 1500.0f;

	/** Terskel (0..0.9) for at støy teller som kast. Høyere = færre, skarpere flekker. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wind|Gusts", meta = (ClampMin = "0", ClampMax = "0.9"))
	float GustThreshold = 0.25f;

private:
	virtual void BeginPlay() override;

	/** Velg nytt segment: ny målretning, ny ustabilitet, ny varighet. */
	void StartNewSegment();

	/** Seed-avhengig offset for å dekorrelere ulike støykanaler. */
	float SeedOffset(float Channel) const;

	// Segment-tilstand (kjøretid).
	FRandomStream RandStream;
	float CurrentBaseYaw = 0.0f;       // nåværende hovedretning (grader), dreier mot målet
	float TargetBaseYaw = 0.0f;        // valgt målretning for inneværende segment
	float SegmentTimeRemaining = 0.0f; // tid igjen før neste hovedskifte
	float CurrentInstability = 0.0f;   // 0..1
};
