#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Simulation/SailingSimulationMath.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FSailingNoGoZoneTest,
	"Sailing.Simulation.Polar.NoGoZoneYieldsZero",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FSailingNoGoZoneTest::RunTest(const FString& Parameters)
{
	FSailingPolarSettings Settings;
	Settings.NoGoZoneAngle = 45.0f;
	Settings.PolarSharpness = 1.0f;

	const float Force = USailingSimulationMath::CalculateSailForce(1000.0f, 20.0f, Settings);
	TestEqual(TEXT("No-go angle should produce zero sail force"), Force, 0.0f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FSailingBeamReachPeakTest,
	"Sailing.Simulation.Polar.BeamReachOutperformsOtherAngles",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FSailingBeamReachPeakTest::RunTest(const FString& Parameters)
{
	FSailingPolarSettings Settings;
	Settings.NoGoZoneAngle = 45.0f;
	Settings.CloseHauledForce = 0.3f;
	Settings.BeamReachForce = 1.0f;
	Settings.BroadReachForce = 0.5f;
	Settings.RunningForce = 0.35f;
	Settings.PolarSharpness = 1.0f;

	const float Wind = 1000.0f;
	const float CloseHauled = USailingSimulationMath::CalculateSailForce(Wind, 50.0f, Settings);
	const float BeamReach = USailingSimulationMath::CalculateSailForce(Wind, 90.0f, Settings);
	const float Running = USailingSimulationMath::CalculateSailForce(Wind, 180.0f, Settings);

	TestTrue(TEXT("Beam reach should be stronger than close-hauled"), BeamReach > CloseHauled);
	TestTrue(TEXT("Beam reach should be stronger than running"), BeamReach > Running);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FSailingPolarSharpnessTest,
	"Sailing.Simulation.Polar.SharpnessPenalizesSuboptimalAngles",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FSailingPolarSharpnessTest::RunTest(const FString& Parameters)
{
	FSailingPolarSettings LinearSettings;
	LinearSettings.PolarSharpness = 1.0f;

	FSailingPolarSettings SharpSettings = LinearSettings;
	SharpSettings.PolarSharpness = 2.0f;

	const float Wind = 1000.0f;
	const float Angle = 120.0f;
	const float LinearForce = USailingSimulationMath::CalculateSailForce(Wind, Angle, LinearSettings);
	const float SharpForce = USailingSimulationMath::CalculateSailForce(Wind, Angle, SharpSettings);

	TestTrue(TEXT("Higher sharpness should reduce force away from optimal angle"), SharpForce < LinearForce);
	return true;
}

#endif
