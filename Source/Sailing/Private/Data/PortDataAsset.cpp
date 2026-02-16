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
