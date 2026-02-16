#include "Simulation/SailingSimulationMath.h"

float USailingSimulationMath::CalculateForceMultiplier(float AngleToWindDegrees, const FSailingPolarSettings& Settings)
{
	const float ClampedNoGo = FMath::Clamp(Settings.NoGoZoneAngle, 1.0f, 89.0f);
	const float Angle = FMath::Clamp(AngleToWindDegrees, 0.0f, 180.0f);

	float ForceMultiplier = 0.0f;
	if (Angle < ClampedNoGo)
	{
		ForceMultiplier = 0.0f;
	}
	else if (Angle < 90.0f)
	{
		const float T = (Angle - ClampedNoGo) / (90.0f - ClampedNoGo);
		ForceMultiplier = FMath::Lerp(Settings.CloseHauledForce, Settings.BeamReachForce, T);
	}
	else if (Angle < 135.0f)
	{
		const float T = (Angle - 90.0f) / 45.0f;
		ForceMultiplier = FMath::Lerp(Settings.BeamReachForce, Settings.BroadReachForce, T);
	}
	else
	{
		const float T = (Angle - 135.0f) / 45.0f;
		ForceMultiplier = FMath::Lerp(Settings.BroadReachForce, Settings.RunningForce, T);
	}

	return FMath::Max(0.0f, ForceMultiplier);
}

float USailingSimulationMath::CalculateSailForce(float ApparentWindStrength, float AngleToWindDegrees, const FSailingPolarSettings& Settings)
{
	const float Wind = FMath::Max(0.0f, ApparentWindStrength);
	const float Multiplier = CalculateForceMultiplier(AngleToWindDegrees, Settings);
	const float Sharpness = FMath::Clamp(Settings.PolarSharpness, 0.1f, 4.0f);
	return Wind * FMath::Pow(Multiplier, Sharpness);
}
