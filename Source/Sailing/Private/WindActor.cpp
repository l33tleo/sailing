#include "WindActor.h"
#include "Math/UnrealMathUtility.h"

AWindActor::AWindActor()
{
	// Tick driver segment-systemet (hovedretning som dreier sakte mot nye mål).
	PrimaryActorTick.bCanEverTick = true;
}

void AWindActor::BeginPlay()
{
	Super::BeginPlay();

	RandStream.Initialize(WindSeed);

	// Start på den konfigurerte retningen, så velg første segment.
	CurrentBaseYaw = WindDirection.GetSafeNormal().Rotation().Yaw;
	TargetBaseYaw = CurrentBaseYaw;
	StartNewSegment();
}

void AWindActor::StartNewSegment()
{
	// Tilfeldig: blir dette segmentet ustabilt?
	bool bUnstable = RandStream.FRand() < UnstableChance;
	CurrentInstability = bUnstable
		? RandStream.FRandRange(0.5f, 1.0f)
		: RandStream.FRandRange(0.0f, 0.2f);

	// Ny målretning: et tilfeldig hopp til venstre eller høyre.
	float Magnitude = RandStream.FRandRange(DirectionChangeMinDeg, DirectionChangeMaxDeg);
	float Sign = (RandStream.FRand() < 0.5f) ? -1.0f : 1.0f;
	TargetBaseYaw = FMath::UnwindDegrees(CurrentBaseYaw + Sign * Magnitude);

	// Varighet: kortere når ustabilt → skifter oftere.
	float Interval = RandStream.FRandRange(MeanChangeIntervalMin, MeanChangeIntervalMax);
	if (bUnstable)
	{
		Interval *= UnstableIntervalScale;
	}
	SegmentTimeRemaining = Interval;
}

void AWindActor::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	// Tell ned mot neste hovedskifte.
	SegmentTimeRemaining -= DeltaTime;
	if (SegmentTimeRemaining <= 0.0f)
	{
		StartNewSegment();
	}

	// Drei hovedretningen sakte mot målet (korteste vei).
	float Delta = FMath::FindDeltaAngleDegrees(CurrentBaseYaw, TargetBaseYaw);
	float Step = TransitionSpeedDeg * DeltaTime;
	if (FMath::Abs(Delta) <= Step)
	{
		CurrentBaseYaw = TargetBaseYaw;
	}
	else
	{
		CurrentBaseYaw = FMath::UnwindDegrees(CurrentBaseYaw + FMath::Sign(Delta) * Step);
	}
}

float AWindActor::SeedOffset(float Channel) const
{
	return WindSeed * 13.137f + Channel * 101.7f;
}

float AWindActor::GetWindShiftDeg() const
{
	float T = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0f;
	float Deg = 0.0f;

	// Ustabilitet forsterker vuggingen.
	float Boost = 1.0f + CurrentInstability * UnstableOscillationBoost;

	if (ShiftAmplitudeDeg > 0.0f)
	{
		Deg += ShiftAmplitudeDeg * Boost * FMath::PerlinNoise1D(T * ShiftTimeScale + SeedOffset(1.0f));
	}
	if (JitterAmplitudeDeg > 0.0f)
	{
		Deg += JitterAmplitudeDeg * Boost * FMath::PerlinNoise1D(T * JitterTimeScale + SeedOffset(2.0f));
	}

	return Deg;
}

FVector AWindActor::GetMeanWindDirection() const
{
	return FRotator(0.0f, CurrentBaseYaw, 0.0f).Vector();
}

FVector AWindActor::GetWindDirection() const
{
	float Yaw = CurrentBaseYaw + GetWindShiftDeg();
	return FRotator(0.0f, Yaw, 0.0f).Vector();
}

float AWindActor::GetWindStrength() const
{
	float S = BaseWindStrength;

	if (StrengthNoiseAmplitude > 0.0f && GetWorld())
	{
		float T = GetWorld()->GetTimeSeconds();
		float N = FMath::PerlinNoise1D(T * StrengthNoiseTimeScale + SeedOffset(3.0f)); // [-1,1]
		S += StrengthNoiseAmplitude * N;
	}

	return FMath::Max(0.0f, S);
}

float AWindActor::GetGustFactorAt(const FVector& WorldPos) const
{
	if (GustAmplitude <= 0.0f || !GetWorld())
	{
		return 0.0f;
	}

	float T = GetWorld()->GetTimeSeconds();

	// Flekkene driver nedvinds (fra vindkilden og forbi spilleren). WindDir peker mot kilden,
	// så +WindDir*tid lar et fast mønster-trekk forflytte seg i flytretningen over tid.
	FVector WindDir = GetWindDirection();
	FVector Advected = WorldPos + WindDir * (GustTravelSpeed * T);

	// 3D-støy: to romlige akser + tid (feltet former/oppløser seg sakte).
	FVector Coord(
		Advected.X * GustSpatialScale + SeedOffset(4.0f),
		Advected.Y * GustSpatialScale + SeedOffset(5.0f),
		T * GustTimeScale + SeedOffset(6.0f));

	float N = FMath::PerlinNoise3D(Coord); // ~[-1,1]

	// Bare topper over terskel teller som kast → diskrete flekker i stedet for konstant variasjon.
	float G = (N - GustThreshold) / FMath::Max(0.05f, 1.0f - GustThreshold);
	return FMath::Clamp(G, 0.0f, 1.0f);
}

FVector AWindActor::GetWindVelocityAt(const FVector& WorldPos) const
{
	FVector Dir = GetWindDirection();
	float Strength = GetWindStrength();
	float Gust = GetGustFactorAt(WorldPos);
	Strength *= (1.0f + GustAmplitude * Gust);
	return Dir * Strength;
}
