#pragma once

#include "CoreMinimal.h"
#include "ProceduralMeshComponent.h"
#include "WakeRibbonComponent.generated.h"

class UWaterBodyComponent;

/**
 * Kjølvann: en strimmel skum som legges igjen bak akterspeilet.
 *
 * En ringbuffer av prøver (posisjon, tverretning, tid, fart) fylles for hver WakeSampleDistance
 * båten har tilbakelagt. Hver frame bygges én trekantstrimmel med to vertekser per prøve (fast
 * topologi, laget én gang) som oppdateres med UpdateMeshSection_LinearColor — verteksbufferet
 * skrives på nytt uten at render-proxyen gjenskapes (ingen MarkRenderStateDirty-hakk).
 * Halvbredden vokser med avstanden akterover (Kelvin-vinkel ≈ 19,5°), alfa i vertexfargen fader
 * med alder og fart, og Z samples fra Water Systems bølgehøyde (samme vei som pongtongene) pluss
 * WakeZOffset, så strimmelen ligger oppå bølgene. Materiale: M_Wake (scripts/create_wake_material.py).
 *
 * Komponenten står i verdensrommet (absolutt transform) og flyttes til båten hver frame;
 * verteksene er relative til den, så float-presisjonen holder i fjordens millioner av uu.
 */
UCLASS(ClassGroup = (Sailing), meta = (BlueprintSpawnableComponent))
class SAILING_API UWakeRibbonComponent : public UProceduralMeshComponent
{
	GENERATED_BODY()

public:
	UWakeRibbonComponent(const FObjectInitializer& ObjectInitializer);

	/** Antall prøver i strimmelen (lengde = WakeSamples * WakeSampleDistance). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sailing|Wake", meta = (ClampMin = "4", ClampMax = "256"))
	int32 WakeSamples = 48;

	/** Tilbakelagt avstand (uu) mellom to prøver. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sailing|Wake", meta = (ClampMin = "10"))
	float WakeSampleDistance = 60.0f;

	/** Hvor lenge (s) skummet er synlig etter at båten passerte. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sailing|Wake", meta = (ClampMin = "0.5"))
	float WakeLifetime = 6.0f;

	/** Halvbredde (uu) ved akterspeilet. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sailing|Wake", meta = (ClampMin = "1"))
	float WakeBaseHalfWidth = 45.0f;

	/** Halv åpningsvinkel (grader) på kjølvannets V. Kelvins vinkel er 19,47°. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sailing|Wake", meta = (ClampMin = "0", ClampMax = "60"))
	float WakeKelvinAngleDeg = 19.47f;

	/** Største halvbredde (uu): skumstripa etter en jolle er smal selv om bølgemønsteret vider seg ut. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sailing|Wake", meta = (ClampMin = "1"))
	float WakeMaxHalfWidth = 150.0f;

	/** Løft over den spurte vannflaten (uu). CPU-spørringen gir Gerstner-HØYDEN i (x,y) uten bølgenes
	 *  horisontale forskyvning, så den tegnede flaten ligger stedvis 10–30 cm høyere og skjuler
	 *  strimmelen (målt: 6 cm → bare nærmeste meter synlig, 40 cm → alt). 22 cm lar bølgetoppene
	 *  bryte opp skummet, som ser naturlig ut. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sailing|Wake", meta = (ClampMin = "0"))
	float WakeZOffset = 22.0f;

	/** Fart (uu/s) der skummet er fullt synlig; lineært svakere under. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sailing|Wake", meta = (ClampMin = "1"))
	float WakeFullSpeed = 300.0f;

	/** Under denne farten (uu/s) legges det ikke igjen skum. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sailing|Wake", meta = (ClampMin = "0"))
	float WakeSpeedThreshold = 80.0f;

	/** Lager mesh-seksjonen (fast topologi) og setter materialet. Kalles én gang fra pawnens BeginPlay. */
	void InitRibbon(UMaterialInterface* Material);

	/**
	 * Oppdater strimmelen. SternPos = punktet skummet slippes fra (verdensrom), Right = båtens
	 * tverretning, Speed = fart forover, Time = spilltid, Ocean = for bølgehøyde (kan være null).
	 */
	void UpdateRibbon(const FVector& SternPos, const FVector& Right, float Speed, float Time, UWaterBodyComponent* Ocean);

	/** Antall gyldige prøver akkurat nå (til logg). */
	int32 GetNumActiveSamples() const { return Count; }

	/** Alder (s) på den eldste prøven som fortsatt er synlig (til logg). */
	float GetOldestAge(float Time) const;

private:
	struct FWakeSample
	{
		FVector Pos = FVector::ZeroVector;
		FVector Right = FVector::RightVector;
		float Time = 0.0f;
		float Speed = 0.0f;
	};

	/** Ringbuffer; Head er nyeste prøve. */
	TArray<FWakeSample> Samples;
	int32 Head = -1;
	int32 Count = 0;
	FVector LastSamplePos = FVector::ZeroVector;
	bool bHasLastSample = false;
	bool bInitialized = false;

	// Gjenbrukte buffere (to vertekser per prøve + to for det levende akterpunktet).
	TArray<FVector> Verts;
	TArray<FVector> Normals;
	TArray<FVector2D> UVs;
	TArray<FLinearColor> Colors;
	TArray<FProcMeshTangent> Tangents;

	const FWakeSample& SampleAt(int32 AgeIndex) const;
	float SurfaceZ(const FVector& P, UWaterBodyComponent* Ocean, float Fallback) const;
};
