#include "Data/PortDataAsset.h"

TArray<FName> UPortDataAsset::FilterIdsByAllowedSet(
	const TArray<FName>& InIds,
	const TSet<FName>& InAllowedIds,
	int32& OutRejectedCount)
{
	OutRejectedCount = 0;

	TArray<FName> Result;
	for (const FName& Id : InIds)
	{
		if (Id.IsNone())
		{
			OutRejectedCount++;
			continue;
		}

		if (InAllowedIds.Num() > 0 && !InAllowedIds.Contains(Id))
		{
			OutRejectedCount++;
			continue;
		}

		Result.AddUnique(Id);
	}

	return Result;
}

TArray<FName> UPortDataAsset::FindMissingRequiredIds(
	const TArray<FName>& InRequiredIds,
	const TSet<FName>& InAvailableIds)
{
	TArray<FName> MissingRequiredIds;
	for (const FName& RequiredId : InRequiredIds)
	{
		if (RequiredId.IsNone() || InAvailableIds.Contains(RequiredId))
		{
			continue;
		}

		MissingRequiredIds.AddUnique(RequiredId);
	}

	MissingRequiredIds.Sort(FNameLexicalLess());
	return MissingRequiredIds;
}

TArray<FPortMissionWeightedOffer> UPortDataAsset::FilterMissionWeightedOffersByAllowedSet(
	const TArray<FPortMissionWeightedOffer>& InOffers,
	const TSet<FName>& InAllowedMissionIds,
	int32& OutRejectedCount)
{
	OutRejectedCount = 0;

	TArray<FPortMissionWeightedOffer> Result;
	TSet<FName> AddedMissionIds;
	for (const FPortMissionWeightedOffer& Offer : InOffers)
	{
		if (Offer.MissionId.IsNone())
		{
			OutRejectedCount++;
			continue;
		}

		if (InAllowedMissionIds.Num() > 0 && !InAllowedMissionIds.Contains(Offer.MissionId))
		{
			OutRejectedCount++;
			continue;
		}

		if (AddedMissionIds.Contains(Offer.MissionId))
		{
			continue;
		}

		Result.Add(Offer);
		AddedMissionIds.Add(Offer.MissionId);
	}

	return Result;
}

TArray<FPortUpgradeWeightedOffer> UPortDataAsset::FilterUpgradeWeightedOffersByAllowedSet(
	const TArray<FPortUpgradeWeightedOffer>& InOffers,
	const TSet<FName>& InAllowedUpgradeIds,
	int32& OutRejectedCount)
{
	OutRejectedCount = 0;

	TArray<FPortUpgradeWeightedOffer> Result;
	TSet<FName> AddedUpgradeIds;
	for (const FPortUpgradeWeightedOffer& Offer : InOffers)
	{
		if (Offer.UpgradeId.IsNone())
		{
			OutRejectedCount++;
			continue;
		}

		if (InAllowedUpgradeIds.Num() > 0 && !InAllowedUpgradeIds.Contains(Offer.UpgradeId))
		{
			OutRejectedCount++;
			continue;
		}

		if (AddedUpgradeIds.Contains(Offer.UpgradeId))
		{
			continue;
		}

		Result.Add(Offer);
		AddedUpgradeIds.Add(Offer.UpgradeId);
	}

	return Result;
}

TArray<FName> UPortDataAsset::BuildPrioritizedMissionIds(
	const TArray<FPortMissionWeightedOffer>& InWeightedOffers,
	const TArray<FName>& InFallbackOffers,
	int32 PortVisitCount,
	int32 MaxOffers)
{
	return BuildPrioritizedMissionSelection(
		InWeightedOffers,
		InFallbackOffers,
		PortVisitCount,
		MaxOffers).MissionIds;
}

FPortMissionOfferSelectionResult UPortDataAsset::BuildPrioritizedMissionSelection(
	const TArray<FPortMissionWeightedOffer>& InWeightedOffers,
	const TArray<FName>& InFallbackOffers,
	int32 PortVisitCount,
	int32 MaxOffers)
{
	FPortMissionOfferSelectionResult SelectionResult;
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
			SelectionResult.VisitGatedRuleCount++;
			continue;
		}

		EligibleWeightedOffers.Add(Offer);
		SelectionResult.EligibleWeightedRuleCount++;
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

	if (Result.Num() > 0)
	{
		if (MaxOffers > 0 && Result.Num() > MaxOffers)
		{
			Result.SetNum(MaxOffers);
		}
		SelectionResult.MissionIds = Result;
		SelectionResult.bUsedWeightedRules = SelectionResult.MissionIds.Num() > 0;
		return SelectionResult;
	}

	for (const FName& OfferId : InFallbackOffers)
	{
		if (!OfferId.IsNone())
		{
			Result.AddUnique(OfferId);
		}
	}

	if (MaxOffers > 0 && Result.Num() > MaxOffers)
	{
		Result.SetNum(MaxOffers);
	}

	SelectionResult.MissionIds = Result;
	SelectionResult.bUsedFallbackOffers = SelectionResult.MissionIds.Num() > 0;
	return SelectionResult;
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
	return BuildPrioritizedUpgradeSelection(
		InWeightedOffers,
		InFallbackOffers,
		PortVisitCount,
		MaxOffers,
		bRotateByVisits).UpgradeIds;
}

FPortUpgradeOfferSelectionResult UPortDataAsset::BuildPrioritizedUpgradeSelection(
	const TArray<FPortUpgradeWeightedOffer>& InWeightedOffers,
	const TArray<FName>& InFallbackOffers,
	int32 PortVisitCount,
	int32 MaxOffers,
	bool bRotateByVisits)
{
	FPortUpgradeOfferSelectionResult SelectionResult;
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
			SelectionResult.VisitGatedRuleCount++;
			continue;
		}

		EligibleWeightedOffers.Add(Offer);
		SelectionResult.EligibleWeightedRuleCount++;
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
		SelectionResult.UpgradeIds = BuildRotatedUpgradeIds(WeightedIds, PortVisitCount, MaxOffers, bRotateByVisits);
		SelectionResult.bUsedWeightedRules = SelectionResult.UpgradeIds.Num() > 0;
		return SelectionResult;
	}

	SelectionResult.UpgradeIds = BuildRotatedUpgradeIds(InFallbackOffers, PortVisitCount, MaxOffers, bRotateByVisits);
	SelectionResult.bUsedFallbackOffers = SelectionResult.UpgradeIds.Num() > 0;
	return SelectionResult;
}

TArray<FName> UPortDataAsset::FilterUpgradeIdsByUnlockedState(
	const TArray<FName>& InUpgradeIds,
	const TSet<FName>& InUnlockedUpgradeIds,
	bool bHideUnlockedUpgrades)
{
	TArray<FName> Result;
	for (const FName& UpgradeId : InUpgradeIds)
	{
		if (UpgradeId.IsNone())
		{
			continue;
		}

		if (bHideUnlockedUpgrades && InUnlockedUpgradeIds.Contains(UpgradeId))
		{
			continue;
		}

		Result.AddUnique(UpgradeId);
	}

	return Result;
}

int32 UPortDataAsset::CalculateAdjustedUpgradeCost(int32 BaseCost, float CostMultiplier)
{
	const int32 SafeBaseCost = FMath::Max(0, BaseCost);
	const float SafeMultiplier = FMath::Max(0.1f, CostMultiplier);
	return FMath::Max(0, FMath::RoundToInt(static_cast<float>(SafeBaseCost) * SafeMultiplier));
}
