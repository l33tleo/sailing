#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
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

private:
	UPROPERTY(EditAnywhere, Category = "Sailing|World")
	int32 TargetActiveChunkRadius = 3;
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

	UFUNCTION(BlueprintPure, Category = "Sailing|Mission")
	TArray<FName> GetRegisteredMissionIds() const;

	UFUNCTION(BlueprintPure, Category = "Sailing|Mission")
	const USailingMissionDataAsset* GetActiveMissionAsset() const;

	UFUNCTION(BlueprintCallable, Category = "Sailing|Mission")
	int32 CompleteActiveMissionByTrigger(ESailingMissionType TriggerType);

	UFUNCTION(BlueprintCallable, Category = "Sailing|Mission")
	int32 CompleteActiveMissionAtLocation(ESailingMissionType TriggerType, const FVector& CompletionLocation);

private:
	UPROPERTY()
	FName ActiveMissionId = NAME_None;

	UPROPERTY()
	TMap<FName, TObjectPtr<USailingMissionDataAsset>> RegisteredMissions;

	UPROPERTY()
	TSet<FName> CompletedMissionIds;

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

	UFUNCTION(BlueprintCallable, Category = "Sailing|Economy")
	bool SpendCredits(int32 Cost);

	UFUNCTION(BlueprintCallable, Category = "Sailing|Economy")
	bool PurchaseUpgrade(const UBoatUpgradeDataAsset* UpgradeData);

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
	TSet<FName> UnlockedUpgradeIds;
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
