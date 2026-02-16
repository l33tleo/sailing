#include "SaveGameSailing.h"

const FString USaveGameSailing::SaveSlotName = TEXT("SailingSaveSlot");

USaveGameSailing::USaveGameSailing()
{
	SaveSchemaVersion = CurrentSaveSchemaVersion;
	TotalIslandsDiscovered = 0;
	LastPlayerLocation = FVector::ZeroVector;
	PlayerCredits = 0;
	UnlockedUpgradeIds.Empty();
	ActiveMissionId = NAME_None;
	CompletedMissionIds.Empty();
	TelemetryCounters.Empty();
}

bool USaveGameSailing::IsIslandDiscovered(FIntPoint ChunkCoord, int32 IslandIndex) const
{
	FString Key = FString::Printf(TEXT("%d_%d_%d"), ChunkCoord.X, ChunkCoord.Y, IslandIndex);
	const FIslandData* Data = DiscoveredIslands.Find(Key);
	return Data != nullptr && Data->bDiscovered;
}

void USaveGameSailing::MarkIslandDiscovered(const FIslandData& IslandData)
{
	FString Key = IslandData.GetUniqueKey();
	if (!DiscoveredIslands.Contains(Key))
	{
		DiscoveredIslands.Add(Key, IslandData);
		TotalIslandsDiscovered++;
	}
	else
	{
		FIslandData& ExistingData = DiscoveredIslands[Key];
		if (!ExistingData.bDiscovered)
		{
			ExistingData = IslandData;
			TotalIslandsDiscovered++;
		}
	}
}

FIslandData* USaveGameSailing::GetIslandData(FIntPoint ChunkCoord, int32 IslandIndex)
{
	FString Key = FString::Printf(TEXT("%d_%d_%d"), ChunkCoord.X, ChunkCoord.Y, IslandIndex);
	return DiscoveredIslands.Find(Key);
}

void USaveGameSailing::EnsureCompatibility()
{
	// Schema <= 0 means legacy save before versioning was introduced.
	if (SaveSchemaVersion <= 0)
	{
		SaveSchemaVersion = 1;
	}

	// Migration from legacy schema where total count could diverge from map size.
	if (SaveSchemaVersion <= 1)
	{
		int32 ComputedDiscovered = 0;
		for (const TPair<FString, FIslandData>& Pair : DiscoveredIslands)
		{
			if (Pair.Value.bDiscovered)
			{
				ComputedDiscovered++;
			}
		}
		TotalIslandsDiscovered = ComputedDiscovered;
	}
	else
	{
		// Defensive consistency clamp for newer schema too.
		TotalIslandsDiscovered = FMath::Max(0, TotalIslandsDiscovered);
	}

	PlayerCredits = FMath::Max(0, PlayerCredits);
	CompletedMissionIds.RemoveAll([](const FName& MissionId) { return MissionId.IsNone(); });
	for (TPair<FName, int32>& Pair : TelemetryCounters)
	{
		Pair.Value = FMath::Max(0, Pair.Value);
	}
	SaveSchemaVersion = CurrentSaveSchemaVersion;
}
