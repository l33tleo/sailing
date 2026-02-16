#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "SaveGameSailing.h"
#include "Systems/SailingCoreSubsystems.h"
#include "Data/BoatUpgradeDataAsset.h"
#include "Data/SailingMissionDataAsset.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FSailingSaveMigrationRecountsDiscoveredIslandsTest,
	"Sailing.Progression.Save.MigrationRecountsDiscoveredIslands",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FSailingSaveMigrationRecountsDiscoveredIslandsTest::RunTest(const FString& Parameters)
{
	USaveGameSailing* Save = NewObject<USaveGameSailing>();
	Save->SaveSchemaVersion = 1;
	Save->TotalIslandsDiscovered = 0;

	FIslandData IslandA;
	IslandA.ChunkCoord = FIntPoint(0, 0);
	IslandA.IslandIndex = 0;
	IslandA.bDiscovered = true;
	Save->DiscoveredIslands.Add(IslandA.GetUniqueKey(), IslandA);

	FIslandData IslandB;
	IslandB.ChunkCoord = FIntPoint(1, 0);
	IslandB.IslandIndex = 1;
	IslandB.bDiscovered = true;
	Save->DiscoveredIslands.Add(IslandB.GetUniqueKey(), IslandB);

	Save->EnsureCompatibility();

	TestEqual(TEXT("Migration should recalculate discovered counter"), Save->TotalIslandsDiscovered, 2);
	TestEqual(TEXT("Save schema should be upgraded"), Save->SaveSchemaVersion, USaveGameSailing::CurrentSaveSchemaVersion);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FSailingEconomyPurchaseUpgradeTest,
	"Sailing.Progression.Economy.PurchaseUpgradeConsumesCreditsAndUnlocks",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FSailingEconomyPurchaseUpgradeTest::RunTest(const FString& Parameters)
{
	UEconomySubsystem* Economy = NewObject<UEconomySubsystem>();
	Economy->SetCredits(1200);

	UBoatUpgradeDataAsset* Upgrade = NewObject<UBoatUpgradeDataAsset>();
	Upgrade->UpgradeId = TEXT("TrimPackV1");
	Upgrade->CreditCost = 1000;

	const bool bPurchaseSucceeded = Economy->PurchaseUpgrade(Upgrade);

	TestTrue(TEXT("Purchase should succeed with enough credits"), bPurchaseSucceeded);
	TestEqual(TEXT("Credits should be reduced by cost"), Economy->GetCredits(), 200);
	TestTrue(TEXT("Upgrade should be unlocked after purchase"), Economy->IsUpgradeUnlocked(Upgrade->UpgradeId));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FSailingTelemetryCounterSanitizationTest,
	"Sailing.Progression.Telemetry.CounterSanitization",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FSailingTelemetryCounterSanitizationTest::RunTest(const FString& Parameters)
{
	USaveGameSailing* Save = NewObject<USaveGameSailing>();
	Save->TelemetryCounters.Add(TEXT("MissionCompleted"), -5);
	Save->TelemetryCounters.Add(TEXT("CreditsGranted"), 150);
	Save->EnsureCompatibility();

	TestEqual(TEXT("Negative telemetry values should be clamped to zero"), Save->TelemetryCounters.FindRef(TEXT("MissionCompleted")), 0);
	TestEqual(TEXT("Positive telemetry values should remain unchanged"), Save->TelemetryCounters.FindRef(TEXT("CreditsGranted")), 150);

	UTelemetrySubsystem* Telemetry = NewObject<UTelemetrySubsystem>();
	TMap<FName, int32> InCounters;
	InCounters.Add(TEXT("IslandDiscovered"), 3);
	InCounters.Add(TEXT("CreditsGranted"), -10);
	Telemetry->SetAllCounters(InCounters);
	Telemetry->RecordCounterEvent(TEXT("IslandDiscovered"), 2);

	TestEqual(TEXT("SetAllCounters should sanitize negatives"), Telemetry->GetCounterValue(TEXT("CreditsGranted")), 0);
	TestEqual(TEXT("RecordCounterEvent should increment stored values"), Telemetry->GetCounterValue(TEXT("IslandDiscovered")), 5);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FSailingMissionLocationCompletionTest,
	"Sailing.Progression.Mission.LocationAwareCompletion",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FSailingMissionLocationCompletionTest::RunTest(const FString& Parameters)
{
	UMissionSubsystem* MissionSubsystem = NewObject<UMissionSubsystem>();
	USailingMissionDataAsset* Mission = NewObject<USailingMissionDataAsset>();
	Mission->MissionId = TEXT("Delivery_A");
	Mission->MissionType = ESailingMissionType::Delivery;
	Mission->RewardCredits = 450;
	Mission->EndWorldLocation = FVector(1000.0f, 0.0f, 0.0f);
	Mission->bRequireLocationMatch = true;
	Mission->CompletionRadius = 500.0f;

	TestTrue(TEXT("Mission registration should succeed"), MissionSubsystem->RegisterMissionAsset(Mission));
	TestTrue(TEXT("Mission activation should succeed"), MissionSubsystem->ActivateMissionAsset(Mission));

	const int32 WrongLocationReward = MissionSubsystem->CompleteActiveMissionAtLocation(
		ESailingMissionType::Delivery, FVector(3000.0f, 0.0f, 0.0f));
	TestEqual(TEXT("Wrong location should not complete mission"), WrongLocationReward, 0);
	TestEqual(TEXT("Active mission should still be set"), MissionSubsystem->GetActiveMissionId(), Mission->MissionId);

	const int32 CorrectLocationReward = MissionSubsystem->CompleteActiveMissionAtLocation(
		ESailingMissionType::Delivery, FVector(1200.0f, 10.0f, 0.0f));
	TestEqual(TEXT("Correct location should pay reward"), CorrectLocationReward, 450);
	TestEqual(TEXT("Mission should be cleared after completion"), MissionSubsystem->GetActiveMissionId(), NAME_None);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FSailingMissionChainProgressionTest,
	"Sailing.Progression.Mission.FollowUpActivation",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FSailingMissionChainProgressionTest::RunTest(const FString& Parameters)
{
	UMissionSubsystem* MissionSubsystem = NewObject<UMissionSubsystem>();

	USailingMissionDataAsset* MissionA = NewObject<USailingMissionDataAsset>();
	MissionA->MissionId = TEXT("Mission_A");
	MissionA->MissionType = ESailingMissionType::NavigationChallenge;
	MissionA->RewardCredits = 100;
	MissionA->NextMissionId = TEXT("Mission_B");

	USailingMissionDataAsset* MissionB = NewObject<USailingMissionDataAsset>();
	MissionB->MissionId = TEXT("Mission_B");
	MissionB->MissionType = ESailingMissionType::Delivery;
	MissionB->RewardCredits = 200;

	TestTrue(TEXT("Mission A registration should succeed"), MissionSubsystem->RegisterMissionAsset(MissionA));
	TestTrue(TEXT("Mission B registration should succeed"), MissionSubsystem->RegisterMissionAsset(MissionB));
	TestTrue(TEXT("Mission A activation should succeed"), MissionSubsystem->ActivateMissionAsset(MissionA));

	const int32 Reward = MissionSubsystem->CompleteActiveMissionByTrigger(ESailingMissionType::NavigationChallenge);
	TestEqual(TEXT("Mission A reward should be returned"), Reward, 100);
	TestEqual(TEXT("Mission B should auto-activate as follow-up"), MissionSubsystem->GetActiveMissionId(), MissionB->MissionId);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FSailingWorldPortRegistryTest,
	"Sailing.Progression.World.PortRegistry",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FSailingWorldPortRegistryTest::RunTest(const FString& Parameters)
{
	UWorldStreamingSubsystem* WorldSubsystem = NewObject<UWorldStreamingSubsystem>();
	WorldSubsystem->ClearPortPoints();
	WorldSubsystem->RegisterPortPoint(TEXT("Port_A"), FVector(100.0f, 200.0f, 300.0f));
	WorldSubsystem->RegisterPortPoint(TEXT("Port_B"), FVector(-100.0f, 50.0f, 10.0f));

	const TArray<FName> PortIds = WorldSubsystem->GetRegisteredPortIds();
	TestEqual(TEXT("Two port ids should be registered"), PortIds.Num(), 2);

	FVector PortLocation = FVector::ZeroVector;
	const bool bHasPortA = WorldSubsystem->GetPortLocation(TEXT("Port_A"), PortLocation);
	TestTrue(TEXT("Port_A should be queryable"), bHasPortA);
	TestEqual(TEXT("Port_A location should match registered value"), PortLocation, FVector(100.0f, 200.0f, 300.0f));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FSailingMissionFallbackSelectionTest,
	"Sailing.Progression.Mission.FallbackSelectionSkipsCompletedNonRepeatable",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FSailingMissionFallbackSelectionTest::RunTest(const FString& Parameters)
{
	UMissionSubsystem* MissionSubsystem = NewObject<UMissionSubsystem>();

	USailingMissionDataAsset* MissionA = NewObject<USailingMissionDataAsset>();
	MissionA->MissionId = TEXT("Mission_A");
	MissionA->MissionType = ESailingMissionType::NavigationChallenge;
	MissionA->bRepeatable = false;

	USailingMissionDataAsset* MissionB = NewObject<USailingMissionDataAsset>();
	MissionB->MissionId = TEXT("Mission_B");
	MissionB->MissionType = ESailingMissionType::Delivery;
	MissionB->bRepeatable = true;

	MissionSubsystem->RegisterMissionAsset(MissionA);
	MissionSubsystem->RegisterMissionAsset(MissionB);
	MissionSubsystem->SetCompletedMissionIds({ MissionA->MissionId });

	TestTrue(TEXT("Fallback should find a mission"), MissionSubsystem->ActivateFallbackMission());
	TestEqual(TEXT("Fallback should skip completed non-repeatable mission"), MissionSubsystem->GetActiveMissionId(), MissionB->MissionId);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FSailingMissionCycleSelectionTest,
	"Sailing.Progression.Mission.CycleToNextMission",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FSailingMissionCycleSelectionTest::RunTest(const FString& Parameters)
{
	UMissionSubsystem* MissionSubsystem = NewObject<UMissionSubsystem>();

	USailingMissionDataAsset* MissionA = NewObject<USailingMissionDataAsset>();
	MissionA->MissionId = TEXT("Mission_A");
	MissionA->MissionType = ESailingMissionType::NavigationChallenge;
	MissionA->bRepeatable = false;

	USailingMissionDataAsset* MissionB = NewObject<USailingMissionDataAsset>();
	MissionB->MissionId = TEXT("Mission_B");
	MissionB->MissionType = ESailingMissionType::Delivery;
	MissionB->bRepeatable = true;

	MissionSubsystem->RegisterMissionAsset(MissionA);
	MissionSubsystem->RegisterMissionAsset(MissionB);
	MissionSubsystem->SetCompletedMissionIds({ MissionA->MissionId });
	MissionSubsystem->SetActiveMissionId(MissionA->MissionId);

	TestTrue(TEXT("Cycle should select next available mission"), MissionSubsystem->CycleToNextMission());
	TestEqual(TEXT("Cycle should land on Mission_B"), MissionSubsystem->GetActiveMissionId(), MissionB->MissionId);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FSailingEconomyRepairFlowTest,
	"Sailing.Progression.Economy.RepairFlow",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FSailingEconomyRepairFlowTest::RunTest(const FString& Parameters)
{
	UEconomySubsystem* Economy = NewObject<UEconomySubsystem>();
	Economy->SetCredits(1000);
	Economy->SetBoatConditionPercent(70);
	Economy->ApplyBoatWear(15);

	TestEqual(TEXT("Boat condition should decrease with wear"), Economy->GetBoatConditionPercent(), 55);

	const bool bRepairSucceeded = Economy->RepairBoatToFull(2);
	TestTrue(TEXT("Repair should succeed with sufficient credits"), bRepairSucceeded);
	TestEqual(TEXT("Repair should restore full condition"), Economy->GetBoatConditionPercent(), 100);
	TestEqual(TEXT("Repair should consume credits based on missing condition"), Economy->GetCredits(), 910);
	return true;
}

#endif
