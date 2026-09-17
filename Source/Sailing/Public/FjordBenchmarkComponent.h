#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "FjordBenchmarkComponent.generated.h"

class ACameraActor;

/**
 * Måleverktøy for land-/øyarbeidet: parkerer kamera på faste stasjoner ved referanseøyer og
 * tar skjermbilder (-FjordShots) og/eller måler fps (-FjordBench), og avslutter så spillet.
 * Opprettes av ASailingGameMode kun når ett av flaggene er gitt på kommandolinjen.
 *
 *   -FjordShots            HighResShot per stasjon til renders/landscape/<label>/<stasjon>.png
 *   -FjordBench            logger «[FPSBENCH] stasjon=… snitt=… p1=…» per stasjon
 *   -FjordLabel=<navn>     undermappe/merkelapp (standard «baseline»)
 *
 * Vinden er allerede deterministisk (AWindActor::WindSeed), så samme stasjon gir sammenlignbare
 * bilder mellom kjøringer.
 */
UCLASS()
class SAILING_API UFjordBenchmarkComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UFjordBenchmarkComponent();

	/** True hvis kommandolinjen ber om skjermbilder eller fps-benk. */
	static bool IsRequestedOnCommandLine();

	virtual void BeginPlay() override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType,
		FActorComponentTickFunction* ThisTickFunction) override;

	/** Sekunder kamera står i ro før første måling/bilde (teksturstrømming, shader-kompilering). */
	UPROPERTY(EditAnywhere, Category = "Benchmark")
	float FirstStationWarmupSeconds = 8.0f;

	UPROPERTY(EditAnywhere, Category = "Benchmark")
	float StationWarmupSeconds = 3.0f;

	UPROPERTY(EditAnywhere, Category = "Benchmark")
	float BenchSeconds = 20.0f;

	/** Ventetid etter HighResShot slik at bildet rekker å bli skrevet før kamera flyttes. */
	UPROPERTY(EditAnywhere, Category = "Benchmark")
	float ShotSettleSeconds = 2.0f;

private:
	struct FStation
	{
		const TCHAR* Name;
		FVector CameraLocation;
		FVector LookAt;
	};

	enum class EPhase : uint8 { Warmup, ShotSettle, Bench, Done };

	void EnterStation(int32 Index);
	void AdvancePhase();
	void FinishBench();

	TArray<FStation> Stations;
	int32 StationIndex = 0;
	EPhase Phase = EPhase::Warmup;
	float PhaseTime = 0.0f;
	TArray<float> FrameTimes;

	bool bShots = false;
	bool bBench = false;
	FString Label = TEXT("baseline");

	UPROPERTY()
	TObjectPtr<ACameraActor> Camera;
};
