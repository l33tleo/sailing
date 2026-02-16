#include "Systems/SailingCoreSubsystems.h"
#include "Data/BoatUpgradeDataAsset.h"
#include "Data/SailingMissionDataAsset.h"
#include "SaveGameSailing.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "Modules/ModuleManager.h"

void USailingSimulationSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	SimulationTickRateHz = FMath::Max(10.0f, SimulationTickRateHz);
	UE_LOG(LogTemp, Log, TEXT("SailingSimulationSubsystem initialized. TickRateHz=%.1f"), SimulationTickRateHz);
}

void USailingSimulationSubsystem::Deinitialize()
{
	UE_LOG(LogTemp, Log, TEXT("SailingSimulationSubsystem deinitialized."));
	Super::Deinitialize();
}

void USailingSimulationSubsystem::SetSimulationTickRateHz(float NewTickRateHz)
{
	SimulationTickRateHz = FMath::Clamp(NewTickRateHz, 10.0f, 240.0f);
}

void UWeatherOceanSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	UE_LOG(LogTemp, Log, TEXT("WeatherOceanSubsystem initialized. Seed=%d"), WeatherSeed);
}

void UWeatherOceanSubsystem::Deinitialize()
{
	UE_LOG(LogTemp, Log, TEXT("WeatherOceanSubsystem deinitialized."));
	Super::Deinitialize();
}

void UWorldStreamingSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	TargetActiveChunkRadius = FMath::Max(1, TargetActiveChunkRadius);
	UE_LOG(LogTemp, Log, TEXT("WorldStreamingSubsystem initialized. TargetChunkRadius=%d"), TargetActiveChunkRadius);
}

void UWorldStreamingSubsystem::Deinitialize()
{
	RegisteredPortPoints.Empty();
	PortVisitCounts.Empty();
	LastVisitedPortId = NAME_None;
	UE_LOG(LogTemp, Log, TEXT("WorldStreamingSubsystem deinitialized."));
	Super::Deinitialize();
}

void UWorldStreamingSubsystem::RegisterPortPoint(FName PortId, const FVector& WorldLocation)
{
	if (PortId.IsNone())
	{
		return;
	}

	RegisteredPortPoints.FindOrAdd(PortId) = WorldLocation;
}

void UWorldStreamingSubsystem::ClearPortPoints()
{
	RegisteredPortPoints.Empty();
}

TArray<FName> UWorldStreamingSubsystem::GetRegisteredPortIds() const
{
	TArray<FName> PortIds;
	RegisteredPortPoints.GenerateKeyArray(PortIds);
	PortIds.Sort(FNameLexicalLess());
	return PortIds;
}

bool UWorldStreamingSubsystem::GetPortLocation(FName PortId, FVector& OutWorldLocation) const
{
	if (const FVector* Location = RegisteredPortPoints.Find(PortId))
	{
		OutWorldLocation = *Location;
		return true;
	}
	return false;
}

void UWorldStreamingSubsystem::MarkPortVisited(FName PortId)
{
	if (PortId.IsNone())
	{
		return;
	}

	LastVisitedPortId = PortId;
	PortVisitCounts.FindOrAdd(PortId) += 1;
}

void UWorldStreamingSubsystem::SetPortVisitStats(FName InLastVisitedPortId, const TMap<FName, int32>& InPortVisitCounts)
{
	LastVisitedPortId = InLastVisitedPortId;
	PortVisitCounts.Empty();
	for (const TPair<FName, int32>& Pair : InPortVisitCounts)
	{
		if (!Pair.Key.IsNone())
		{
			PortVisitCounts.Add(Pair.Key, FMath::Max(0, Pair.Value));
		}
	}
}

void UMissionSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	ReloadMissionAssets();
	UE_LOG(LogTemp, Log, TEXT("MissionSubsystem initialized."));
}

void UMissionSubsystem::Deinitialize()
{
	UE_LOG(LogTemp, Log, TEXT("MissionSubsystem deinitialized."));
	Super::Deinitialize();
}

bool UMissionSubsystem::ActivateMissionAsset(const USailingMissionDataAsset* MissionData)
{
	if (!RegisterMissionAsset(MissionData))
	{
		return false;
	}

	if (CompletedMissionIds.Contains(MissionData->MissionId) && !MissionData->bRepeatable)
	{
		return false;
	}

	ActiveMissionId = MissionData->MissionId;
	return true;
}

bool UMissionSubsystem::RegisterMissionAsset(const USailingMissionDataAsset* MissionData)
{
	if (!MissionData || MissionData->MissionId.IsNone())
	{
		return false;
	}

	RegisteredMissions.FindOrAdd(MissionData->MissionId) = const_cast<USailingMissionDataAsset*>(MissionData);
	return true;
}

int32 UMissionSubsystem::ReloadMissionAssets()
{
	RegisteredMissions.Empty();

	const FName ScanPath = MissionAssetPath.IsNone() ? FName(TEXT("/Game")) : MissionAssetPath;

	FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry"));
	FARFilter Filter;
	Filter.PackagePaths.Add(ScanPath);
	Filter.ClassPaths.Add(USailingMissionDataAsset::StaticClass()->GetClassPathName());
	Filter.bRecursivePaths = true;

	TArray<FAssetData> MissionAssets;
	AssetRegistryModule.Get().GetAssets(Filter, MissionAssets);

	int32 RegisteredCount = 0;
	for (const FAssetData& AssetData : MissionAssets)
	{
		if (const USailingMissionDataAsset* MissionData = Cast<USailingMissionDataAsset>(AssetData.GetAsset()))
		{
			RegisteredCount += RegisterMissionAsset(MissionData) ? 1 : 0;
		}
	}

	UE_LOG(LogTemp, Log, TEXT("MissionSubsystem: Registered %d mission assets from %s"), RegisteredCount, *ScanPath.ToString());
	return RegisteredCount;
}

void UMissionSubsystem::SetMissionAssetPath(FName InMissionAssetPath)
{
	const FName NormalizedPath = InMissionAssetPath.IsNone() ? FName(TEXT("/Game")) : InMissionAssetPath;
	if (MissionAssetPath == NormalizedPath)
	{
		return;
	}

	MissionAssetPath = NormalizedPath;
	ReloadMissionAssets();
}

TArray<FName> UMissionSubsystem::GetRegisteredMissionIds() const
{
	TArray<FName> MissionIds;
	RegisteredMissions.GenerateKeyArray(MissionIds);
	MissionIds.Sort(FNameLexicalLess());
	return MissionIds;
}

const USailingMissionDataAsset* UMissionSubsystem::GetActiveMissionAsset() const
{
	if (ActiveMissionId.IsNone())
	{
		return nullptr;
	}

	if (const TObjectPtr<USailingMissionDataAsset>* MissionPtr = RegisteredMissions.Find(ActiveMissionId))
	{
		return MissionPtr ? MissionPtr->Get() : nullptr;
	}
	return nullptr;
}

const USailingMissionDataAsset* UMissionSubsystem::GetMissionAssetById(FName MissionId) const
{
	if (MissionId.IsNone())
	{
		return nullptr;
	}

	if (const TObjectPtr<USailingMissionDataAsset>* MissionPtr = RegisteredMissions.Find(MissionId))
	{
		return MissionPtr ? MissionPtr->Get() : nullptr;
	}
	return nullptr;
}

FText UMissionSubsystem::GetActiveMissionDisplayName() const
{
	if (const USailingMissionDataAsset* ActiveMission = GetActiveMissionAsset())
	{
		return ActiveMission->DisplayName;
	}
	return FText::GetEmpty();
}

FText UMissionSubsystem::GetMissionDisplayNameById(FName MissionId) const
{
	if (MissionId.IsNone())
	{
		return FText::GetEmpty();
	}

	if (const TObjectPtr<USailingMissionDataAsset>* MissionPtr = RegisteredMissions.Find(MissionId))
	{
		if (const USailingMissionDataAsset* MissionData = MissionPtr ? MissionPtr->Get() : nullptr)
		{
			return MissionData->DisplayName;
		}
	}

	return FText::FromName(MissionId);
}

bool UMissionSubsystem::GetActiveMissionObjectiveLocation(FVector& OutLocation) const
{
	if (const USailingMissionDataAsset* ActiveMission = GetActiveMissionAsset())
	{
		if (ActiveMission->bRequireLocationMatch)
		{
			OutLocation = ActiveMission->EndWorldLocation;
			return true;
		}
	}
	return false;
}

int32 UMissionSubsystem::CompleteActiveMissionByTrigger(ESailingMissionType TriggerType)
{
	return CompleteActiveMissionAtLocation(TriggerType, FVector::ZeroVector);
}

int32 UMissionSubsystem::CompleteActiveMissionAtLocation(ESailingMissionType TriggerType, const FVector& CompletionLocation)
{
	if (ActiveMissionId.IsNone())
	{
		return 0;
	}

	const TObjectPtr<USailingMissionDataAsset>* MissionPtr = RegisteredMissions.Find(ActiveMissionId);
	if (!MissionPtr || !(*MissionPtr))
	{
		return 0;
	}

	const USailingMissionDataAsset* ActiveMission = MissionPtr->Get();
	if (!ActiveMission || ActiveMission->MissionType != TriggerType)
	{
		return 0;
	}

	if (ActiveMission->bRequireLocationMatch)
	{
		const float AllowedRadius = FMath::Max(50.0f, ActiveMission->CompletionRadius);
		const float DistSq = FVector::DistSquared(ActiveMission->EndWorldLocation, CompletionLocation);
		if (DistSq > FMath::Square(AllowedRadius))
		{
			return 0;
		}
	}

	const int32 RewardCredits = FMath::Max(0, ActiveMission->RewardCredits);
	const FName CompletedMissionId = ActiveMissionId;
	CompletedMissionIds.Add(CompletedMissionId);

	if (!ActiveMission->NextMissionId.IsNone() && RegisteredMissions.Contains(ActiveMission->NextMissionId))
	{
		ActiveMissionId = ActiveMission->NextMissionId;
	}
	else
	{
		ActiveMissionId = NAME_None;
	}

	UE_LOG(LogTemp, Log, TEXT("MissionSubsystem: Completed '%s', next='%s', reward=%d"),
		*CompletedMissionId.ToString(), *ActiveMissionId.ToString(), RewardCredits);

	if (ActiveMissionId.IsNone())
	{
		ActivateFallbackMission();
	}

	return RewardCredits;
}

bool UMissionSubsystem::ActivateFallbackMission()
{
	if (!ActiveMissionId.IsNone())
	{
		return false;
	}

	TArray<FName> MissionIds = GetRegisteredMissionIds();
	for (const FName& MissionId : MissionIds)
	{
		const TObjectPtr<USailingMissionDataAsset>* MissionPtr = RegisteredMissions.Find(MissionId);
		const USailingMissionDataAsset* Mission = MissionPtr ? MissionPtr->Get() : nullptr;
		if (!Mission)
		{
			continue;
		}

		const bool bAlreadyCompleted = CompletedMissionIds.Contains(MissionId);
		if (bAlreadyCompleted && !Mission->bRepeatable)
		{
			continue;
		}

		ActiveMissionId = MissionId;
		return true;
	}

	return false;
}

bool UMissionSubsystem::CycleToNextMission()
{
	const TArray<FName> MissionIds = GetRegisteredMissionIds();
	if (MissionIds.Num() == 0)
	{
		return false;
	}

	int32 StartIndex = 0;
	if (!ActiveMissionId.IsNone())
	{
		const int32 CurrentIndex = MissionIds.IndexOfByKey(ActiveMissionId);
		if (CurrentIndex != INDEX_NONE)
		{
			StartIndex = (CurrentIndex + 1) % MissionIds.Num();
		}
	}

	for (int32 Offset = 0; Offset < MissionIds.Num(); ++Offset)
	{
		const int32 Idx = (StartIndex + Offset) % MissionIds.Num();
		const FName CandidateId = MissionIds[Idx];
		const TObjectPtr<USailingMissionDataAsset>* MissionPtr = RegisteredMissions.Find(CandidateId);
		const USailingMissionDataAsset* Mission = MissionPtr ? MissionPtr->Get() : nullptr;
		if (!Mission)
		{
			continue;
		}

		const bool bCompleted = CompletedMissionIds.Contains(CandidateId);
		if (bCompleted && !Mission->bRepeatable)
		{
			continue;
		}

		ActiveMissionId = CandidateId;
		return true;
	}

	return false;
}

bool UMissionSubsystem::ActivateMissionFromCandidates(const TArray<FName>& CandidateMissionIds, bool bCycleFromCurrentMission)
{
	if (CandidateMissionIds.Num() == 0)
	{
		return false;
	}

	TArray<FName> OrderedCandidates;
	for (const FName& CandidateId : CandidateMissionIds)
	{
		if (!CandidateId.IsNone())
		{
			OrderedCandidates.AddUnique(CandidateId);
		}
	}
	if (OrderedCandidates.Num() == 0)
	{
		return false;
	}

	int32 StartIndex = 0;
	if (bCycleFromCurrentMission && !ActiveMissionId.IsNone())
	{
		const int32 CurrentIdx = OrderedCandidates.IndexOfByKey(ActiveMissionId);
		if (CurrentIdx != INDEX_NONE)
		{
			StartIndex = (CurrentIdx + 1) % OrderedCandidates.Num();
		}
	}

	for (int32 Offset = 0; Offset < OrderedCandidates.Num(); ++Offset)
	{
		const int32 Idx = (StartIndex + Offset) % OrderedCandidates.Num();
		const FName CandidateId = OrderedCandidates[Idx];
		const TObjectPtr<USailingMissionDataAsset>* MissionPtr = RegisteredMissions.Find(CandidateId);
		const USailingMissionDataAsset* Mission = MissionPtr ? MissionPtr->Get() : nullptr;
		if (!Mission)
		{
			continue;
		}

		const bool bAlreadyCompleted = CompletedMissionIds.Contains(CandidateId);
		if (bAlreadyCompleted && !Mission->bRepeatable)
		{
			continue;
		}

		ActiveMissionId = CandidateId;
		return true;
	}

	return false;
}

void UMissionSubsystem::SetCompletedMissionIds(const TArray<FName>& MissionIds)
{
	CompletedMissionIds.Empty();
	for (const FName& MissionId : MissionIds)
	{
		if (!MissionId.IsNone())
		{
			CompletedMissionIds.Add(MissionId);
		}
	}
}

TArray<FName> UMissionSubsystem::GetCompletedMissionIds() const
{
	TArray<FName> Result;
	CompletedMissionIds.GenerateKeyArray(Result);
	Result.Sort(FNameLexicalLess());
	return Result;
}

void UMissionSubsystem::RecordMissionBoardSelection(FName PortId, FName MissionId)
{
	if (PortId.IsNone() || MissionId.IsNone())
	{
		return;
	}

	FMissionBoardSelectionEntry Entry;
	Entry.PortId = PortId;
	Entry.MissionId = MissionId;
	Entry.AcceptedTime = FDateTime::UtcNow();
	MissionBoardSelectionHistory.Add(Entry);
}

void UMissionSubsystem::SetMissionBoardSelectionHistory(const TArray<FMissionBoardSelectionEntry>& InHistory)
{
	MissionBoardSelectionHistory.Empty();
	for (const FMissionBoardSelectionEntry& Entry : InHistory)
	{
		if (Entry.PortId.IsNone() || Entry.MissionId.IsNone())
		{
			continue;
		}
		MissionBoardSelectionHistory.Add(Entry);
	}
}

TArray<FMissionBoardSelectionEntry> UMissionSubsystem::GetRecentMissionBoardSelectionsForPort(FName PortId, int32 MaxEntries) const
{
	TArray<FMissionBoardSelectionEntry> Result;
	if (PortId.IsNone() || MaxEntries <= 0)
	{
		return Result;
	}

	for (int32 i = MissionBoardSelectionHistory.Num() - 1; i >= 0; --i)
	{
		const FMissionBoardSelectionEntry& Entry = MissionBoardSelectionHistory[i];
		if (Entry.PortId != PortId)
		{
			continue;
		}

		Result.Add(Entry);
		if (Result.Num() >= MaxEntries)
		{
			break;
		}
	}

	return Result;
}

void UEconomySubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	ReloadUpgradeAssets();
	UE_LOG(LogTemp, Log, TEXT("EconomySubsystem initialized. Credits=%d Condition=%d%%"), Credits, BoatConditionPercent);
}

void UEconomySubsystem::Deinitialize()
{
	RegisteredUpgrades.Empty();
	UE_LOG(LogTemp, Log, TEXT("EconomySubsystem deinitialized."));
	Super::Deinitialize();
}

void UEconomySubsystem::AddCredits(int32 Delta)
{
	Credits = FMath::Max(0, Credits + Delta);
}

void UEconomySubsystem::SetCredits(int32 InCredits)
{
	Credits = FMath::Max(0, InCredits);
}

void UEconomySubsystem::SetBoatConditionPercent(int32 InPercent)
{
	BoatConditionPercent = FMath::Clamp(InPercent, 0, 100);
}

void UEconomySubsystem::ApplyBoatWear(int32 WearAmount)
{
	if (WearAmount <= 0)
	{
		return;
	}

	BoatConditionPercent = FMath::Clamp(BoatConditionPercent - WearAmount, 0, 100);
}

bool UEconomySubsystem::SpendCredits(int32 Cost)
{
	if (Cost <= 0)
	{
		return true;
	}

	if (Credits < Cost)
	{
		return false;
	}

	Credits -= Cost;
	return true;
}

bool UEconomySubsystem::RepairBoatToFull(int32 CostPerPercentPoint)
{
	const int32 ClampedCostPerPoint = FMath::Max(0, CostPerPercentPoint);
	const int32 MissingCondition = 100 - FMath::Clamp(BoatConditionPercent, 0, 100);
	if (MissingCondition <= 0)
	{
		return true;
	}

	const int32 TotalCost = MissingCondition * ClampedCostPerPoint;
	if (!SpendCredits(TotalCost))
	{
		return false;
	}

	BoatConditionPercent = 100;
	return true;
}

bool UEconomySubsystem::PurchaseUpgrade(const UBoatUpgradeDataAsset* UpgradeData)
{
	if (!UpgradeData || UpgradeData->UpgradeId.IsNone())
	{
		return false;
	}

	RegisterUpgradeAsset(UpgradeData);
	return PurchaseUpgradeById(UpgradeData->UpgradeId, UpgradeData->CreditCost);
}

bool UEconomySubsystem::PurchaseUpgradeById(FName UpgradeId, int32 CostOverride)
{
	if (UpgradeId.IsNone())
	{
		return false;
	}

	const UBoatUpgradeDataAsset* UpgradeData = GetUpgradeAssetById(UpgradeId);
	if (!UpgradeData)
	{
		return false;
	}

	if (UnlockedUpgradeIds.Contains(UpgradeId))
	{
		return true;
	}

	const int32 EffectiveCost = CostOverride == INDEX_NONE
		? FMath::Max(0, UpgradeData->CreditCost)
		: FMath::Max(0, CostOverride);
	if (!SpendCredits(EffectiveCost))
	{
		return false;
	}

	UnlockedUpgradeIds.Add(UpgradeId);
	return true;
}

bool UEconomySubsystem::RegisterUpgradeAsset(const UBoatUpgradeDataAsset* UpgradeData)
{
	if (!UpgradeData || UpgradeData->UpgradeId.IsNone())
	{
		return false;
	}

	RegisteredUpgrades.FindOrAdd(UpgradeData->UpgradeId) = const_cast<UBoatUpgradeDataAsset*>(UpgradeData);
	return true;
}

int32 UEconomySubsystem::ReloadUpgradeAssets()
{
	RegisteredUpgrades.Empty();

	const FName ScanPath = UpgradeAssetPath.IsNone() ? FName(TEXT("/Game")) : UpgradeAssetPath;

	FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry"));
	FARFilter Filter;
	Filter.PackagePaths.Add(ScanPath);
	Filter.ClassPaths.Add(UBoatUpgradeDataAsset::StaticClass()->GetClassPathName());
	Filter.bRecursivePaths = true;

	TArray<FAssetData> UpgradeAssets;
	AssetRegistryModule.Get().GetAssets(Filter, UpgradeAssets);

	int32 RegisteredCount = 0;
	for (const FAssetData& AssetData : UpgradeAssets)
	{
		if (const UBoatUpgradeDataAsset* UpgradeData = Cast<UBoatUpgradeDataAsset>(AssetData.GetAsset()))
		{
			RegisteredCount += RegisterUpgradeAsset(UpgradeData) ? 1 : 0;
		}
	}

	UE_LOG(LogTemp, Log, TEXT("EconomySubsystem: Registered %d upgrade assets from %s"), RegisteredCount, *ScanPath.ToString());
	return RegisteredCount;
}

void UEconomySubsystem::SetUpgradeAssetPath(FName InUpgradeAssetPath)
{
	const FName NormalizedPath = InUpgradeAssetPath.IsNone() ? FName(TEXT("/Game")) : InUpgradeAssetPath;
	if (UpgradeAssetPath == NormalizedPath)
	{
		return;
	}

	UpgradeAssetPath = NormalizedPath;
	ReloadUpgradeAssets();
}

TArray<FName> UEconomySubsystem::GetRegisteredUpgradeIds() const
{
	TArray<FName> UpgradeIds;
	RegisteredUpgrades.GenerateKeyArray(UpgradeIds);
	UpgradeIds.Sort(FNameLexicalLess());
	return UpgradeIds;
}

const UBoatUpgradeDataAsset* UEconomySubsystem::GetUpgradeAssetById(FName UpgradeId) const
{
	if (UpgradeId.IsNone())
	{
		return nullptr;
	}

	if (const TObjectPtr<UBoatUpgradeDataAsset>* UpgradePtr = RegisteredUpgrades.Find(UpgradeId))
	{
		return UpgradePtr ? UpgradePtr->Get() : nullptr;
	}
	return nullptr;
}

void UEconomySubsystem::GetCombinedUpgradeMultipliers(
	float& OutMaxSpeedMultiplier,
	float& OutDragMultiplier,
	float& OutTurnRateMultiplier) const
{
	OutMaxSpeedMultiplier = 1.0f;
	OutDragMultiplier = 1.0f;
	OutTurnRateMultiplier = 1.0f;

	for (const FName& UpgradeId : UnlockedUpgradeIds)
	{
		const UBoatUpgradeDataAsset* UpgradeData = GetUpgradeAssetById(UpgradeId);
		if (!UpgradeData)
		{
			continue;
		}

		OutMaxSpeedMultiplier *= FMath::Max(0.1f, UpgradeData->MaxSpeedMultiplier);
		OutDragMultiplier *= FMath::Max(0.1f, UpgradeData->DragMultiplier);
		OutTurnRateMultiplier *= FMath::Max(0.1f, UpgradeData->TurnRateMultiplier);
	}
}

void UEconomySubsystem::SetUnlockedUpgrades(const TArray<FName>& UpgradeIds)
{
	UnlockedUpgradeIds.Empty();
	for (const FName& UpgradeId : UpgradeIds)
	{
		if (!UpgradeId.IsNone())
		{
			UnlockedUpgradeIds.Add(UpgradeId);
		}
	}
}

TArray<FName> UEconomySubsystem::GetUnlockedUpgradeIds() const
{
	TArray<FName> Result;
	UnlockedUpgradeIds.GenerateKeyArray(Result);
	Result.Sort(FNameLexicalLess());
	return Result;
}

void UUISubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	UE_LOG(LogTemp, Log, TEXT("UISubsystem initialized. Assist=%s"), bAccessibilityAssistEnabled ? TEXT("true") : TEXT("false"));
}

void UUISubsystem::Deinitialize()
{
	UE_LOG(LogTemp, Log, TEXT("UISubsystem deinitialized."));
	Super::Deinitialize();
}

void USaveSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	SaveSchemaVersion = USaveGameSailing::CurrentSaveSchemaVersion;
	UE_LOG(LogTemp, Log, TEXT("SaveSubsystem initialized. SchemaVersion=%d"), SaveSchemaVersion);
}

void USaveSubsystem::Deinitialize()
{
	UE_LOG(LogTemp, Log, TEXT("SaveSubsystem deinitialized."));
	Super::Deinitialize();
}

bool USaveSubsystem::MigrateSaveGame(USaveGameSailing* SaveGame) const
{
	if (!SaveGame)
	{
		return false;
	}

	SaveGame->EnsureCompatibility();
	return true;
}

void UTelemetrySubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	EventCounters.Empty();
	UE_LOG(LogTemp, Log, TEXT("TelemetrySubsystem initialized."));
}

void UTelemetrySubsystem::Deinitialize()
{
	EventCounters.Empty();
	UE_LOG(LogTemp, Log, TEXT("TelemetrySubsystem deinitialized."));
	Super::Deinitialize();
}

void UTelemetrySubsystem::RecordCounterEvent(FName EventName, int32 Delta)
{
	if (EventName.IsNone() || Delta == 0)
	{
		return;
	}

	EventCounters.FindOrAdd(EventName) += Delta;
}

void UTelemetrySubsystem::SetCounterValue(FName EventName, int32 Value)
{
	if (EventName.IsNone())
	{
		return;
	}

	EventCounters.FindOrAdd(EventName) = FMath::Max(0, Value);
}

void UTelemetrySubsystem::SetAllCounters(const TMap<FName, int32>& InCounters)
{
	EventCounters.Empty();
	for (const TPair<FName, int32>& Pair : InCounters)
	{
		if (!Pair.Key.IsNone())
		{
			EventCounters.Add(Pair.Key, FMath::Max(0, Pair.Value));
		}
	}
}

int32 UTelemetrySubsystem::GetCounterValue(FName EventName) const
{
	if (const int32* Counter = EventCounters.Find(EventName))
	{
		return *Counter;
	}
	return 0;
}
