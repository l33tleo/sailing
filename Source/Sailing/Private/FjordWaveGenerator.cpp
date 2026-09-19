#include "FjordWaveGenerator.h"

void UFjordWaveGenerator::GenerateGerstnerWaves_Implementation(TArray<FGerstnerWave>& OutWaves) const
{
	for (const FWaveSet& Set : Sets)
	{
		if (Set.Weight <= 0.0f)
		{
			continue;
		}
		// Energibevarende overtoning: to uavhengige sett med amplitudefaktor √w og √(1−w) gir samme
		// RMS-bølgehøyde gjennom hele overgangen (lineær vekt ville gitt en «dupp» midt i).
		const float Fade = FMath::Sqrt(FMath::Min(Set.Weight, 1.0f));

		// Samme frø for hvert sett og hver regenerering: et sett med uendret retning gir nøyaktig de
		// samme bølgene (samme fase) hver gang RecomputeWaves kjøres.
		FRandomStream Stream(Seed);
		for (int32 i = 0; i < NumWaves; ++i)
		{
			const float Alpha = 1.0f - static_cast<float>(i) / NumWaves;   // 1 = lengste bølge
			const float Offset = (i == 0) ? 0.0f : Stream.FRandRange(-DirectionSpreadDeg, DirectionSpreadDeg);

			FGerstnerWave& W = OutWaves.AddDefaulted_GetRef();
			const float Rad = FMath::DegreesToRadians(Set.AngleDeg + Offset);
			W.Direction = FVector(FMath::Cos(Rad), FMath::Sin(Rad), 0.0f);
			W.WaveLength = FMath::Lerp(MinWavelength, MaxWavelength, Alpha * Alpha);
			W.Amplitude = FMath::Max(FMath::Lerp(MinAmplitude, MaxAmplitude, Alpha * Alpha) * AmplitudeScale * Fade, 0.0001f);
			W.Steepness = FMath::Lerp(LargeWaveSteepness, SmallWaveSteepness, static_cast<float>(i) / NumWaves) * Fade;
		}
	}
}
