#include "WakeRibbonComponent.h"
#include "WaterBodyComponent.h"
#include "Materials/MaterialInterface.h"

UWakeRibbonComponent::UWakeRibbonComponent(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	PrimaryComponentTick.bCanEverTick = false;   // drives fra pawnens Tick
	SetCollisionEnabled(ECollisionEnabled::NoCollision);
	SetCastShadow(false);
	bUseComplexAsSimpleCollision = false;
	SetUsingAbsoluteLocation(true);
	SetUsingAbsoluteRotation(true);
	SetUsingAbsoluteScale(true);
}

void UWakeRibbonComponent::InitRibbon(UMaterialInterface* Material)
{
	Samples.SetNum(WakeSamples);
	Head = -1;
	Count = 0;
	bHasLastSample = false;

	const int32 NumRows = WakeSamples + 1;   // + det levende akterpunktet
	Verts.SetNum(NumRows * 2);
	Normals.Init(FVector::UpVector, NumRows * 2);
	UVs.SetNum(NumRows * 2);
	Colors.Init(FLinearColor(1.0f, 1.0f, 1.0f, 0.0f), NumRows * 2);
	Tangents.Init(FProcMeshTangent(0.0f, 1.0f, 0.0f), NumRows * 2);
	for (int32 i = 0; i < NumRows; ++i)
	{
		Verts[i * 2] = Verts[i * 2 + 1] = FVector::ZeroVector;
		UVs[i * 2] = FVector2D(0.0f, 0.0f);
		UVs[i * 2 + 1] = FVector2D(1.0f, 0.0f);
	}

	TArray<int32> Tris;
	Tris.Reserve((NumRows - 1) * 6);
	for (int32 i = 0; i < NumRows - 1; ++i)
	{
		const int32 A = i * 2, B = i * 2 + 1, C = (i + 1) * 2, D = (i + 1) * 2 + 1;
		// Normalen skal peke opp (mot kameraet); rekkefølgen er ufarlig siden materialet er TwoSided.
		Tris.Append({ A, C, B, B, C, D });
	}
	CreateMeshSection_LinearColor(0, Verts, Tris, Normals, UVs, Colors, Tangents, /*bCreateCollision=*/false);
	if (Material)
	{
		SetMaterial(0, Material);
	}
	bInitialized = true;
}

const UWakeRibbonComponent::FWakeSample& UWakeRibbonComponent::SampleAt(int32 AgeIndex) const
{
	const int32 N = Samples.Num();
	return Samples[((Head - AgeIndex) % N + N) % N];
}

float UWakeRibbonComponent::SurfaceZ(const FVector& P, UWaterBodyComponent* Ocean, float Fallback) const
{
	if (Ocean)
	{
		const auto Q = Ocean->TryQueryWaterInfoClosestToWorldLocation(P,
			EWaterBodyQueryFlags::ComputeLocation | EWaterBodyQueryFlags::IncludeWaves);
		if (Q.HasValue())
		{
			return Q.GetValue().GetWaterSurfaceLocation().Z;
		}
	}
	return Fallback;
}

float UWakeRibbonComponent::GetOldestAge(float Time) const
{
	return Count > 0 ? Time - SampleAt(Count - 1).Time : 0.0f;
}

void UWakeRibbonComponent::UpdateRibbon(const FVector& SternPos, const FVector& Right, float Speed, float Time, UWaterBodyComponent* Ocean)
{
	if (!bInitialized)
	{
		return;
	}

	// Ny prøve for hver WakeSampleDistance tilbakelagt — uansett fart, så strimmelen aldri strekkes
	// vilkårlig langt fra siste prøve når båten driver sakte; alfaen tar seg av synligheten.
	if (!bHasLastSample || FVector::DistSquared2D(SternPos, LastSamplePos) >= FMath::Square(WakeSampleDistance))
	{
		Head = (Head + 1) % Samples.Num();
		Samples[Head] = { SternPos, Right, Time, Speed };
		Count = FMath::Min(Count + 1, Samples.Num());
		LastSamplePos = SternPos;
		bHasLastSample = true;
	}

	SetWorldLocation(SternPos);
	const float TanKelvin = FMath::Tan(FMath::DegreesToRadians(WakeKelvinAngleDeg));
	const float SpeedSpan = FMath::Max(1.0f, WakeFullSpeed - WakeSpeedThreshold);

	FWakeSample Live{ SternPos, Right, Time, Speed };
	float Along = 0.0f;
	FVector PrevPos = SternPos;
	const int32 NumRows = WakeSamples + 1;
	for (int32 Row = 0; Row < NumRows; ++Row)
	{
		// Rad 0 = levende akterpunkt, deretter prøvene fra nyeste til eldste; ubrukte rader kollapser
		// på den eldste prøven med alfa 0.
		const FWakeSample& S = Row == 0 ? Live : SampleAt(FMath::Min(Row - 1, FMath::Max(Count - 1, 0)));
		const bool bValid = Row == 0 || Row - 1 < Count;

		Along += FVector::Dist2D(PrevPos, S.Pos);
		PrevPos = S.Pos;
		const float HalfW = FMath::Min(WakeBaseHalfWidth + Along * TanKelvin, WakeMaxHalfWidth);
		const float Age = Time - S.Time;
		const float Alpha = bValid
			? FMath::Clamp(1.0f - Age / WakeLifetime, 0.0f, 1.0f)
				* FMath::Clamp((S.Speed - WakeSpeedThreshold) / SpeedSpan, 0.0f, 1.0f)
			: 0.0f;

		const float Z = SurfaceZ(S.Pos, Ocean, S.Pos.Z) + WakeZOffset;
		const FVector Center(S.Pos.X, S.Pos.Y, Z);
		Verts[Row * 2] = Center - S.Right * HalfW - SternPos;
		Verts[Row * 2 + 1] = Center + S.Right * HalfW - SternPos;
		const float V = Along / 200.0f;
		UVs[Row * 2].Y = V;
		UVs[Row * 2 + 1].Y = V;
		Colors[Row * 2].A = Alpha;
		Colors[Row * 2 + 1].A = Alpha;
		Tangents[Row * 2] = Tangents[Row * 2 + 1] = FProcMeshTangent(S.Right, false);
	}

	UpdateMeshSection_LinearColor(0, Verts, Normals, UVs, Colors, Tangents);
}
