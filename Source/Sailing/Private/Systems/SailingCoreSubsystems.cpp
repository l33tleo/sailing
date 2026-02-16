#include "Systems/SailingCoreSubsystems.h"

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
	UE_LOG(LogTemp, Log, TEXT("WorldStreamingSubsystem deinitialized."));
	Super::Deinitialize();
}

void UMissionSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	UE_LOG(LogTemp, Log, TEXT("MissionSubsystem initialized."));
}

void UMissionSubsystem::Deinitialize()
{
	UE_LOG(LogTemp, Log, TEXT("MissionSubsystem deinitialized."));
	Super::Deinitialize();
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
	UE_LOG(LogTemp, Log, TEXT("SaveSubsystem initialized. SchemaVersion=%d"), SaveSchemaVersion);
}

void USaveSubsystem::Deinitialize()
{
	UE_LOG(LogTemp, Log, TEXT("SaveSubsystem deinitialized."));
	Super::Deinitialize();
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

int32 UTelemetrySubsystem::GetCounterValue(FName EventName) const
{
	if (const int32* Counter = EventCounters.Find(EventName))
	{
		return *Counter;
	}
	return 0;
}
