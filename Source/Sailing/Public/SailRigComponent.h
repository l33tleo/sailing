#pragma once

#include "CoreMinimal.h"
#include "Components/SceneComponent.h"
#include "SailRigComponent.generated.h"

class UStaticMeshComponent;
class UMaterialInstanceDynamic;

/**
 * Riggen på Optimisten: bom + sprit + seil som én stiv enhet som svinger om masten.
 *
 * Komponenten ER mastepivoten: den plasseres på mastaksen i bomhøyde under BoatMesh, dens relative
 * yaw er bomvinkelen, og RigMesh (bom/sprit/seil, eksportert med pivot i samme punkt) henger under
 * den med identitetstransform.
 *
 * Modell (enkel og riktig for en jolle): skjøten begrenser bare hvor langt UT bommen kan gå. Vinden
 * skyver bommen ut til seilet står i vindens retning (angrepsvinkel 0, seilet flagrer) med mindre
 * skjøten stopper den først:
 *   BoomMag    = min(|AWA|, SheetLimitDeg)
 *   AoA        = |AWA| - BoomMag           (>0: seilet trekker, ~0: flagrer)
 * Auto-trim setter skjøtegrensen slik at AoA blir OptimalAoADeg; manuell skjøt (hal inn/slakk)
 * overstyrer og gir mindre kraft ved feil trim (TrimEfficiency, som pawnen ganger seilkraften med —
 * lik 1 under auto-trim, så dagens seilfølelse er uendret).
 *
 * Konvensjoner (UE: X fram, +Y styrbord): AWA er signert, positiv = vind fra styrbord. BoomAngleDeg
 * er signert, 0 = bommen akterover langs senterlinjen, positiv = bomnokk mot styrbord (= le side når
 * vinden kommer fra babord).
 */
UCLASS(ClassGroup = (Sailing), meta = (BlueprintSpawnableComponent))
class SAILING_API USailRigComponent : public USceneComponent
{
	GENERATED_BODY()

public:
	USailRigComponent();

	/** Bom/sprit/seil-meshet. Settes av pawnen (opprettes som default-subobject der, festet til denne). */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UStaticMeshComponent> RigMesh;

	// --- Tuning ---

	/** Angrepsvinkel (grader mellom seil og tilsynelatende vind) auto-trimmen sikter mot. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sailing|Rig", meta = (ClampMin = "5", ClampMax = "45"))
	float OptimalAoADeg = 20.0f;

	/** Under denne angrepsvinkelen flagrer seilet og gir ingen kraft. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sailing|Rig", meta = (ClampMin = "0", ClampMax = "20"))
	float LuffAoADeg = 4.0f;

	/** Minste skjøtegrense (bommen kan aldri hales helt inn til senterlinjen). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sailing|Rig", meta = (ClampMin = "0", ClampMax = "45"))
	float MinSheetDeg = 5.0f;

	/** Største skjøtegrense (bommen mot vantet/skroget på lens). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sailing|Rig", meta = (ClampMin = "45", ClampMax = "90"))
	float MaxSheetDeg = 85.0f;

	/** Hvor fort spilleren haler inn / slakker (grader/s) når skjøtetasten holdes. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sailing|Rig", meta = (ClampMin = "1"))
	float SheetRateDegPerS = 30.0f;

	/** Bommens svinghastighet mot målvinkelen (grader/s). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sailing|Rig", meta = (ClampMin = "1"))
	float BoomRateDegPerS = 120.0f;

	/** Bommens svinghastighet under jibb (vinden tar seilet over på lens). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sailing|Rig", meta = (ClampMin = "1"))
	float JibeRateDegPerS = 360.0f;

	/** Krafttap (0..1) når bommen er halt OverSheetRangeDeg lenger inn enn ideelt. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sailing|Rig", meta = (ClampMin = "0", ClampMax = "1"))
	float OverSheetPenalty = 0.6f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sailing|Rig", meta = (ClampMin = "1"))
	float OverSheetRangeDeg = 30.0f;

	/** Tilsynelatende vindstyrke (uu/s) der seilet er fullt utspent (SailFill = 1). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sailing|Rig", meta = (ClampMin = "1"))
	float FillWindRef = 900.0f;

	/** Tilsynelatende vindstyrke (uu/s) der flagringen har full amplitude. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sailing|Rig", meta = (ClampMin = "1"))
	float FlutterWindRef = 400.0f;

	/** Auto-trim: skjøtegrensen følger vinden. Slås av ved manuell skjøt, på igjen med AutoTrim-tasten. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sailing|Rig")
	bool bAutoTrim = true;

	/** Mastaksen i bomhøyde, i BoatMesh-lokale cm: mast på x=75, bom 168 cm under mastetoppen (z=66.5,
	 *  CR 3.5.2.13). Må stemme med MAST_PIVOT i scripts/blender/export_optimist_parts.py. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sailing|Rig")
	FVector MastPivotLocal = FVector(75.0f, 0.0f, 66.5f);

	// --- Tilstand ---

	/** Skjøtegrense: hvor langt ut (grader fra senterlinjen) bommen får gå. */
	UPROPERTY(BlueprintReadOnly, Category = "Sailing|Rig|State")
	float SheetLimitDeg = 45.0f;

	/** Bomvinkel, signert (se klassekommentar). */
	UPROPERTY(BlueprintReadOnly, Category = "Sailing|Rig|State")
	float BoomAngleDeg = 0.0f;

	UPROPERTY(BlueprintReadOnly, Category = "Sailing|Rig|State")
	float AngleOfAttackDeg = 0.0f;

	/** 0..1, ganges inn i seilkraften av pawnen. 1 under auto-trim i stasjonær tilstand. */
	UPROPERTY(BlueprintReadOnly, Category = "Sailing|Rig|State")
	float TrimEfficiency = 1.0f;

	/** 0..1: hvor overhalt seilet er (1 = OverSheetRangeDeg eller mer innenfor ideell trim). HUD-varsel. */
	UPROPERTY(BlueprintReadOnly, Category = "Sailing|Rig|State")
	float OverSheetAmount = 0.0f;

	/** 0..1 flagring (shader-parameter). */
	UPROPERTY(BlueprintReadOnly, Category = "Sailing|Rig|State")
	float Flutter = 0.0f;

	/** 0..1 seilfylde/bukt (shader-parameter). */
	UPROPERTY(BlueprintReadOnly, Category = "Sailing|Rig|State")
	float SailFill = 0.0f;

	/** -1/+1: siden bukten er på (= bommens side). Beholder forrige verdi når bommen står midtskips. */
	UPROPERTY(BlueprintReadOnly, Category = "Sailing|Rig|State")
	float SailSide = 1.0f;

	UPROPERTY(BlueprintReadOnly, Category = "Sailing|Rig|State")
	bool bJibing = false;

	/** Oppdater bom/skjøt/trim fra tilsynelatende vind (signert vinkel og styrke). Kalles av pawnen hver Tick. */
	void UpdateRig(float ApparentWindAngleDeg, float ApparentWindStr, float DeltaTime);

	/** Manuell skjøt: +1 = slakk, -1 = hal inn (akkumuleres til neste UpdateRig). Slår av auto-trim. */
	void AddSheetInput(float Axis);

	void ToggleAutoTrim();

	/** Setter riggens yaw fra BoomAngleDeg og seilets materialparametre (SailFill/SailSide/Flutter). */
	void ApplyToMesh();

	/** Dynamisk instans av seilmaterialet (opprettes i InitSailMaterial når RigMesh har et mesh). */
	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> SailMID;

	/** Lager SailMID på RigMesh-sloten som heter M_Sail (no-op hvis RigMesh mangler mesh/slot). */
	void InitSailMaterial();

private:
	float PendingSheetInput = 0.0f;
};
