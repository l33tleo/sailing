#include "IslandNameGenerator.h"

const TArray<FString>& UIslandNameGenerator::GetPrefixes()
{
	static TArray<FString> Prefixes = {
		TEXT("Nord"),
		TEXT("Syd"),
		TEXT("Vest"),
		TEXT("Aust"),
		TEXT("Stor"),
		TEXT("Lille"),
		TEXT("Hvit"),
		TEXT("Svart"),
		TEXT("Grønn"),
		TEXT("Blå"),
		TEXT("Gull"),
		TEXT("Sølv"),
		TEXT("Lang"),
		TEXT("Rund"),
		TEXT("Høy"),
		TEXT("Flat"),
		TEXT("Gammel"),
		TEXT("Ny"),
		TEXT("Fjern"),
		TEXT("Skjult")
	};
	return Prefixes;
}

const TArray<FString>& UIslandNameGenerator::GetMiddles()
{
	static TArray<FString> Middles = {
		TEXT("fjord"),
		TEXT("vik"),
		TEXT("dal"),
		TEXT("berg"),
		TEXT("stein"),
		TEXT("nes"),
		TEXT("strand"),
		TEXT("sand"),
		TEXT("havn"),
		TEXT("bukt"),
		TEXT("sund"),
		TEXT("skip"),
		TEXT("fisk"),
		TEXT("sel"),
		TEXT("hval"),
		TEXT("ørn"),
		TEXT("ulv"),
		TEXT("bjørn"),
		TEXT("rev"),
		TEXT("elg")
	};
	return Middles;
}

const TArray<FString>& UIslandNameGenerator::GetSuffixes()
{
	static TArray<FString> Suffixes = {
		TEXT("øy"),
		TEXT("øya"),
		TEXT("holmen"),
		TEXT("holm"),
		TEXT("skjær"),
		TEXT("land"),
		TEXT("rike"),
		TEXT("borg"),
		TEXT("stad"),
		TEXT("by")
	};
	return Suffixes;
}

FString UIslandNameGenerator::GenerateIslandName(FIntPoint ChunkCoord, int32 IslandIndex)
{
	// Create deterministic seed from coordinates
	int32 Seed = HashCombine(
		HashCombine(GetTypeHash(ChunkCoord.X), GetTypeHash(ChunkCoord.Y)),
		GetTypeHash(IslandIndex));
	FRandomStream RNG(Seed);

	const TArray<FString>& Prefixes = GetPrefixes();
	const TArray<FString>& Middles = GetMiddles();
	const TArray<FString>& Suffixes = GetSuffixes();

	// 70% chance to have a prefix
	bool bHasPrefix = RNG.FRand() < 0.7f;
	// 50% chance to have a middle part
	bool bHasMiddle = RNG.FRand() < 0.5f;

	FString Name;

	if (bHasPrefix)
	{
		int32 PrefixIdx = RNG.RandRange(0, Prefixes.Num() - 1);
		Name = Prefixes[PrefixIdx];
	}

	if (bHasMiddle)
	{
		int32 MiddleIdx = RNG.RandRange(0, Middles.Num() - 1);
		Name += Middles[MiddleIdx];
	}
	else if (!bHasPrefix)
	{
		// Must have at least prefix or middle
		int32 MiddleIdx = RNG.RandRange(0, Middles.Num() - 1);
		// Capitalize first letter
		FString Middle = Middles[MiddleIdx];
		Middle[0] = FChar::ToUpper(Middle[0]);
		Name = Middle;
	}

	int32 SuffixIdx = RNG.RandRange(0, Suffixes.Num() - 1);
	Name += Suffixes[SuffixIdx];

	return Name;
}
