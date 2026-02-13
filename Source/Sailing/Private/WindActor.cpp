#include "WindActor.h"
#include "Math/UnrealMathUtility.h"

AWindActor::AWindActor()
{
	PrimaryActorTick.bCanEverTick = true;
}

FVector AWindActor::GetWindDirection() const
{
	FVector Dir = WindDirection.GetSafeNormal();
	if (DirectionOscillationAngle > 0.0f && GetWorld())
	{
		float T = GetWorld()->GetTimeSeconds();
		float YawDeg = DirectionOscillationAngle * FMath::Sin(2.0f * UE_PI * DirectionOscillationFrequency * T);
		FRotator OscRot(0.0f, YawDeg, 0.0f);
		Dir = OscRot.RotateVector(Dir);
	}
	return Dir.GetSafeNormal();
}

float AWindActor::GetWindStrength() const
{
	float S = BaseWindStrength;
	if (GustStrength > 0.0f && GetWorld())
	{
		float T = GetWorld()->GetTimeSeconds();
		S += GustStrength * FMath::Sin(2.0f * UE_PI * GustFrequency * T);
	}
	return FMath::Max(0.0f, S);
}

void AWindActor::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	if (WindRotationRate != 0.0f)
	{
		FRotator Rot = WindDirection.Rotation();
		Rot.Yaw += WindRotationRate * DeltaTime;
		WindDirection = Rot.Vector();
	}
}
