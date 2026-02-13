#include "SaveGameSailing.h"

const FString USaveGameSailing::SaveSlotName = TEXT("SailingSaveSlot");

USaveGameSailing::USaveGameSailing()
{
	TotalIslandsDiscovered = 0;
	LastPlayerLocation = FVector::ZeroVector;
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
