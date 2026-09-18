#include "SailRigComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "HAL/IConsoleManager.h"

// Testkroker for skjermbildeverifikasjon (scripts/run_fullscreen.sh --boat-shot --exec "sailing.BoomTestDeg 60").
// Sentinel -999 = av (bomvinkelen kan være negativ, så -1 duger ikke).
static TAutoConsoleVariable<float> CVarBoomTestDeg(
	TEXT("sailing.BoomTestDeg"), -999.0f,
	TEXT("Tvinger bomvinkelen (grader, + = styrbord). -999 = av."));

static TAutoConsoleVariable<float> CVarFlutterTest(
	TEXT("sailing.FlutterTest"), -1.0f,
	TEXT("Tvinger seilets flagring (0..1). -1 = av."));

USailRigComponent::USailRigComponent()
{
	PrimaryComponentTick.bCanEverTick = false;   // drives eksplisitt fra pawnens Tick (etter vindberegningen)
}

void USailRigComponent::AddSheetInput(float Axis)
{
	if (!FMath::IsNearlyZero(Axis))
	{
		PendingSheetInput = Axis;
		bAutoTrim = false;
	}
}

void USailRigComponent::ToggleAutoTrim()
{
	bAutoTrim = !bAutoTrim;
}

void USailRigComponent::UpdateRig(float ApparentWindAngleDeg, float ApparentWindStr, float DeltaTime)
{
	const float AbsAWA = FMath::Abs(ApparentWindAngleDeg);
	const float WindSideSign = ApparentWindAngleDeg >= 0.0f ? 1.0f : -1.0f;

	// Skjøtegrensen auto-trimmen ville valgt: seilet står OptimalAoADeg innenfor vindretningen.
	const float SheetAuto = FMath::Clamp(AbsAWA - OptimalAoADeg, MinSheetDeg, MaxSheetDeg);
	if (bAutoTrim)
	{
		SheetLimitDeg = SheetAuto;
	}
	else
	{
		SheetLimitDeg = FMath::Clamp(SheetLimitDeg + PendingSheetInput * SheetRateDegPerS * DeltaTime, MinSheetDeg, MaxSheetDeg);
	}
	PendingSheetInput = 0.0f;

	// Vinden skyver bommen ut til seilet står i vindretningen — eller til skjøten stopper den.
	const float BoomMag = FMath::Min(AbsAWA, SheetLimitDeg);
	const float BoomTarget = -WindSideSign * BoomMag;   // le side
	AngleOfAttackDeg = AbsAWA - BoomMag;

	// Jibb: målet byttet side mens bommen sto tydelig ute og vinden kommer aktenfra — seilet slår
	// over med JibeRateDegPerS til det har passert senterlinjen.
	const bool bTargetOtherSide = (BoomTarget * BoomAngleDeg) < 0.0f && FMath::Abs(BoomAngleDeg) > 1.0f;
	if (bTargetOtherSide && AbsAWA > 90.0f)
	{
		bJibing = true;
	}
	if (bJibing && (BoomTarget * BoomAngleDeg) >= 0.0f)
	{
		bJibing = false;
	}
	const float Rate = bJibing ? JibeRateDegPerS : BoomRateDegPerS;
	BoomAngleDeg = FMath::FInterpConstantTo(BoomAngleDeg, BoomTarget, DeltaTime, Rate);

	// Trim-effektivitet: null når seilet flagrer (for slakk / i jern), redusert når det er halt for hardt.
	const float LuffFactor = FMath::SmoothStep(LuffAoADeg, OptimalAoADeg, AngleOfAttackDeg);
	const float OverSheet = FMath::Clamp((SheetAuto - BoomMag) / OverSheetRangeDeg, 0.0f, 1.0f);
	const float OverFactor = 1.0f - OverSheetPenalty * OverSheet;
	TrimEfficiency = LuffFactor * OverFactor;

	// Shader-parametre.
	const float FlutterWindFactor = FMath::Clamp(ApparentWindStr / FlutterWindRef, 0.0f, 1.0f);
	Flutter = (1.0f - FMath::SmoothStep(LuffAoADeg, LuffAoADeg + 8.0f, AngleOfAttackDeg)) * FlutterWindFactor;
	SailFill = FMath::Clamp(ApparentWindStr / FillWindRef, 0.0f, 1.0f)
		* FMath::SmoothStep(0.0f, OptimalAoADeg, AngleOfAttackDeg)
		* (1.0f - 0.6f * Flutter);
	if (FMath::Abs(BoomAngleDeg) > 0.5f)
	{
		SailSide = BoomAngleDeg > 0.0f ? 1.0f : -1.0f;
	}

	// Testkroker.
	const float BoomTest = CVarBoomTestDeg.GetValueOnGameThread();
	if (BoomTest > -998.0f)
	{
		BoomAngleDeg = FMath::Clamp(BoomTest, -MaxSheetDeg, MaxSheetDeg);
		SailSide = BoomAngleDeg >= 0.0f ? 1.0f : -1.0f;
		SailFill = 1.0f;
	}
	const float FlutterTest = CVarFlutterTest.GetValueOnGameThread();
	if (FlutterTest >= 0.0f)
	{
		Flutter = FMath::Clamp(FlutterTest, 0.0f, 1.0f);
	}
}

void USailRigComponent::ApplyToMesh()
{
	// Bommen peker -X. Yaw +θ tar -X mot -Y (babord), så positiv bomvinkel (styrbord) er negativ yaw.
	// Verifisert med --boat-shot --exec "sailing.BoomTestDeg 50": bommen står ut til styrbord.
	SetRelativeRotation(FRotator(0.0f, -BoomAngleDeg, 0.0f));

	if (SailMID)
	{
		SailMID->SetScalarParameterValue(TEXT("SailFill"), SailFill);
		SailMID->SetScalarParameterValue(TEXT("SailSide"), SailSide);
		SailMID->SetScalarParameterValue(TEXT("Flutter"), Flutter);
	}
}

void USailRigComponent::InitSailMaterial()
{
	SailMID = nullptr;
	if (!RigMesh || !RigMesh->GetStaticMesh())
	{
		return;
	}
	const int32 Slot = RigMesh->GetMaterialIndex(TEXT("M_Sail"));
	if (Slot == INDEX_NONE)
	{
		UE_LOG(LogTemp, Warning, TEXT("[RIGG] RigMesh %s mangler materialslot M_Sail — ingen seilparametre."),
			*RigMesh->GetStaticMesh()->GetName());
		return;
	}
	SailMID = RigMesh->CreateAndSetMaterialInstanceDynamic(Slot);
}
