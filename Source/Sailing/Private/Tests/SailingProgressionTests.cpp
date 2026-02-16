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

	const TArray<FName> RotatedUpgrades = UPortDataAsset::BuildRotatedUpgradeIds(
		{ TEXT("Upgrade_A"), TEXT("Upgrade_B"), TEXT("Upgrade_C") }, 2, 2, true);
	TestEqual(TEXT("Upgrade rotation should respect max-offer cap"), RotatedUpgrades.Num(), 2);
	TestEqual(TEXT("Upgrade rotation should offset by visit count"), RotatedUpgrades[0], FName(TEXT("Upgrade_C")));
	TestEqual(TEXT("Upgrade rotation should continue circular order"), RotatedUpgrades[1], FName(TEXT("Upgrade_A")));

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
