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

FText UMissionSubsystem::GetActiveMissionDisplayName() const
{
	if (const USailingMissionDataAsset* ActiveMission = GetActiveMissionAsset())
	{
		return ActiveMission->DisplayName;
	}
	return FText::GetEmpty();
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

void UEconomySubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	UE_LOG(LogTemp, Log, TEXT("EconomySubsystem initialized. Credits=%d"), Credits);
}

void UEconomySubsystem::Deinitialize()
{
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

bool UEconomySubsystem::PurchaseUpgrade(const UBoatUpgradeDataAsset* UpgradeData)
{
	if (!UpgradeData || UpgradeData->UpgradeId.IsNone())
	{
		return false;
	}

	if (UnlockedUpgradeIds.Contains(UpgradeData->UpgradeId))
	{
		return true;
	}

	if (!SpendCredits(UpgradeData->CreditCost))
	{
		return false;
	}

	UnlockedUpgradeIds.Add(UpgradeData->UpgradeId);
	return true;
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
