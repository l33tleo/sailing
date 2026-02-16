#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "SaveGameSailing.h"
#include "Systems/SailingCoreSubsystems.h"
#include "Data/BoatUpgradeDataAsset.h"

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

#endif
