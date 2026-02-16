#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "SaveGameSailing.h"
#include "Systems/SailingCoreSubsystems.h"
#include "Data/BoatUpgradeDataAsset.h"
#include "Data/PortDataAsset.h"
#include "Data/SailingMissionDataAsset.h"
#include "UI/PortMissionBoardWidget.h"

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
	Save->LastVisitedPortId = TEXT("HavnNord");
	Save->PortVisitCounts.Add(TEXT("HavnNord"), 3);
	Save->PortVisitCounts.Add(TEXT("HavnVest"), -2);

	FMissionBoardSelectionEntry HistoryA;
	HistoryA.PortId = TEXT("HavnNord");
	HistoryA.MissionId = TEXT("LeveringTilBoye");
	HistoryA.AcceptedTime = FDateTime::UtcNow();
	Save->MissionBoardSelectionHistory.Add(HistoryA);

	FMissionBoardSelectionEntry HistoryInvalid;
	HistoryInvalid.PortId = NAME_None;
	HistoryInvalid.MissionId = TEXT("Broken");
	Save->MissionBoardSelectionHistory.Add(HistoryInvalid);

	Save->EnsureCompatibility();

	TestEqual(TEXT("Migration should recalculate discovered counter"), Save->TotalIslandsDiscovered, 2);
	TestEqual(TEXT("Save schema should be upgraded"), Save->SaveSchemaVersion, USaveGameSailing::CurrentSaveSchemaVersion);
	TestEqual(TEXT("Port visit counts should be clamped"), Save->PortVisitCounts.FindRef(TEXT("HavnVest")), 0);
	TestEqual(TEXT("Mission board history should remove invalid entries"), Save->MissionBoardSelectionHistory.Num(), 1);
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
	TestTrue(TEXT("Upgrade registration should succeed"), Economy->RegisterUpgradeAsset(Upgrade));
	TestNotNull(TEXT("Upgrade lookup by id should return registered asset"), Economy->GetUpgradeAssetById(Upgrade->UpgradeId));
	const TArray<FName> RegisteredUpgradeIds = Economy->GetRegisteredUpgradeIds();
	TestTrue(TEXT("Registered upgrades should include purchased upgrade"), RegisteredUpgradeIds.Contains(Upgrade->UpgradeId));
	TestNull(TEXT("Unknown upgrade lookup should return null"), Economy->GetUpgradeAssetById(TEXT("UnknownUpgrade")));

	UBoatUpgradeDataAsset* UpgradeB = NewObject<UBoatUpgradeDataAsset>();
	UpgradeB->UpgradeId = TEXT("TurnPackV1");
	UpgradeB->CreditCost = 100;
	UpgradeB->MaxSpeedMultiplier = 1.1f;
	UpgradeB->DragMultiplier = 0.9f;
	UpgradeB->TurnRateMultiplier = 1.2f;
	Economy->RegisterUpgradeAsset(UpgradeB);
	Economy->SetUnlockedUpgrades({ Upgrade->UpgradeId, UpgradeB->UpgradeId });

	float MaxSpeedMultiplier = 0.0f;
	float DragMultiplier = 0.0f;
	float TurnRateMultiplier = 0.0f;
	Economy->GetCombinedUpgradeMultipliers(MaxSpeedMultiplier, DragMultiplier, TurnRateMultiplier);
	TestEqual(TEXT("Combined speed multiplier should include unlocked upgrades"), MaxSpeedMultiplier, 1.1f);
	TestEqual(TEXT("Combined drag multiplier should include unlocked upgrades"), DragMultiplier, 0.9f);
	TestEqual(TEXT("Combined turn multiplier should include unlocked upgrades"), TurnRateMultiplier, 1.2f);

	UEconomySubsystem* EconomyWithOverride = NewObject<UEconomySubsystem>();
	EconomyWithOverride->SetCredits(650);
	UBoatUpgradeDataAsset* UpgradeOverride = NewObject<UBoatUpgradeDataAsset>();
	UpgradeOverride->UpgradeId = TEXT("OverrideCostUpgrade");
	UpgradeOverride->CreditCost = 800;
	EconomyWithOverride->RegisterUpgradeAsset(UpgradeOverride);
	TestTrue(TEXT("PurchaseUpgradeById should allow cheaper override cost"),
		EconomyWithOverride->PurchaseUpgradeById(UpgradeOverride->UpgradeId, 500));
	TestEqual(TEXT("PurchaseUpgradeById should spend override amount"), EconomyWithOverride->GetCredits(), 150);
	TestTrue(TEXT("PurchaseUpgradeById should unlock upgrade"), EconomyWithOverride->IsUpgradeUnlocked(UpgradeOverride->UpgradeId));
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

	USailingMissionDataAsset* ZeroRewardMission = NewObject<USailingMissionDataAsset>();
	ZeroRewardMission->MissionId = TEXT("Delivery_Zero");
	ZeroRewardMission->MissionType = ESailingMissionType::Delivery;
	ZeroRewardMission->RewardCredits = 0;
	ZeroRewardMission->EndWorldLocation = FVector(3000.0f, 0.0f, 0.0f);
	ZeroRewardMission->bRequireLocationMatch = true;
	ZeroRewardMission->CompletionRadius = 400.0f;
	TestTrue(TEXT("Zero reward mission registration should succeed"), MissionSubsystem->RegisterMissionAsset(ZeroRewardMission));
	TestTrue(TEXT("Zero reward mission activation should succeed"), MissionSubsystem->ActivateMissionAsset(ZeroRewardMission));

	const int32 ZeroRewardCompletion = MissionSubsystem->CompleteActiveMissionAtLocation(
		ESailingMissionType::Delivery, FVector(3000.0f, 100.0f, 0.0f));
	TestEqual(TEXT("Zero reward mission should complete with zero reward"), ZeroRewardCompletion, 0);
	TestEqual(TEXT("Zero reward mission should still clear active mission"), MissionSubsystem->GetActiveMissionId(), NAME_None);

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
	FSailingMissionObjectiveMarkerConfigTest,
	"Sailing.Progression.Mission.ObjectiveMarkerConfig",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FSailingMissionObjectiveMarkerConfigTest::RunTest(const FString& Parameters)
{
	UMissionSubsystem* MissionSubsystem = NewObject<UMissionSubsystem>();

	USailingMissionDataAsset* DeliveryMission = NewObject<USailingMissionDataAsset>();
	DeliveryMission->MissionId = TEXT("Mission_Delivery");
	DeliveryMission->MissionType = ESailingMissionType::Delivery;
	DeliveryMission->EndWorldLocation = FVector(500.0f, 250.0f, 50.0f);
	DeliveryMission->bRequireLocationMatch = false;
	MissionSubsystem->RegisterMissionAsset(DeliveryMission);

	USailingMissionDataAsset* OptionalMission = NewObject<USailingMissionDataAsset>();
	OptionalMission->MissionId = TEXT("Mission_Optional");
	OptionalMission->MissionType = ESailingMissionType::NavigationChallenge;
	OptionalMission->bRequireLocationMatch = false;
	OptionalMission->EndWorldLocation = FVector(1000.0f, 0.0f, 0.0f);
	MissionSubsystem->RegisterMissionAsset(OptionalMission);

	USailingMissionDataAsset* RequiredMission = NewObject<USailingMissionDataAsset>();
	RequiredMission->MissionId = TEXT("Mission_Required");
	RequiredMission->MissionType = ESailingMissionType::NavigationChallenge;
	RequiredMission->bRequireLocationMatch = true;
	RequiredMission->EndWorldLocation = FVector(1500.0f, -300.0f, 0.0f);
	MissionSubsystem->RegisterMissionAsset(RequiredMission);

	FVector ObjectiveLocation = FVector::ZeroVector;
	ESailingMissionType ObjectiveType = ESailingMissionType::NavigationChallenge;
	TestTrue(TEXT("Delivery mission should require objective marker by mission type"),
		MissionSubsystem->GetMissionObjectiveMarkerConfig(DeliveryMission->MissionId, ObjectiveLocation, ObjectiveType));
	TestEqual(TEXT("Delivery objective location should match mission"), ObjectiveLocation, DeliveryMission->EndWorldLocation);
	TestEqual(TEXT("Delivery objective trigger type should match mission type"), ObjectiveType, ESailingMissionType::Delivery);

	ObjectiveLocation = FVector::ZeroVector;
	ObjectiveType = ESailingMissionType::Delivery;
	TestFalse(TEXT("Non-location mission should not request objective marker"),
		MissionSubsystem->GetMissionObjectiveMarkerConfig(OptionalMission->MissionId, ObjectiveLocation, ObjectiveType));
	MissionSubsystem->SetActiveMissionId(DeliveryMission->MissionId);
	TestTrue(TEXT("Active mission objective lookup should include delivery missions"),
		MissionSubsystem->GetActiveMissionObjectiveLocation(ObjectiveLocation));
	TestEqual(TEXT("Active mission objective location should match delivery mission"), ObjectiveLocation, DeliveryMission->EndWorldLocation);

	ObjectiveLocation = FVector::ZeroVector;
	ObjectiveType = ESailingMissionType::Delivery;
	TestTrue(TEXT("Location-required mission should request objective marker"),
		MissionSubsystem->GetMissionObjectiveMarkerConfig(RequiredMission->MissionId, ObjectiveLocation, ObjectiveType));
	TestEqual(TEXT("Required mission objective location should match mission"), ObjectiveLocation, RequiredMission->EndWorldLocation);
	TestEqual(TEXT("Required mission objective trigger type should match mission type"), ObjectiveType, ESailingMissionType::NavigationChallenge);

	TestFalse(TEXT("Unknown mission should not request objective marker"),
		MissionSubsystem->GetMissionObjectiveMarkerConfig(TEXT("Mission_Unknown"), ObjectiveLocation, ObjectiveType));
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

	WorldSubsystem->MarkPortVisited(TEXT("Port_A"));
	WorldSubsystem->MarkPortVisited(TEXT("Port_A"));
	WorldSubsystem->MarkPortVisited(TEXT("Port_B"));
	TestEqual(TEXT("Last visited port should update"), WorldSubsystem->GetLastVisitedPortId(), FName(TEXT("Port_B")));
	const TMap<FName, int32> VisitCounts = WorldSubsystem->GetPortVisitCounts();
	TestEqual(TEXT("Port_A should have two visits"), VisitCounts.FindRef(TEXT("Port_A")), 2);
	TestEqual(TEXT("Port_B should have one visit"), VisitCounts.FindRef(TEXT("Port_B")), 1);

	TMap<FName, int32> PersistedStats;
	PersistedStats.Add(TEXT("Port_A"), -4);
	PersistedStats.Add(TEXT("Port_C"), 6);
	WorldSubsystem->SetPortVisitStats(TEXT("Port_C"), PersistedStats);
	const TMap<FName, int32> SanitizedStats = WorldSubsystem->GetPortVisitCounts();
	TestEqual(TEXT("SetPortVisitStats should sanitize negative counts"), SanitizedStats.FindRef(TEXT("Port_A")), 0);
	TestEqual(TEXT("SetPortVisitStats should keep positive counts"), SanitizedStats.FindRef(TEXT("Port_C")), 6);
	TestEqual(TEXT("SetPortVisitStats should set last visited port"), WorldSubsystem->GetLastVisitedPortId(), FName(TEXT("Port_C")));
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
	FSailingMissionCandidateSelectionTest,
	"Sailing.Progression.Mission.CandidateSelection",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FSailingMissionCandidateSelectionTest::RunTest(const FString& Parameters)
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

	const bool bActivated = MissionSubsystem->ActivateMissionFromCandidates(
		{ MissionA->MissionId, MissionB->MissionId }, true);
	TestTrue(TEXT("Candidate activation should succeed"), bActivated);
	TestEqual(TEXT("Candidate selector should skip completed non-repeatable mission"), MissionSubsystem->GetActiveMissionId(), MissionB->MissionId);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FSailingPortWeightedOffersTest,
	"Sailing.Progression.Port.WeightedOfferPrioritization",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FSailingPortWeightedOffersTest::RunTest(const FString& Parameters)
{
	TArray<FPortMissionWeightedOffer> WeightedOffers;

	FPortMissionWeightedOffer OfferHigh;
	OfferHigh.MissionId = TEXT("Mission_A");
	OfferHigh.PriorityWeight = 3.0f;
	WeightedOffers.Add(OfferHigh);

	FPortMissionWeightedOffer OfferGated;
	OfferGated.MissionId = TEXT("Mission_B");
	OfferGated.PriorityWeight = 2.0f;
	OfferGated.MinPortVisits = 2;
	WeightedOffers.Add(OfferGated);

	FPortMissionWeightedOffer OfferLow;
	OfferLow.MissionId = TEXT("Mission_C");
	OfferLow.PriorityWeight = 1.0f;
	WeightedOffers.Add(OfferLow);

	const TArray<FName> PrioritizedAtOneVisit = UPortDataAsset::BuildPrioritizedMissionIds(
		WeightedOffers, { TEXT("Fallback_1") }, 1, 3);
	TestEqual(TEXT("Only ungated weighted offers should appear"), PrioritizedAtOneVisit.Num(), 2);
	TestEqual(TEXT("Highest weight should come first"), PrioritizedAtOneVisit[0], FName(TEXT("Mission_A")));
	TestEqual(TEXT("Lower weight should come after"), PrioritizedAtOneVisit[1], FName(TEXT("Mission_C")));

	const TArray<FName> PrioritizedAtThreeVisits = UPortDataAsset::BuildPrioritizedMissionIds(
		WeightedOffers, { TEXT("Fallback_1") }, 3, 2);
	TestEqual(TEXT("Max offer limit should trim result"), PrioritizedAtThreeVisits.Num(), 2);
	TestEqual(TEXT("Top priority mission should remain first"), PrioritizedAtThreeVisits[0], FName(TEXT("Mission_A")));
	TestEqual(TEXT("Second priority mission should be included once gate opens"), PrioritizedAtThreeVisits[1], FName(TEXT("Mission_B")));

	const TArray<FName> FallbackOnly = UPortDataAsset::BuildPrioritizedMissionIds(
		{}, { TEXT("Fallback_1"), TEXT("Fallback_1"), TEXT("Fallback_2") }, 0, 0);
	TestEqual(TEXT("Fallback list should deduplicate mission ids"), FallbackOnly.Num(), 2);
	TestEqual(TEXT("Fallback should preserve first unique order"), FallbackOnly[0], FName(TEXT("Fallback_1")));
	TestEqual(TEXT("Fallback should include subsequent unique ids"), FallbackOnly[1], FName(TEXT("Fallback_2")));

	const FPortMissionOfferSelectionResult MissionSelectionResult = UPortDataAsset::BuildPrioritizedMissionSelection(
		WeightedOffers, { TEXT("Fallback_1") }, 1, 3);
	TestTrue(TEXT("Mission selection should indicate weighted source with eligible rules"), MissionSelectionResult.bUsedWeightedRules);
	TestFalse(TEXT("Mission selection should not indicate fallback when weighted rules are used"), MissionSelectionResult.bUsedFallbackOffers);
	TestEqual(TEXT("Mission selection should track eligible weighted rules"), MissionSelectionResult.EligibleWeightedRuleCount, 2);
	TestEqual(TEXT("Mission selection should track visit-gated rules"), MissionSelectionResult.VisitGatedRuleCount, 1);

	const FPortMissionOfferSelectionResult MissionFallbackSelection = UPortDataAsset::BuildPrioritizedMissionSelection(
		{}, { TEXT("Fallback_1"), TEXT("Fallback_2") }, 0, 1);
	TestFalse(TEXT("Mission fallback selection should not use weighted source"), MissionFallbackSelection.bUsedWeightedRules);
	TestTrue(TEXT("Mission fallback selection should use fallback source"), MissionFallbackSelection.bUsedFallbackOffers);
	TestEqual(TEXT("Mission fallback selection should respect max-offer cap"), MissionFallbackSelection.MissionIds.Num(), 1);

	const TArray<FName> RotatedUpgrades = UPortDataAsset::BuildRotatedUpgradeIds(
		{ TEXT("Upgrade_A"), TEXT("Upgrade_B"), TEXT("Upgrade_C") }, 2, 2, true);
	TestEqual(TEXT("Upgrade rotation should respect max-offer cap"), RotatedUpgrades.Num(), 2);
	TestEqual(TEXT("Upgrade rotation should offset by visit count"), RotatedUpgrades[0], FName(TEXT("Upgrade_C")));
	TestEqual(TEXT("Upgrade rotation should continue circular order"), RotatedUpgrades[1], FName(TEXT("Upgrade_A")));

	TArray<FPortUpgradeWeightedOffer> WeightedUpgradeOffers;
	FPortUpgradeWeightedOffer UpgradeOfferA;
	UpgradeOfferA.UpgradeId = TEXT("Upgrade_A");
	UpgradeOfferA.PriorityWeight = 2.0f;
	WeightedUpgradeOffers.Add(UpgradeOfferA);
	FPortUpgradeWeightedOffer UpgradeOfferB;
	UpgradeOfferB.UpgradeId = TEXT("Upgrade_B");
	UpgradeOfferB.PriorityWeight = 1.0f;
	UpgradeOfferB.MinPortVisits = 2;
	WeightedUpgradeOffers.Add(UpgradeOfferB);
	const TArray<FName> PrioritizedWeightedUpgrades = UPortDataAsset::BuildPrioritizedUpgradeIds(
		WeightedUpgradeOffers, { TEXT("Fallback_Upgrade") }, 2, 2, true);
	TestEqual(TEXT("Weighted upgrade selector should include gated offer once unlocked"),
		PrioritizedWeightedUpgrades.Num(), 2);
	TestEqual(TEXT("Weighted upgrade selector should preserve priority before rotation"),
		PrioritizedWeightedUpgrades[0], FName(TEXT("Upgrade_A")));
	TestEqual(TEXT("Weighted upgrade selector should include second eligible offer"),
		PrioritizedWeightedUpgrades[1], FName(TEXT("Upgrade_B")));

	const TArray<FName> FallbackWeightedUpgrades = UPortDataAsset::BuildPrioritizedUpgradeIds(
		{}, { TEXT("Fallback_Upgrade_A"), TEXT("Fallback_Upgrade_B") }, 1, 1, false);
	TestEqual(TEXT("Weighted upgrade selector should fallback when weighted list is empty"),
		FallbackWeightedUpgrades.Num(), 1);
	TestEqual(TEXT("Weighted upgrade fallback should respect source order"),
		FallbackWeightedUpgrades[0], FName(TEXT("Fallback_Upgrade_A")));

	const FPortUpgradeOfferSelectionResult WeightedSelectionResult = UPortDataAsset::BuildPrioritizedUpgradeSelection(
		WeightedUpgradeOffers,
		{ TEXT("Fallback_Upgrade_A"), TEXT("Fallback_Upgrade_B") },
		1,
		3,
		false);
	TestTrue(TEXT("Selection result should indicate weighted-rules source when eligible offers exist"), WeightedSelectionResult.bUsedWeightedRules);
	TestFalse(TEXT("Selection result should not indicate fallback source when weighted-rules used"), WeightedSelectionResult.bUsedFallbackOffers);
	TestEqual(TEXT("Selection result should include eligible weighted rule count"), WeightedSelectionResult.EligibleWeightedRuleCount, 1);
	TestEqual(TEXT("Selection result should include visit-gated weighted rule count"), WeightedSelectionResult.VisitGatedRuleCount, 1);

	const FPortUpgradeOfferSelectionResult FallbackSelectionResult = UPortDataAsset::BuildPrioritizedUpgradeSelection(
		{},
		{ TEXT("Fallback_Upgrade_A"), TEXT("Fallback_Upgrade_B") },
		1,
		2,
		true);
	TestFalse(TEXT("Fallback selection should not mark weighted rule usage"), FallbackSelectionResult.bUsedWeightedRules);
	TestTrue(TEXT("Fallback selection should mark fallback source"), FallbackSelectionResult.bUsedFallbackOffers);
	TestEqual(TEXT("Fallback selection should respect max count"), FallbackSelectionResult.UpgradeIds.Num(), 2);

	TSet<FName> UnlockedUpgradeIds;
	UnlockedUpgradeIds.Add(TEXT("Upgrade_A"));
	const TArray<FName> FilteredLockedOnly = UPortDataAsset::FilterUpgradeIdsByUnlockedState(
		{ TEXT("Upgrade_A"), TEXT("Upgrade_B"), TEXT("Upgrade_B") }, UnlockedUpgradeIds, true);
	TestEqual(TEXT("Upgrade filter should remove unlocked upgrades when requested"), FilteredLockedOnly.Num(), 1);
	TestEqual(TEXT("Upgrade filter should keep locked upgrades"), FilteredLockedOnly[0], FName(TEXT("Upgrade_B")));

	const TArray<FName> FilteredAll = UPortDataAsset::FilterUpgradeIdsByUnlockedState(
		{ TEXT("Upgrade_A"), TEXT("Upgrade_B") }, UnlockedUpgradeIds, false);
	TestEqual(TEXT("Upgrade filter should keep all upgrades when hide-unlocked disabled"), FilteredAll.Num(), 2);

	int32 RejectedMissionIdCount = 0;
	TSet<FName> AllowedMissionIds;
	AllowedMissionIds.Add(TEXT("Mission_A"));
	AllowedMissionIds.Add(TEXT("Mission_C"));
	const TArray<FName> MissingRequiredMissionIds = UPortDataAsset::FindMissingRequiredIds(
		{ TEXT("Mission_A"), TEXT("Mission_B"), TEXT("Mission_B"), NAME_None },
		AllowedMissionIds);
	TestEqual(TEXT("Missing-required helper should return unique missing mission ids"), MissingRequiredMissionIds.Num(), 1);
	TestEqual(TEXT("Missing-required helper should include expected missing mission id"), MissingRequiredMissionIds[0], FName(TEXT("Mission_B")));
	const TArray<FName> MissingSortedIds = UPortDataAsset::FindMissingRequiredIds(
		{ TEXT("Mission_Z"), TEXT("Mission_B"), TEXT("Mission_C"), TEXT("Mission_B") },
		TSet<FName>());
	TestEqual(TEXT("Missing-required helper should sort ids lexically"), MissingSortedIds.Num(), 3);
	TestEqual(TEXT("Sorted missing ids should place Mission_B first"), MissingSortedIds[0], FName(TEXT("Mission_B")));
	TestEqual(TEXT("Sorted missing ids should place Mission_C second"), MissingSortedIds[1], FName(TEXT("Mission_C")));
	TestEqual(TEXT("Sorted missing ids should place Mission_Z last"), MissingSortedIds[2], FName(TEXT("Mission_Z")));

	const TArray<FName> FilteredMissionIds = UPortDataAsset::FilterIdsByAllowedSet(
		{ NAME_None, TEXT("Mission_A"), TEXT("Mission_B"), TEXT("Mission_A"), TEXT("Mission_C") },
		AllowedMissionIds,
		RejectedMissionIdCount);
	TestEqual(TEXT("Allowed-set mission filter should reject unknown and None ids"), RejectedMissionIdCount, 2);
	TestEqual(TEXT("Allowed-set mission filter should keep unique allowed ids"), FilteredMissionIds.Num(), 2);
	TestEqual(TEXT("Allowed-set mission filter should keep source order for allowed ids"), FilteredMissionIds[0], FName(TEXT("Mission_A")));
	TestEqual(TEXT("Allowed-set mission filter should include second allowed id"), FilteredMissionIds[1], FName(TEXT("Mission_C")));

	int32 RejectedWeightedMissionCount = 0;
	TArray<FPortMissionWeightedOffer> MissionRulesToFilter;
	FPortMissionWeightedOffer MissionRuleA;
	MissionRuleA.MissionId = TEXT("Mission_A");
	MissionRulesToFilter.Add(MissionRuleA);
	FPortMissionWeightedOffer MissionRuleB;
	MissionRuleB.MissionId = TEXT("Mission_B");
	MissionRulesToFilter.Add(MissionRuleB);
	FPortMissionWeightedOffer MissionRuleNone;
	MissionRuleNone.MissionId = NAME_None;
	MissionRulesToFilter.Add(MissionRuleNone);
	FPortMissionWeightedOffer MissionRuleDuplicate;
	MissionRuleDuplicate.MissionId = TEXT("Mission_A");
	MissionRulesToFilter.Add(MissionRuleDuplicate);
	const TArray<FPortMissionWeightedOffer> FilteredMissionRules = UPortDataAsset::FilterMissionWeightedOffersByAllowedSet(
		MissionRulesToFilter,
		AllowedMissionIds,
		RejectedWeightedMissionCount);
	TestEqual(TEXT("Weighted mission filter should reject unknown and None rules"), RejectedWeightedMissionCount, 2);
	TestEqual(TEXT("Weighted mission filter should keep first unique valid rule"), FilteredMissionRules.Num(), 1);
	TestEqual(TEXT("Weighted mission filter should keep expected mission id"), FilteredMissionRules[0].MissionId, FName(TEXT("Mission_A")));

	int32 RejectedWeightedUpgradeCount = 0;
	TSet<FName> AllowedUpgradeIds;
	AllowedUpgradeIds.Add(TEXT("Upgrade_A"));
	TArray<FPortUpgradeWeightedOffer> UpgradeRulesToFilter;
	FPortUpgradeWeightedOffer UpgradeRuleA;
	UpgradeRuleA.UpgradeId = TEXT("Upgrade_A");
	UpgradeRulesToFilter.Add(UpgradeRuleA);
	FPortUpgradeWeightedOffer UpgradeRuleB;
	UpgradeRuleB.UpgradeId = TEXT("Upgrade_B");
	UpgradeRulesToFilter.Add(UpgradeRuleB);
	FPortUpgradeWeightedOffer UpgradeRuleNone;
	UpgradeRuleNone.UpgradeId = NAME_None;
	UpgradeRulesToFilter.Add(UpgradeRuleNone);
	const TArray<FPortUpgradeWeightedOffer> FilteredUpgradeRules = UPortDataAsset::FilterUpgradeWeightedOffersByAllowedSet(
		UpgradeRulesToFilter,
		AllowedUpgradeIds,
		RejectedWeightedUpgradeCount);
	TestEqual(TEXT("Weighted upgrade filter should reject unknown and None rules"), RejectedWeightedUpgradeCount, 2);
	TestEqual(TEXT("Weighted upgrade filter should keep valid rules"), FilteredUpgradeRules.Num(), 1);
	TestEqual(TEXT("Weighted upgrade filter should keep expected upgrade id"), FilteredUpgradeRules[0].UpgradeId, FName(TEXT("Upgrade_A")));

	const TArray<FName> StaticUpgrades = UPortDataAsset::BuildRotatedUpgradeIds(
		{ TEXT("Upgrade_A"), TEXT("Upgrade_A"), TEXT("Upgrade_B") }, 5, 0, false);
	TestEqual(TEXT("Non-rotating upgrade list should deduplicate"), StaticUpgrades.Num(), 2);
	TestEqual(TEXT("Non-rotating first upgrade should preserve source order"), StaticUpgrades[0], FName(TEXT("Upgrade_A")));
	TestEqual(TEXT("Non-rotating second upgrade should preserve source order"), StaticUpgrades[1], FName(TEXT("Upgrade_B")));

	TestEqual(TEXT("Adjusted upgrade cost should apply multiplier"),
		UPortDataAsset::CalculateAdjustedUpgradeCost(1000, 0.85f), 850);
	TestEqual(TEXT("Adjusted upgrade cost should clamp negative base cost"),
		UPortDataAsset::CalculateAdjustedUpgradeCost(-100, 1.3f), 0);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FSailingMissionSwitchConfirmationPolicyTest,
	"Sailing.Progression.Port.MissionSwitchConfirmationPolicy",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FSailingMissionSwitchConfirmationPolicyTest::RunTest(const FString& Parameters)
{
	TestFalse(TEXT("No current mission should not require confirmation"),
		UPortMissionBoardWidget::RequiresMissionSwitchConfirmation(NAME_None, TEXT("Mission_A"), NAME_None));
	TestFalse(TEXT("Switch to same mission should not require confirmation"),
		UPortMissionBoardWidget::RequiresMissionSwitchConfirmation(TEXT("Mission_A"), TEXT("Mission_A"), NAME_None));
	TestTrue(TEXT("Switch to different mission should require first confirmation"),
		UPortMissionBoardWidget::RequiresMissionSwitchConfirmation(TEXT("Mission_A"), TEXT("Mission_B"), NAME_None));
	TestFalse(TEXT("Second click on pending mission should pass confirmation"),
		UPortMissionBoardWidget::RequiresMissionSwitchConfirmation(TEXT("Mission_A"), TEXT("Mission_B"), TEXT("Mission_B")));
	TestTrue(TEXT("Different mission than pending should request new confirmation"),
		UPortMissionBoardWidget::RequiresMissionSwitchConfirmation(TEXT("Mission_A"), TEXT("Mission_C"), TEXT("Mission_B")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FSailingUpgradePurchaseRequestValidationTest,
	"Sailing.Progression.Port.UpgradePurchaseRequestValidation",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FSailingUpgradePurchaseRequestValidationTest::RunTest(const FString& Parameters)
{
	TestFalse(TEXT("Upgrade request should be rejected when service disabled"),
		UPortMissionBoardWidget::IsUpgradePurchaseRequestValid(false, { TEXT("Upgrade_A") }, TEXT("Upgrade_A")));
	TestFalse(TEXT("Upgrade request should reject None id"),
		UPortMissionBoardWidget::IsUpgradePurchaseRequestValid(true, { TEXT("Upgrade_A") }, NAME_None));
	TestTrue(TEXT("Empty offered-list should allow any request when service enabled"),
		UPortMissionBoardWidget::IsUpgradePurchaseRequestValid(true, {}, TEXT("Upgrade_A")));
	TestTrue(TEXT("Configured offered upgrade should be accepted"),
		UPortMissionBoardWidget::IsUpgradePurchaseRequestValid(true, { TEXT("Upgrade_A"), TEXT("Upgrade_B") }, TEXT("Upgrade_B")));
	TestFalse(TEXT("Unconfigured upgrade should be rejected when list is present"),
		UPortMissionBoardWidget::IsUpgradePurchaseRequestValid(true, { TEXT("Upgrade_A"), TEXT("Upgrade_B") }, TEXT("Upgrade_C")));
	TestTrue(TEXT("First click should require upgrade purchase confirmation"),
		UPortMissionBoardWidget::RequiresUpgradePurchaseConfirmation(TEXT("Upgrade_A"), NAME_None));
	TestFalse(TEXT("Second click on same upgrade should pass confirmation"),
		UPortMissionBoardWidget::RequiresUpgradePurchaseConfirmation(TEXT("Upgrade_A"), TEXT("Upgrade_A")));
	TestTrue(TEXT("Switching pending upgrade should require new confirmation"),
		UPortMissionBoardWidget::RequiresUpgradePurchaseConfirmation(TEXT("Upgrade_B"), TEXT("Upgrade_A")));
	TestEqual(TEXT("Pricing helper should return unavailable text when service disabled"),
		UPortMissionBoardWidget::BuildUpgradePricingStatusText(false, 1.0f).ToString(),
		FString(TEXT("Ingen oppgraderingspriser tilgjengelig.")));
	TestEqual(TEXT("Pricing helper should return standard label for multiplier 1.0"),
		UPortMissionBoardWidget::BuildUpgradePricingStatusText(true, 1.0f).ToString(),
		FString(TEXT("Standardpriser aktiv.")));
	TestEqual(TEXT("Pricing helper should return discount label when multiplier below 1"),
		UPortMissionBoardWidget::BuildUpgradePricingStatusText(true, 0.9f).ToString(),
		FString(TEXT("Havnerabatt: x0.90")));
	TestEqual(TEXT("Pricing helper should return surcharge label when multiplier above 1"),
		UPortMissionBoardWidget::BuildUpgradePricingStatusText(true, 1.1f).ToString(),
		FString(TEXT("Havnepåslag: x1.10")));
	TestEqual(TEXT("Mission availability reason should mark disabled board"),
		UPortMissionBoardWidget::DetermineMissionAvailabilityReason(false, false, true),
		EPortMissionAvailabilityReason::MissionBoardDisabled);
	TestEqual(TEXT("Mission availability reason should mark cooldown"),
		UPortMissionBoardWidget::DetermineMissionAvailabilityReason(true, true, true),
		EPortMissionAvailabilityReason::CooldownActive);
	TestEqual(TEXT("Mission availability reason should mark no offers"),
		UPortMissionBoardWidget::DetermineMissionAvailabilityReason(true, false, false),
		EPortMissionAvailabilityReason::NoOffers);
	TestEqual(TEXT("Mission availability reason should mark ready"),
		UPortMissionBoardWidget::DetermineMissionAvailabilityReason(true, false, true),
		EPortMissionAvailabilityReason::Ready);
	TestEqual(TEXT("Mission offer source should be none when board disabled"),
		UPortMissionBoardWidget::DetermineMissionOfferSource(false, true, false),
		EPortMissionOfferSource::None);
	TestEqual(TEXT("Mission offer source should prefer weighted rules"),
		UPortMissionBoardWidget::DetermineMissionOfferSource(true, true, true),
		EPortMissionOfferSource::WeightedRules);
	TestEqual(TEXT("Mission offer source should report fallback when weighted unavailable"),
		UPortMissionBoardWidget::DetermineMissionOfferSource(true, false, true),
		EPortMissionOfferSource::FallbackList);
	TestEqual(TEXT("Mission offer source status should include weighted and gated counts"),
		UPortMissionBoardWidget::BuildMissionOfferSourceStatusText(
			EPortMissionOfferSource::WeightedRules, 3, 2).ToString(),
		FString(TEXT("Vektede oppdragsregler aktiv (3 kvalifiserte). 2 regel(er) låst av havnebesøk.")));
	TestEqual(TEXT("Mission visit requirement helper should indicate no requirement"),
		UPortMissionBoardWidget::BuildMissionVisitRequirementStatusText(0, 4).ToString(),
		FString(TEXT("Ingen havnekrav.")));
	TestEqual(TEXT("Mission visit requirement helper should indicate missing visits"),
		UPortMissionBoardWidget::BuildMissionVisitRequirementStatusText(4, 1).ToString(),
		FString(TEXT("Krever 4 havnebesøk (mangler 3).")));
	TestEqual(TEXT("Mission availability text should expose cooldown seconds"),
		UPortMissionBoardWidget::BuildMissionAvailabilityStatusText(
			EPortMissionAvailabilityReason::CooldownActive, 7.2f, false).ToString(),
		FString(TEXT("Tavlen oppdateres om 7 sekunder")));
	TestEqual(TEXT("Upgrade availability reason should mark service unavailable"),
		UPortMissionBoardWidget::DetermineUpgradeAvailabilityReason(false, 2, 2),
		EPortUpgradeAvailabilityReason::ServiceUnavailable);
	TestEqual(TEXT("Upgrade availability reason should mark no valid offers"),
		UPortMissionBoardWidget::DetermineUpgradeAvailabilityReason(true, 0, 0),
		EPortUpgradeAvailabilityReason::NoValidOffers);
	TestEqual(TEXT("Upgrade availability reason should mark no affordable offers"),
		UPortMissionBoardWidget::DetermineUpgradeAvailabilityReason(true, 2, 0),
		EPortUpgradeAvailabilityReason::NoAffordableOffers);
	TestEqual(TEXT("Upgrade availability reason should mark ready"),
		UPortMissionBoardWidget::DetermineUpgradeAvailabilityReason(true, 2, 1),
		EPortUpgradeAvailabilityReason::Ready);
	TestEqual(TEXT("Upgrade availability text should include affordable count"),
		UPortMissionBoardWidget::BuildUpgradeAvailabilityStatusText(
			EPortUpgradeAvailabilityReason::Ready, 3).ToString(),
		FString(TEXT("3 oppgradering(er) kan kjøpes nå.")));
	TestEqual(TEXT("Upgrade offer source should be none when service disabled"),
		UPortMissionBoardWidget::DetermineUpgradeOfferSource(false, true, false),
		EPortUpgradeOfferSource::None);
	TestEqual(TEXT("Upgrade offer source should prefer weighted rules"),
		UPortMissionBoardWidget::DetermineUpgradeOfferSource(true, true, true),
		EPortUpgradeOfferSource::WeightedRules);
	TestEqual(TEXT("Upgrade offer source should report fallback list when weighted unavailable"),
		UPortMissionBoardWidget::DetermineUpgradeOfferSource(true, false, true),
		EPortUpgradeOfferSource::FallbackList);
	TestEqual(TEXT("Upgrade offer source status should include weighted and gated counts"),
		UPortMissionBoardWidget::BuildUpgradeOfferSourceStatusText(
			EPortUpgradeOfferSource::WeightedRules, 2, 1, 3).ToString(),
		FString(TEXT("Vektede oppgraderingsregler aktiv (2 kvalifiserte). 1 regel(er) låst av havnebesøk. 3 opplåste skjult.")));
	TestEqual(TEXT("Upgrade visit status should indicate no requirement"),
		UPortMissionBoardWidget::BuildUpgradeVisitRequirementStatusText(0, 4).ToString(),
		FString(TEXT("Ingen havnekrav.")));
	TestEqual(TEXT("Upgrade visit status should indicate remaining visits"),
		UPortMissionBoardWidget::BuildUpgradeVisitRequirementStatusText(5, 3).ToString(),
		FString(TEXT("Krever 5 havnebesøk (mangler 2).")));
	TestEqual(TEXT("Refresh context helper should map dock-arrival text"),
		UPortMissionBoardWidget::BuildRefreshContextStatusText(EPortBoardRefreshContext::DockArrival).ToString(),
		FString(TEXT("Tavle oppdatert ved anløp.")));
	TestEqual(TEXT("Refresh context helper should map cooldown text"),
		UPortMissionBoardWidget::BuildRefreshContextStatusText(EPortBoardRefreshContext::CooldownBlocked).ToString(),
		FString(TEXT("Tavle oppdatert mens havnen er i nedkjøling.")));
	TestEqual(TEXT("Refresh context helper should map manual refresh text"),
		UPortMissionBoardWidget::BuildRefreshContextStatusText(EPortBoardRefreshContext::ManualRefresh).ToString(),
		FString(TEXT("Tavle manuelt oppdatert.")));
	TestEqual(TEXT("Manual refresh helper should report disabled state"),
		UPortMissionBoardWidget::BuildManualRefreshStatusText(false, false, 0.0f, 0, true).ToString(),
		FString(TEXT("Manuell oppfriskning er deaktivert i denne havnen.")));
	TestEqual(TEXT("Manual refresh helper should report cooldown state"),
		UPortMissionBoardWidget::BuildManualRefreshStatusText(true, true, 5.2f, 25, true).ToString(),
		FString(TEXT("Manuell oppfriskning klar om 5 sekunder.")));
	TestEqual(TEXT("Manual refresh helper should report free refresh state"),
		UPortMissionBoardWidget::BuildManualRefreshStatusText(true, false, 0.0f, 0, true).ToString(),
		FString(TEXT("Manuell oppfriskning er gratis.")));
	TestEqual(TEXT("Manual refresh helper should report affordable paid refresh"),
		UPortMissionBoardWidget::BuildManualRefreshStatusText(true, false, 0.0f, 40, true).ToString(),
		FString(TEXT("Manuell oppfriskning tilgjengelig (40 kreditter).")));
	TestEqual(TEXT("Manual refresh helper should report insufficient credits"),
		UPortMissionBoardWidget::BuildManualRefreshStatusText(true, false, 0.0f, 40, false).ToString(),
		FString(TEXT("Mangler kreditter til manuell oppfriskning (40).")));

	FPortMissionBoardData MissionBoardData;
	MissionBoardData.bSupportsMissionBoard = true;
	MissionBoardData.OfferedMissionIds = { TEXT("Mission_A"), TEXT("Mission_B") };
	MissionBoardData.CurrentMissionId = TEXT("Mission_A");
	MissionBoardData.bMissionBoardOnCooldown = false;

	FText BlockedReason;
	TestFalse(TEXT("Mission accept helper should reject already-active mission"),
		UPortMissionBoardWidget::CanRequestMissionAccept(MissionBoardData, TEXT("Mission_A"), BlockedReason));
	TestEqual(TEXT("Mission accept helper should explain already-active rejection"),
		BlockedReason.ToString(),
		FString(TEXT("Oppdraget er allerede aktivt.")));
	TestFalse(TEXT("Mission accept helper should reject non-offered mission"),
		UPortMissionBoardWidget::CanRequestMissionAccept(MissionBoardData, TEXT("Mission_C"), BlockedReason));
	TestEqual(TEXT("Mission accept helper should explain non-offered mission"),
		BlockedReason.ToString(),
		FString(TEXT("Oppdraget tilbys ikke i denne havnen.")));
	MissionBoardData.bSupportsMissionBoard = false;
	TestFalse(TEXT("Mission accept helper should reject when board service disabled"),
		UPortMissionBoardWidget::CanRequestMissionAccept(MissionBoardData, TEXT("Mission_B"), BlockedReason));
	TestEqual(TEXT("Mission accept helper should expose disabled-board reason"),
		BlockedReason.ToString(),
		FString(TEXT("Ingen tavletjenester tilgjengelig i denne havnen.")));
	MissionBoardData.bSupportsMissionBoard = true;
	MissionBoardData.bMissionBoardOnCooldown = true;
	MissionBoardData.CooldownRemainingSeconds = 9.0f;
	TestFalse(TEXT("Mission accept helper should reject during cooldown"),
		UPortMissionBoardWidget::CanRequestMissionAccept(MissionBoardData, TEXT("Mission_B"), BlockedReason));
	TestEqual(TEXT("Mission accept helper should expose cooldown reason"),
		BlockedReason.ToString(),
		FString(TEXT("Tavlen oppdateres om 9 sekunder")));
	MissionBoardData.bMissionBoardOnCooldown = false;
	TestTrue(TEXT("Mission accept helper should allow offered mission switch"),
		UPortMissionBoardWidget::CanRequestMissionAccept(MissionBoardData, TEXT("Mission_B"), BlockedReason));

	FPortUpgradeOfferEntry UpgradeOffer;
	UpgradeOffer.UpgradeId = TEXT("Upgrade_A");
	UpgradeOffer.CreditCost = 200;
	UpgradeOffer.bUnlocked = false;
	UpgradeOffer.bAffordable = true;
	UpgradeOffer.bVisitGateSatisfied = true;

	FPortMissionBoardData UpgradeBoardData;
	UpgradeBoardData.bSupportsUpgradeService = true;
	UpgradeBoardData.OfferedUpgradeIds = { UpgradeOffer.UpgradeId };
	UpgradeBoardData.OfferedUpgrades = { UpgradeOffer };
	UpgradeBoardData.UpgradeAvailabilityReason = EPortUpgradeAvailabilityReason::Ready;
	TestTrue(TEXT("Upgrade helper should allow affordable offered upgrade"),
		UPortMissionBoardWidget::CanRequestUpgradePurchase(UpgradeBoardData, UpgradeOffer.UpgradeId, BlockedReason));

	UpgradeBoardData.OfferedUpgrades[0].bAffordable = false;
	TestFalse(TEXT("Upgrade helper should reject unaffordable upgrade"),
		UPortMissionBoardWidget::CanRequestUpgradePurchase(UpgradeBoardData, UpgradeOffer.UpgradeId, BlockedReason));
	TestEqual(TEXT("Upgrade helper should explain unaffordable rejection"),
		BlockedReason.ToString(),
		FString(TEXT("Ikke nok kreditter (200 kreves).")));

	UpgradeBoardData.OfferedUpgrades[0].bAffordable = true;
	UpgradeBoardData.OfferedUpgrades[0].bUnlocked = true;
	TestFalse(TEXT("Upgrade helper should reject already unlocked upgrade"),
		UPortMissionBoardWidget::CanRequestUpgradePurchase(UpgradeBoardData, UpgradeOffer.UpgradeId, BlockedReason));
	TestEqual(TEXT("Upgrade helper should explain unlocked rejection"),
		BlockedReason.ToString(),
		FString(TEXT("Oppgraderingen er allerede opplåst.")));
	UpgradeBoardData.OfferedUpgrades[0].bUnlocked = false;
	UpgradeBoardData.OfferedUpgrades[0].bVisitGateSatisfied = false;
	UpgradeBoardData.OfferedUpgrades[0].VisitRequirementStatus = FText::FromString(TEXT("Krever 3 havnebesøk (mangler 2)."));
	TestFalse(TEXT("Upgrade helper should reject visit-gated upgrades"),
		UPortMissionBoardWidget::CanRequestUpgradePurchase(UpgradeBoardData, UpgradeOffer.UpgradeId, BlockedReason));
	TestEqual(TEXT("Upgrade helper should expose visit-gate reason"),
		BlockedReason.ToString(),
		FString(TEXT("Krever 3 havnebesøk (mangler 2).")));

	FPortMissionBoardData RepairBoardData;
	RepairBoardData.bSupportsRepairService = true;
	RepairBoardData.CurrentBoatConditionPercent = 75;
	RepairBoardData.bCanAffordRepair = true;
	TestTrue(TEXT("Repair helper should allow when service available and affordable"),
		UPortMissionBoardWidget::CanRequestRepairService(RepairBoardData, BlockedReason));
	RepairBoardData.bCanAffordRepair = false;
	RepairBoardData.RepairStatus = FText::FromString(TEXT("Mangler kreditter til reparasjon (30)."));
	TestFalse(TEXT("Repair helper should reject unaffordable repair"),
		UPortMissionBoardWidget::CanRequestRepairService(RepairBoardData, BlockedReason));
	TestEqual(TEXT("Repair helper should explain unaffordable repair"),
		BlockedReason.ToString(),
		FString(TEXT("Mangler kreditter til reparasjon (30).")));

	FPortMissionBoardData RefreshBoardData;
	RefreshBoardData.bSupportsManualRefresh = true;
	RefreshBoardData.bManualRefreshOnCooldown = false;
	RefreshBoardData.ManualRefreshCreditCost = 25;
	RefreshBoardData.bCanAffordManualRefresh = true;
	TestTrue(TEXT("Manual refresh helper should allow enabled affordable refresh"),
		UPortMissionBoardWidget::CanRequestManualRefresh(RefreshBoardData, BlockedReason));
	RefreshBoardData.bSupportsManualRefresh = false;
	TestFalse(TEXT("Manual refresh helper should reject when refresh disabled"),
		UPortMissionBoardWidget::CanRequestManualRefresh(RefreshBoardData, BlockedReason));
	TestEqual(TEXT("Manual refresh helper should expose disabled reason"),
		BlockedReason.ToString(),
		FString(TEXT("Manuell oppfriskning er deaktivert i denne havnen.")));
	RefreshBoardData.bSupportsManualRefresh = true;
	RefreshBoardData.bManualRefreshOnCooldown = true;
	RefreshBoardData.ManualRefreshCooldownRemainingSeconds = 6.0f;
	TestFalse(TEXT("Manual refresh helper should reject while cooldown active"),
		UPortMissionBoardWidget::CanRequestManualRefresh(RefreshBoardData, BlockedReason));
	TestEqual(TEXT("Manual refresh helper should expose cooldown reason"),
		BlockedReason.ToString(),
		FString(TEXT("Manuell oppfriskning klar om 6 sekunder.")));
	RefreshBoardData.bManualRefreshOnCooldown = false;
	RefreshBoardData.bCanAffordManualRefresh = false;
	TestFalse(TEXT("Manual refresh helper should reject unaffordable refresh"),
		UPortMissionBoardWidget::CanRequestManualRefresh(RefreshBoardData, BlockedReason));
	TestEqual(TEXT("Manual refresh helper should explain unaffordable refresh"),
		BlockedReason.ToString(),
		FString(TEXT("Mangler kreditter til manuell oppfriskning (25).")));

	FPortMissionBoardData AnnotatedBoardData;
	AnnotatedBoardData.bSupportsMissionBoard = true;
	AnnotatedBoardData.OfferedMissionIds = { TEXT("Mission_A") };
	AnnotatedBoardData.CurrentMissionId = TEXT("Mission_A");
	FPortMissionOfferEntry AnnotatedMissionEntry;
	AnnotatedMissionEntry.MissionId = TEXT("Mission_A");
	AnnotatedBoardData.OfferedMissions.Add(AnnotatedMissionEntry);

	AnnotatedBoardData.bSupportsUpgradeService = true;
	AnnotatedBoardData.OfferedUpgradeIds = { TEXT("Upgrade_A") };
	AnnotatedBoardData.UpgradeAvailabilityReason = EPortUpgradeAvailabilityReason::Ready;
	FPortUpgradeOfferEntry AnnotatedUpgradeEntry;
	AnnotatedUpgradeEntry.UpgradeId = TEXT("Upgrade_A");
	AnnotatedUpgradeEntry.CreditCost = 300;
	AnnotatedUpgradeEntry.bAffordable = false;
	AnnotatedUpgradeEntry.bUnlocked = false;
	AnnotatedUpgradeEntry.bVisitGateSatisfied = true;
	AnnotatedBoardData.OfferedUpgrades.Add(AnnotatedUpgradeEntry);

	const FPortMissionBoardData AnnotatedResult = UPortMissionBoardWidget::BuildActionStateAnnotatedBoardData(AnnotatedBoardData);
	TestEqual(TEXT("Annotated board should preserve mission offer count"), AnnotatedResult.OfferedMissions.Num(), 1);
	TestFalse(TEXT("Annotated board should mark active mission as non-selectable"), AnnotatedResult.OfferedMissions[0].bSelectable);
	TestEqual(TEXT("Annotated board should include mission blocked reason"), AnnotatedResult.OfferedMissions[0].SelectionBlockedReason.ToString(), FString(TEXT("Oppdraget er allerede aktivt.")));
	TestEqual(TEXT("Annotated board should track selectable mission count"), AnnotatedResult.SelectableMissionOfferCount, 0);
	TestEqual(TEXT("Annotated board should track blocked mission count"), AnnotatedResult.BlockedMissionOfferCount, 1);
	TestFalse(TEXT("Annotated board should mark no selectable missions"), AnnotatedResult.bHasSelectableMissionOffers);
	TestEqual(TEXT("Annotated board should preserve upgrade offer count"), AnnotatedResult.OfferedUpgrades.Num(), 1);
	TestFalse(TEXT("Annotated board should mark unaffordable upgrade as non-purchasable"), AnnotatedResult.OfferedUpgrades[0].bPurchasable);
	TestEqual(TEXT("Annotated board should include upgrade blocked reason"), AnnotatedResult.OfferedUpgrades[0].PurchaseBlockedReason.ToString(), FString(TEXT("Ikke nok kreditter (300 kreves).")));
	TestEqual(TEXT("Annotated board should track purchasable upgrade count"), AnnotatedResult.PurchasableUpgradeOfferCount, 0);
	TestEqual(TEXT("Annotated board should track blocked upgrade count"), AnnotatedResult.BlockedUpgradeOfferCount, 1);
	TestFalse(TEXT("Annotated board should mark no purchasable upgrades"), AnnotatedResult.bHasPurchasableUpgradeOffers);
	TestFalse(TEXT("Annotated board should mark no immediate actions"), AnnotatedResult.bHasAnyImmediateActions);
	TestEqual(TEXT("Annotated board should expose no primary action hint"),
		AnnotatedResult.PrimaryActionHint,
		EPortBoardPrimaryActionHint::None);
	TestEqual(TEXT("Annotated board should expose no-action hint text"),
		AnnotatedResult.PrimaryActionHintStatus.ToString(),
		FString(TEXT("Ingen umiddelbare handlinger tilgjengelig.")));
	TestEqual(TEXT("Annotated board should include action summary text"),
		AnnotatedResult.OfferActionSummaryStatus.ToString(),
		FString(TEXT("Valgbare oppdrag: 0 (1 blokkert) | Kjøpbare oppgraderinger: 0 (1 blokkert)")));

	UPortMissionBoardWidget* Widget = NewObject<UPortMissionBoardWidget>();
	TestNotNull(TEXT("Widget object should be created for annotation push test"), Widget);
	if (Widget)
	{
		Widget->PushMissionBoardData(AnnotatedBoardData);
		const FPortMissionBoardData& LastData = Widget->GetLastData();
		TestEqual(TEXT("PushMissionBoardData should preserve mission offer count"), LastData.OfferedMissions.Num(), 1);
		TestFalse(TEXT("PushMissionBoardData should auto-annotate mission selection state"), LastData.OfferedMissions[0].bSelectable);
		TestEqual(TEXT("PushMissionBoardData should include mission blocked reason"), LastData.OfferedMissions[0].SelectionBlockedReason.ToString(), FString(TEXT("Oppdraget er allerede aktivt.")));
		TestEqual(TEXT("PushMissionBoardData should track blocked mission count"), LastData.BlockedMissionOfferCount, 1);
		TestEqual(TEXT("PushMissionBoardData should preserve upgrade offer count"), LastData.OfferedUpgrades.Num(), 1);
		TestFalse(TEXT("PushMissionBoardData should auto-annotate upgrade purchase state"), LastData.OfferedUpgrades[0].bPurchasable);
		TestEqual(TEXT("PushMissionBoardData should include upgrade blocked reason"), LastData.OfferedUpgrades[0].PurchaseBlockedReason.ToString(), FString(TEXT("Ikke nok kreditter (300 kreves).")));
		TestEqual(TEXT("PushMissionBoardData should track blocked upgrade count"), LastData.BlockedUpgradeOfferCount, 1);
	}

	FPortMissionBoardData SummaryBoardData;
	SummaryBoardData.SelectableMissionOfferCount = 2;
	SummaryBoardData.BlockedMissionOfferCount = 1;
	SummaryBoardData.PurchasableUpgradeOfferCount = 3;
	SummaryBoardData.BlockedUpgradeOfferCount = 2;
	SummaryBoardData.bSupportsMissionBoard = true;
	SummaryBoardData.bSupportsUpgradeService = true;
	TestEqual(TEXT("Action summary helper should render compact counts"),
		UPortMissionBoardWidget::BuildOfferActionSummaryStatusText(SummaryBoardData).ToString(),
		FString(TEXT("Valgbare oppdrag: 2 (1 blokkert) | Kjøpbare oppgraderinger: 3 (2 blokkert)")));

	FPortMissionBoardData MissionOnlySummaryData;
	MissionOnlySummaryData.bSupportsMissionBoard = true;
	MissionOnlySummaryData.bSupportsUpgradeService = false;
	MissionOnlySummaryData.SelectableMissionOfferCount = 1;
	MissionOnlySummaryData.BlockedMissionOfferCount = 2;
	TestEqual(TEXT("Action summary helper should render mission-only status"),
		UPortMissionBoardWidget::BuildOfferActionSummaryStatusText(MissionOnlySummaryData).ToString(),
		FString(TEXT("Valgbare oppdrag: 1 (2 blokkert)")));

	FPortMissionBoardData UpgradeOnlySummaryData;
	UpgradeOnlySummaryData.bSupportsMissionBoard = false;
	UpgradeOnlySummaryData.bSupportsUpgradeService = true;
	UpgradeOnlySummaryData.PurchasableUpgradeOfferCount = 4;
	UpgradeOnlySummaryData.BlockedUpgradeOfferCount = 1;
	TestEqual(TEXT("Action summary helper should render upgrade-only status"),
		UPortMissionBoardWidget::BuildOfferActionSummaryStatusText(UpgradeOnlySummaryData).ToString(),
		FString(TEXT("Kjøpbare oppgraderinger: 4 (1 blokkert)")));

	FPortMissionBoardData NoActionSummaryData;
	NoActionSummaryData.bSupportsMissionBoard = false;
	NoActionSummaryData.bSupportsUpgradeService = false;
	TestEqual(TEXT("Action summary helper should render no-action status"),
		UPortMissionBoardWidget::BuildOfferActionSummaryStatusText(NoActionSummaryData).ToString(),
		FString(TEXT("Ingen handlinger tilgjengelig i denne havnen.")));

	FPortMissionBoardData MixedBoardData;
	MixedBoardData.bSupportsMissionBoard = true;
	MixedBoardData.CurrentMissionId = TEXT("Mission_A");
	MixedBoardData.OfferedMissionIds = { TEXT("Mission_A"), TEXT("Mission_B") };
	FPortMissionOfferEntry MixedMissionA;
	MixedMissionA.MissionId = TEXT("Mission_A");
	MixedBoardData.OfferedMissions.Add(MixedMissionA);
	FPortMissionOfferEntry MixedMissionB;
	MixedMissionB.MissionId = TEXT("Mission_B");
	MixedBoardData.OfferedMissions.Add(MixedMissionB);

	MixedBoardData.bSupportsUpgradeService = true;
	MixedBoardData.UpgradeAvailabilityReason = EPortUpgradeAvailabilityReason::Ready;
	MixedBoardData.OfferedUpgradeIds = { TEXT("Upgrade_A"), TEXT("Upgrade_B") };
	FPortUpgradeOfferEntry MixedUpgradeA;
	MixedUpgradeA.UpgradeId = TEXT("Upgrade_A");
	MixedUpgradeA.CreditCost = 100;
	MixedUpgradeA.bAffordable = true;
	MixedUpgradeA.bUnlocked = false;
	MixedUpgradeA.bVisitGateSatisfied = true;
	MixedBoardData.OfferedUpgrades.Add(MixedUpgradeA);
	FPortUpgradeOfferEntry MixedUpgradeB;
	MixedUpgradeB.UpgradeId = TEXT("Upgrade_B");
	MixedUpgradeB.CreditCost = 400;
	MixedUpgradeB.bAffordable = false;
	MixedUpgradeB.bUnlocked = false;
	MixedUpgradeB.bVisitGateSatisfied = true;
	MixedBoardData.OfferedUpgrades.Add(MixedUpgradeB);

	const FPortMissionBoardData MixedAnnotatedResult = UPortMissionBoardWidget::BuildActionStateAnnotatedBoardData(MixedBoardData);
	TestEqual(TEXT("Mixed annotated board should count selectable missions"), MixedAnnotatedResult.SelectableMissionOfferCount, 1);
	TestEqual(TEXT("Mixed annotated board should count blocked missions"), MixedAnnotatedResult.BlockedMissionOfferCount, 1);
	TestTrue(TEXT("Mixed annotated board should mark selectable missions"), MixedAnnotatedResult.bHasSelectableMissionOffers);
	TestEqual(TEXT("Mixed annotated board should count purchasable upgrades"), MixedAnnotatedResult.PurchasableUpgradeOfferCount, 1);
	TestEqual(TEXT("Mixed annotated board should count blocked upgrades"), MixedAnnotatedResult.BlockedUpgradeOfferCount, 1);
	TestTrue(TEXT("Mixed annotated board should mark purchasable upgrades"), MixedAnnotatedResult.bHasPurchasableUpgradeOffers);
	TestTrue(TEXT("Mixed annotated board should mark immediate actions"), MixedAnnotatedResult.bHasAnyImmediateActions);
	TestEqual(TEXT("Mixed annotated board should expose combined primary action hint"),
		MixedAnnotatedResult.PrimaryActionHint,
		EPortBoardPrimaryActionHint::SelectMissionOrBuyUpgrade);
	TestEqual(TEXT("Mixed annotated board should expose combined hint text"),
		MixedAnnotatedResult.PrimaryActionHintStatus.ToString(),
		FString(TEXT("Anbefalt neste steg: Velg oppdrag eller kjøp oppgradering.")));
	TestEqual(TEXT("Mixed annotated board should summarize mixed counts"),
		MixedAnnotatedResult.OfferActionSummaryStatus.ToString(),
		FString(TEXT("Valgbare oppdrag: 1 (1 blokkert) | Kjøpbare oppgraderinger: 1 (1 blokkert)")));

	TestEqual(TEXT("Primary action helper should return mission-only hint"),
		UPortMissionBoardWidget::DeterminePrimaryActionHint(MissionOnlySummaryData),
		EPortBoardPrimaryActionHint::SelectMission);
	TestEqual(TEXT("Primary action helper should return upgrade-only hint"),
		UPortMissionBoardWidget::DeterminePrimaryActionHint(UpgradeOnlySummaryData),
		EPortBoardPrimaryActionHint::BuyUpgrade);
	TestEqual(TEXT("Primary action helper should return no-action hint"),
		UPortMissionBoardWidget::DeterminePrimaryActionHint(NoActionSummaryData),
		EPortBoardPrimaryActionHint::None);
	TestEqual(TEXT("Primary action status helper should render mission-only text"),
		UPortMissionBoardWidget::BuildPrimaryActionHintStatusText(EPortBoardPrimaryActionHint::SelectMission).ToString(),
		FString(TEXT("Anbefalt neste steg: Velg et oppdrag.")));
	TestEqual(TEXT("Primary action status helper should render upgrade-only text"),
		UPortMissionBoardWidget::BuildPrimaryActionHintStatusText(EPortBoardPrimaryActionHint::BuyUpgrade).ToString(),
		FString(TEXT("Anbefalt neste steg: Kjøp en oppgradering.")));
	TestEqual(TEXT("Primary action status helper should render no-action text"),
		UPortMissionBoardWidget::BuildPrimaryActionHintStatusText(EPortBoardPrimaryActionHint::None).ToString(),
		FString(TEXT("Ingen umiddelbare handlinger tilgjengelig.")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FSailingMissionDisplayNameLookupTest,
	"Sailing.Progression.Mission.DisplayNameLookup",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FSailingMissionDisplayNameLookupTest::RunTest(const FString& Parameters)
{
	UMissionSubsystem* MissionSubsystem = NewObject<UMissionSubsystem>();
	MissionSubsystem->SetMissionAssetPath(NAME_None);
	TestEqual(TEXT("Mission asset scan path should normalize to /Game"), MissionSubsystem->GetMissionAssetPath(), FName(TEXT("/Game")));

	USailingMissionDataAsset* Mission = NewObject<USailingMissionDataAsset>();
	Mission->MissionId = TEXT("Mission_Display");
	Mission->DisplayName = FText::FromString(TEXT("Visningsnavn"));
	Mission->MissionType = ESailingMissionType::Delivery;

	MissionSubsystem->RegisterMissionAsset(Mission);
	const USailingMissionDataAsset* FoundMission = MissionSubsystem->GetMissionAssetById(Mission->MissionId);
	TestNotNull(TEXT("Mission lookup by id should return registered mission"), FoundMission);
	TestNull(TEXT("Mission lookup by id should return null for unknown mission"),
		MissionSubsystem->GetMissionAssetById(TEXT("UnknownMission")));

	const FText FoundTitle = MissionSubsystem->GetMissionDisplayNameById(Mission->MissionId);
	TestEqual(TEXT("Display name lookup should return registered title"), FoundTitle.ToString(), FString(TEXT("Visningsnavn")));

	const FText FallbackTitle = MissionSubsystem->GetMissionDisplayNameById(TEXT("UnknownMission"));
	TestEqual(TEXT("Unknown mission should fallback to mission id text"), FallbackTitle.ToString(), FString(TEXT("UnknownMission")));

	MissionSubsystem->RecordMissionBoardSelection(TEXT("HavnNord"), Mission->MissionId);
	MissionSubsystem->RecordMissionBoardSelection(TEXT("HavnNord"), TEXT("Mission_B"));
	MissionSubsystem->RecordMissionBoardSelection(TEXT("HavnVest"), TEXT("Mission_C"));
	const TArray<FMissionBoardSelectionEntry> History = MissionSubsystem->GetMissionBoardSelectionHistory();
	TestEqual(TEXT("Mission board history should contain selections"), History.Num(), 3);
	TestEqual(TEXT("History entry should include port id"), History[0].PortId, FName(TEXT("HavnNord")));

	const TArray<FMissionBoardSelectionEntry> RecentNord = MissionSubsystem->GetRecentMissionBoardSelectionsForPort(TEXT("HavnNord"), 2);
	TestEqual(TEXT("Recent per-port selections should return requested count"), RecentNord.Num(), 2);
	TestEqual(TEXT("Most recent entry should be first in result"), RecentNord[0].MissionId, FName(TEXT("Mission_B")));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FSailingEconomyRepairFlowTest,
	"Sailing.Progression.Economy.RepairFlow",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FSailingEconomyRepairFlowTest::RunTest(const FString& Parameters)
{
	UEconomySubsystem* Economy = NewObject<UEconomySubsystem>();
	Economy->SetUpgradeAssetPath(NAME_None);
	TestEqual(TEXT("Upgrade asset scan path should normalize to /Game"), Economy->GetUpgradeAssetPath(), FName(TEXT("/Game")));
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
