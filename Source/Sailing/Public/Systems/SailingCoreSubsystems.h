#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "Data/SailingProgressionTypes.h"
#include "SailingCoreSubsystems.generated.h"

class UBoatUpgradeDataAsset;
class USailingMissionDataAsset;
class USaveGameSailing;
enum class ESailingMissionType : uint8;

/**
 * Subsystem skeleton for sailing simulation systems.
 * This class intentionally starts lean and is expanded in iterative phases.
 */
UCLASS()
class SAILING_API USailingSimulationSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	UFUNCTION(BlueprintPure, Category = "Sailing|Simulation")
	float GetSimulationTickRateHz() const { return SimulationTickRateHz; }

	UFUNCTION(BlueprintCallable, Category = "Sailing|Simulation")
	void SetSimulationTickRateHz(float NewTickRateHz);

private:
	UPROPERTY(EditAnywhere, Category = "Sailing|Simulation")
	float SimulationTickRateHz = 60.0f;
};

UCLASS()
class SAILING_API UWeatherOceanSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	UFUNCTION(BlueprintPure, Category = "Sailing|Weather")
	int32 GetWeatherSeed() const { return WeatherSeed; }

	UFUNCTION(BlueprintCallable, Category = "Sailing|Weather")
	void SetWeatherSeed(int32 InSeed) { WeatherSeed = InSeed; }

private:
	UPROPERTY()
	int32 WeatherSeed = 1337;
};

UCLASS()
class SAILING_API UWorldStreamingSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	UFUNCTION(BlueprintPure, Category = "Sailing|World")
	int32 GetTargetActiveChunkRadius() const { return TargetActiveChunkRadius; }

	UFUNCTION(BlueprintCallable, Category = "Sailing|World")
	void RegisterPortPoint(FName PortId, const FVector& WorldLocation);

	UFUNCTION(BlueprintCallable, Category = "Sailing|World")
	void ClearPortPoints();

	UFUNCTION(BlueprintPure, Category = "Sailing|World")
	TArray<FName> GetRegisteredPortIds() const;

	UFUNCTION(BlueprintPure, Category = "Sailing|World")
	bool GetPortLocation(FName PortId, FVector& OutWorldLocation) const;

	UFUNCTION(BlueprintCallable, Category = "Sailing|World")
	void MarkPortVisited(FName PortId);

	UFUNCTION(BlueprintPure, Category = "Sailing|World")
	FName GetLastVisitedPortId() const { return LastVisitedPortId; }

	UFUNCTION(BlueprintPure, Category = "Sailing|World")
	TMap<FName, int32> GetPortVisitCounts() const { return PortVisitCounts; }

	UFUNCTION(BlueprintCallable, Category = "Sailing|World")
	void SetPortVisitStats(FName InLastVisitedPortId, const TMap<FName, int32>& InPortVisitCounts);

private:
	UPROPERTY(EditAnywhere, Category = "Sailing|World")
	int32 TargetActiveChunkRadius = 3;

	UPROPERTY()
	TMap<FName, FVector> RegisteredPortPoints;

	UPROPERTY()
	FName LastVisitedPortId = NAME_None;

	UPROPERTY()
	TMap<FName, int32> PortVisitCounts;
};

UCLASS()
class SAILING_API UMissionSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	UFUNCTION(BlueprintPure, Category = "Sailing|Mission")
	FName GetActiveMissionId() const { return ActiveMissionId; }

	UFUNCTION(BlueprintCallable, Category = "Sailing|Mission")
	void SetActiveMissionId(FName MissionId) { ActiveMissionId = MissionId; }

	UFUNCTION(BlueprintCallable, Category = "Sailing|Mission")
	bool ActivateMissionAsset(const USailingMissionDataAsset* MissionData);

	UFUNCTION(BlueprintCallable, Category = "Sailing|Mission")
	bool RegisterMissionAsset(const USailingMissionDataAsset* MissionData);

	UFUNCTION(BlueprintCallable, Category = "Sailing|Mission")
	int32 ReloadMissionAssets();

	UFUNCTION(BlueprintCallable, Category = "Sailing|Mission")
	void SetMissionAssetPath(FName InMissionAssetPath);

	UFUNCTION(BlueprintPure, Category = "Sailing|Mission")
	FName GetMissionAssetPath() const { return MissionAssetPath; }

	UFUNCTION(BlueprintPure, Category = "Sailing|Mission")
	TArray<FName> GetRegisteredMissionIds() const;

	UFUNCTION(BlueprintPure, Category = "Sailing|Mission")
	const USailingMissionDataAsset* GetActiveMissionAsset() const;

	UFUNCTION(BlueprintPure, Category = "Sailing|Mission")
	const USailingMissionDataAsset* GetMissionAssetById(FName MissionId) const;

	UFUNCTION(BlueprintPure, Category = "Sailing|Mission")
	FText GetActiveMissionDisplayName() const;

	UFUNCTION(BlueprintPure, Category = "Sailing|Mission")
	FText GetMissionDisplayNameById(FName MissionId) const;

	UFUNCTION(BlueprintPure, Category = "Sailing|Mission")
	bool GetActiveMissionObjectiveLocation(FVector& OutLocation) const;

	UFUNCTION(BlueprintCallable, Category = "Sailing|Mission")
	int32 CompleteActiveMissionByTrigger(ESailingMissionType TriggerType);

	UFUNCTION(BlueprintCallable, Category = "Sailing|Mission")
	int32 CompleteActiveMissionAtLocation(ESailingMissionType TriggerType, const FVector& CompletionLocation);

	UFUNCTION(BlueprintCallable, Category = "Sailing|Mission")
	bool ActivateFallbackMission();

	UFUNCTION(BlueprintCallable, Category = "Sailing|Mission")
	bool CycleToNextMission();

	UFUNCTION(BlueprintCallable, Category = "Sailing|Mission")
	bool ActivateMissionFromCandidates(const TArray<FName>& CandidateMissionIds, bool bCycleFromCurrentMission);

	UFUNCTION(BlueprintCallable, Category = "Sailing|Mission")
	void SetCompletedMissionIds(const TArray<FName>& MissionIds);

	UFUNCTION(BlueprintPure, Category = "Sailing|Mission")
	TArray<FName> GetCompletedMissionIds() const;

	UFUNCTION(BlueprintCallable, Category = "Sailing|MissionBoard")
	void RecordMissionBoardSelection(FName PortId, FName MissionId);

	UFUNCTION(BlueprintPure, Category = "Sailing|MissionBoard")
	TArray<FMissionBoardSelectionEntry> GetMissionBoardSelectionHistory() const { return MissionBoardSelectionHistory; }

	UFUNCTION(BlueprintCallable, Category = "Sailing|MissionBoard")
	void SetMissionBoardSelectionHistory(const TArray<FMissionBoardSelectionEntry>& InHistory);

	UFUNCTION(BlueprintPure, Category = "Sailing|MissionBoard")
	TArray<FMissionBoardSelectionEntry> GetRecentMissionBoardSelectionsForPort(FName PortId, int32 MaxEntries = 5) const;

private:
	UPROPERTY()
	FName ActiveMissionId = NAME_None;

	UPROPERTY()
	TMap<FName, TObjectPtr<USailingMissionDataAsset>> RegisteredMissions;

	UPROPERTY()
	TSet<FName> CompletedMissionIds;

	UPROPERTY()
	TArray<FMissionBoardSelectionEntry> MissionBoardSelectionHistory;

	UPROPERTY(EditAnywhere, Category = "Sailing|Mission")
	FName MissionAssetPath = TEXT("/Game");
};

UCLASS()
class SAILING_API UEconomySubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	UFUNCTION(BlueprintPure, Category = "Sailing|Economy")
	int32 GetCredits() const { return Credits; }

	UFUNCTION(BlueprintCallable, Category = "Sailing|Economy")
	void AddCredits(int32 Delta);

	UFUNCTION(BlueprintCallable, Category = "Sailing|Economy")
	void SetCredits(int32 InCredits);

	UFUNCTION(BlueprintPure, Category = "Sailing|Economy")
	int32 GetBoatConditionPercent() const { return BoatConditionPercent; }

	UFUNCTION(BlueprintCallable, Category = "Sailing|Economy")
	void SetBoatConditionPercent(int32 InPercent);

	UFUNCTION(BlueprintCallable, Category = "Sailing|Economy")
	void ApplyBoatWear(int32 WearAmount);

	UFUNCTION(BlueprintCallable, Category = "Sailing|Economy")
	bool SpendCredits(int32 Cost);

	UFUNCTION(BlueprintCallable, Category = "Sailing|Economy")
	bool RepairBoatToFull(int32 CostPerPercentPoint = 2);

	UFUNCTION(BlueprintCallable, Category = "Sailing|Economy")
	bool PurchaseUpgrade(const UBoatUpgradeDataAsset* UpgradeData);

	UFUNCTION(BlueprintCallable, Category = "Sailing|Economy")
	bool PurchaseUpgradeById(FName UpgradeId, int32 CostOverride = INDEX_NONE);

	UFUNCTION(BlueprintCallable, Category = "Sailing|Economy")
	bool RegisterUpgradeAsset(const UBoatUpgradeDataAsset* UpgradeData);

	UFUNCTION(BlueprintCallable, Category = "Sailing|Economy")
	int32 ReloadUpgradeAssets();

	UFUNCTION(BlueprintCallable, Category = "Sailing|Economy")
	void SetUpgradeAssetPath(FName InUpgradeAssetPath);

	UFUNCTION(BlueprintPure, Category = "Sailing|Economy")
	FName GetUpgradeAssetPath() const { return UpgradeAssetPath; }

	UFUNCTION(BlueprintPure, Category = "Sailing|Economy")
	TArray<FName> GetRegisteredUpgradeIds() const;

	UFUNCTION(BlueprintPure, Category = "Sailing|Economy")
	const UBoatUpgradeDataAsset* GetUpgradeAssetById(FName UpgradeId) const;

	UFUNCTION(BlueprintPure, Category = "Sailing|Economy")
	void GetCombinedUpgradeMultipliers(float& OutMaxSpeedMultiplier, float& OutDragMultiplier, float& OutTurnRateMultiplier) const;

	UFUNCTION(BlueprintPure, Category = "Sailing|Economy")
	bool IsUpgradeUnlocked(FName UpgradeId) const { return UnlockedUpgradeIds.Contains(UpgradeId); }

	UFUNCTION(BlueprintCallable, Category = "Sailing|Economy")
	void SetUnlockedUpgrades(const TArray<FName>& UpgradeIds);

	UFUNCTION(BlueprintPure, Category = "Sailing|Economy")
	TArray<FName> GetUnlockedUpgradeIds() const;

private:
	UPROPERTY()
	int32 Credits = 0;

	UPROPERTY()
	int32 BoatConditionPercent = 100;

	UPROPERTY()
	TSet<FName> UnlockedUpgradeIds;

	UPROPERTY()
	TMap<FName, TObjectPtr<UBoatUpgradeDataAsset>> RegisteredUpgrades;

	UPROPERTY(EditAnywhere, Category = "Sailing|Economy")
	FName UpgradeAssetPath = TEXT("/Game");
};

UCLASS()
class SAILING_API UUISubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	UFUNCTION(BlueprintPure, Category = "Sailing|UI")
	bool IsAccessibilityAssistEnabled() const { return bAccessibilityAssistEnabled; }

	UFUNCTION(BlueprintCallable, Category = "Sailing|UI")
	void SetAccessibilityAssistEnabled(bool bEnabled) { bAccessibilityAssistEnabled = bEnabled; }

private:
	UPROPERTY()
	bool bAccessibilityAssistEnabled = true;
};

UCLASS()
class SAILING_API USaveSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	UFUNCTION(BlueprintPure, Category = "Sailing|Save")
	int32 GetSaveSchemaVersion() const { return SaveSchemaVersion; }

	UFUNCTION(BlueprintCallable, Category = "Sailing|Save")
	bool MigrateSaveGame(USaveGameSailing* SaveGame) const;

private:
	UPROPERTY()
	int32 SaveSchemaVersion = 2;
};

UCLASS()
class SAILING_API UTelemetrySubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	UFUNCTION(BlueprintCallable, Category = "Sailing|Telemetry")
	void RecordCounterEvent(FName EventName, int32 Delta = 1);

	UFUNCTION(BlueprintCallable, Category = "Sailing|Telemetry")
	void SetCounterValue(FName EventName, int32 Value);

	UFUNCTION(BlueprintCallable, Category = "Sailing|Telemetry")
	void SetAllCounters(const TMap<FName, int32>& InCounters);

	UFUNCTION(BlueprintPure, Category = "Sailing|Telemetry")
	int32 GetCounterValue(FName EventName) const;

	UFUNCTION(BlueprintPure, Category = "Sailing|Telemetry")
	TMap<FName, int32> GetAllCounters() const { return EventCounters; }

private:
	UPROPERTY()
	TMap<FName, int32> EventCounters;
};
