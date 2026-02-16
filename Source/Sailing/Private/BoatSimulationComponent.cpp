#include "BoatSimulationComponent.h"
#include "SailboatPawn.h"
#include "Simulation/SailingSimulationMath.h"
#include "WindActor.h"
#include "Kismet/GameplayStatics.h"

UBoatSimulationComponent::UBoatSimulationComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void UBoatSimulationComponent::Simulate(ASailboatPawn* OwnerPawn, float DeltaTime)
{
	if (!OwnerPawn || DeltaTime <= 0.0f)
	{
		return;
	}

	const FVector Forward = OwnerPawn->GetActorForwardVector();
	AWindActor* Wind = FindWind(OwnerPawn->GetWorld());

	// Apparent wind and polar-force model
	if (Wind)
	{
		const FVector TrueWindVec = Wind->GetWindDirection() * Wind->GetWindStrength();
		const FVector BoatVelocity = Forward * CurrentSpeed;
		const FVector ApparentWindVec = TrueWindVec - BoatVelocity;
		const float ApparentWindStrength = ApparentWindVec.Size();

		FVector WindDir;
		float WindStr;
		if (ApparentWindStrength < 1.0f)
		{
			WindDir = Wind->GetWindDirection();
			WindStr = 0.0f;
		}
		else
		{
			WindDir = ApparentWindVec.GetSafeNormal();
			WindStr = ApparentWindStrength;
		}

		const float CosAngle = FVector::DotProduct(Forward, WindDir);
		const float AngleToWind = FMath::RadiansToDegrees(FMath::Acos(FMath::Clamp(CosAngle, -1.0f, 1.0f)));

		FSailingPolarSettings PolarSettings;
		PolarSettings.NoGoZoneAngle = OwnerPawn->NoGoZoneAngle;
		PolarSettings.CloseHauledForce = OwnerPawn->CloseHauledForce;
		PolarSettings.BeamReachForce = OwnerPawn->BeamReachForce;
		PolarSettings.BroadReachForce = OwnerPawn->BroadReachForce;
		PolarSettings.RunningForce = OwnerPawn->RunningForce;
		PolarSettings.PolarSharpness = OwnerPawn->PolarSharpness;
		CurrentSailForce = USailingSimulationMath::CalculateSailForce(WindStr, AngleToWind, PolarSettings);
	}
	else
	{
		CurrentSailForce = 0.0f;
	}

	// Integrate forward speed with quadratic drag
	const float Drag = OwnerPawn->DragCoefficient * FMath::Square(CurrentSpeed);
	const float Acceleration = CurrentSailForce - Drag;
	const float NewSpeed = FMath::Clamp(CurrentSpeed + Acceleration * DeltaTime, 0.0f, OwnerPawn->MaxBoatSpeed);
	const FVector Movement = Forward * NewSpeed * DeltaTime;
	OwnerPawn->AddActorWorldOffset(Movement, false);
	CurrentSpeed = NewSpeed;

	// Keep actor on water level with simple heave oscillation
	if (UWorld* World = OwnerPawn->GetWorld())
	{
		FVector Loc = OwnerPawn->GetActorLocation();
		const float Time = World->GetTimeSeconds();
		Loc.Z = OwnerPawn->WaterZ + FMath::Sin(Time * OwnerPawn->WaveFrequency * 2.0f * PI) * OwnerPawn->WaveAmplitude;
		OwnerPawn->SetActorLocation(Loc, false);

		DebugLogTimer += DeltaTime;
		if (DebugLogTimer > 2.0f)
		{
			UE_LOG(LogTemp, Log, TEXT("BoatSim: Force=%.1f Speed=%.1f Pos=(%.0f,%.0f,%.0f)"),
				CurrentSailForce, CurrentSpeed, Loc.X, Loc.Y, Loc.Z);
			DebugLogTimer = 0.0f;
		}
	}
}

AWindActor* UBoatSimulationComponent::FindWind(UWorld* WorldContext)
{
	if (!WorldContext)
	{
		return nullptr;
	}

	if (CachedWind.IsValid())
	{
		return CachedWind.Get();
	}

	TArray<AActor*> Found;
	UGameplayStatics::GetAllActorsOfClass(WorldContext, AWindActor::StaticClass(), Found);
	if (Found.Num() > 0)
	{
		CachedWind = Cast<AWindActor>(Found[0]);
		return CachedWind.Get();
	}

	return nullptr;
}
