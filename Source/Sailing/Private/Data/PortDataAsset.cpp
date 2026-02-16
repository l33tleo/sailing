#include "Data/PortDataAsset.h"

TArray<FName> UPortDataAsset::BuildPrioritizedMissionIds(
	const TArray<FPortMissionWeightedOffer>& InWeightedOffers,
	const TArray<FName>& InFallbackOffers,
	int32 PortVisitCount,
	int32 MaxOffers)
{
	TArray<FName> Result;

	TArray<FPortMissionWeightedOffer> EligibleWeightedOffers;
	for (const FPortMissionWeightedOffer& Offer : InWeightedOffers)
	{
		if (Offer.MissionId.IsNone())
		{
			continue;
		}

		if (PortVisitCount < FMath::Max(0, Offer.MinPortVisits))
		{
			continue;
		}

		EligibleWeightedOffers.Add(Offer);
	}

	EligibleWeightedOffers.Sort([](const FPortMissionWeightedOffer& A, const FPortMissionWeightedOffer& B)
	{
		if (!FMath::IsNearlyEqual(A.PriorityWeight, B.PriorityWeight))
		{
			return A.PriorityWeight > B.PriorityWeight;
		}
		return A.MissionId.LexicalLess(B.MissionId);
	});

	for (const FPortMissionWeightedOffer& Offer : EligibleWeightedOffers)
	{
		Result.AddUnique(Offer.MissionId);
	}

	if (Result.Num() == 0)
	{
		for (const FName& OfferId : InFallbackOffers)
		{
			if (!OfferId.IsNone())
			{
				Result.AddUnique(OfferId);
			}
		}
	}

	if (MaxOffers > 0 && Result.Num() > MaxOffers)
	{
		Result.SetNum(MaxOffers);
	}

	return Result;
}

TArray<FName> UPortDataAsset::BuildRotatedUpgradeIds(
	const TArray<FName>& InOfferedUpgradeIds,
	int32 PortVisitCount,
	int32 MaxOffers,
	bool bRotateByVisits)
{
	TArray<FName> UniqueUpgradeIds;
	for (const FName& UpgradeId : InOfferedUpgradeIds)
	{
		if (!UpgradeId.IsNone())
		{
			UniqueUpgradeIds.AddUnique(UpgradeId);
		}
	}

	TArray<FName> Result;
	if (UniqueUpgradeIds.Num() == 0)
	{
		return Result;
	}

	int32 StartIndex = 0;
	if (bRotateByVisits && UniqueUpgradeIds.Num() > 1)
	{
		const int32 SafeVisits = FMath::Max(0, PortVisitCount);
		StartIndex = SafeVisits % UniqueUpgradeIds.Num();
	}

	for (int32 Offset = 0; Offset < UniqueUpgradeIds.Num(); ++Offset)
	{
		const int32 Index = (StartIndex + Offset) % UniqueUpgradeIds.Num();
		Result.Add(UniqueUpgradeIds[Index]);

		if (MaxOffers > 0 && Result.Num() >= MaxOffers)
		{
			break;
		}
	}

	return Result;
}

TArray<FName> UPortDataAsset::BuildPrioritizedUpgradeIds(
	const TArray<FPortUpgradeWeightedOffer>& InWeightedOffers,
	const TArray<FName>& InFallbackOffers,
	int32 PortVisitCount,
	int32 MaxOffers,
	bool bRotateByVisits)
{
	TArray<FName> WeightedIds;
	TArray<FPortUpgradeWeightedOffer> EligibleWeightedOffers;

	for (const FPortUpgradeWeightedOffer& Offer : InWeightedOffers)
	{
		if (Offer.UpgradeId.IsNone())
		{
			continue;
		}

		if (PortVisitCount < FMath::Max(0, Offer.MinPortVisits))
		{
			continue;
		}

		EligibleWeightedOffers.Add(Offer);
	}

	EligibleWeightedOffers.Sort([](const FPortUpgradeWeightedOffer& A, const FPortUpgradeWeightedOffer& B)
	{
		if (!FMath::IsNearlyEqual(A.PriorityWeight, B.PriorityWeight))
		{
			return A.PriorityWeight > B.PriorityWeight;
		}
		return A.UpgradeId.LexicalLess(B.UpgradeId);
	});

	for (const FPortUpgradeWeightedOffer& Offer : EligibleWeightedOffers)
	{
		WeightedIds.AddUnique(Offer.UpgradeId);
	}

	if (WeightedIds.Num() > 0)
	{
		return BuildRotatedUpgradeIds(WeightedIds, PortVisitCount, MaxOffers, bRotateByVisits);
	}

	return BuildRotatedUpgradeIds(InFallbackOffers, PortVisitCount, MaxOffers, bRotateByVisits);
}

int32 UPortDataAsset::CalculateAdjustedUpgradeCost(int32 BaseCost, float CostMultiplier)
{
	const int32 SafeBaseCost = FMath::Max(0, BaseCost);
	const float SafeMultiplier = FMath::Max(0.1f, CostMultiplier);
	return FMath::Max(0, FMath::RoundToInt(static_cast<float>(SafeBaseCost) * SafeMultiplier));
}
