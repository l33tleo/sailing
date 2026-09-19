#include "OceanWaterSetupActor.h"
#include "FjordWaveGenerator.h"
#include "WaterBodyComponent.h"
#include "WindActor.h"
#include "WaterZoneActor.h"
#include "WaterBodyOceanActor.h"
#include "WaterBodyOceanComponent.h"
#include "FjordOceanBodyActor.h"
#include "WaterSplineComponent.h"
#include "GerstnerWaterWaves.h"
#include "Kismet/GameplayStatics.h"
#include "Materials/MaterialInterface.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Components/PrimitiveComponent.h"
#include "UObject/UnrealType.h"
#include "HAL/IConsoleManager.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/Pawn.h"

// Testkrok for overtoningen (scripts/run_fullscreen.sh --boat-shot --arg -FjordBoatShotDelay=40
// --exec "sailing.WaveTestTurnDeg 90"): legger grader til bølgeretningen etter 10 s spilltid.
static TAutoConsoleVariable<float> CVarWaveTestTurnDeg(
	TEXT("sailing.WaveTestTurnDeg"), 0.0f,
	TEXT("Dreier bølgenes målretning så mange grader etter 10 s (test av overtoning). 0 = av."));

AOceanWaterSetupActor::AOceanWaterSetupActor()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.TickInterval = 0.5f; // Vind-sync trenger ikke full frame-rate.
}

void AOceanWaterSetupActor::BeginPlay()
{
	Super::BeginPlay();

	SpawnWaterZone();
	SpawnOceanBody();
	BuildAndAssignWaves();
	ApplyWaterLook();
}

void AOceanWaterSetupActor::ApplyWaterLook()
{
	UWaterBodyComponent* OceanComp = SpawnedOcean ? SpawnedOcean->GetWaterBodyComponent() : nullptr;
	if (!OceanComp)
	{
		return;
	}

	// Komponenten eier sin egen MID (WaterMID) med Water_Material_Ocean som forelder; den kan bli
	// gjenskapt av motoren ved vannkropp-oppdateringer, så dette kalles også fra vind-synken.
	if (UMaterialInstanceDynamic* WaterMID = OceanComp->GetWaterMaterialInstance())
	{
		WaterMID->SetVectorParameterValue(TEXT("Absorption"), WaterAbsorption);
		WaterMID->SetVectorParameterValue(TEXT("Scattering"), WaterScattering);
		WaterMID->SetScalarParameterValue(TEXT("Default Near Normal Strength"), NearDetailNormalStrength);
		WaterMID->SetScalarParameterValue(TEXT("Default Distant Normal Strength"), DistantDetailNormalStrength);
		WaterMID->SetScalarParameterValue(TEXT("Default Distant Normal StrengthB"), DistantDetailNormalStrength * 0.8f);
	}
}

void AOceanWaterSetupActor::SpawnWaterZone()
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

	SpawnedZone = World->SpawnActor<AWaterZone>(AWaterZone::StaticClass(),
		FVector(ZoneCenter.X, ZoneCenter.Y, WaterLevel), FRotator::ZeroRotator, Params);
	if (SpawnedZone)
	{
		SpawnedZone->SetZoneExtent(ZoneExtent);
		// Water info-teksturen (vannhøyde/-dybde per texel, brukt av havmaterialet) er 512² som
		// standard = ~47×78 m per texel over 24×40 km. r.Water.WaterInfo.RenderTargetResolutionMax i
		// DefaultEngine.ini kan bare klampe NED, så oppløsningen må heves her.
		SpawnedZone->SetRenderTargetResolution(FIntPoint(WaterInfoResolution, WaterInfoResolution));

		// Nivåets Landscape er kun et lite dekorativt bakteppe (fjordterrenget er egne
		// ProceduralMeshComponent-øyer, ikke Landscape-carvet). bAutoIncludeLandscapesAsTerrain
		// (privat felt, ingen offentlig setter) trigger ellers WaterEditor-modulens automatiske
		// "water brush"-tilknytning til Landscape ved aktør-spawn — noe som viste seg å kunne
		// henge PIE-økten (modal "Insert New Landscape Edit Layer"-dialog). Satt av via reflection
		// siden feltet ikke har en UFUNCTION-setter.
		if (FBoolProperty* Prop = FindFProperty<FBoolProperty>(AWaterZone::StaticClass(), TEXT("bAutoIncludeLandscapesAsTerrain")))
		{
			Prop->SetPropertyValue_InContainer(SpawnedZone, false);
		}

		// FORSØKT: bEnableLocalOnlyTessellation ("glidende vindu" rundt kameraet, ment å fikse et
		// flatt/teksturløst hav for en så stor sone) + en eksplisitt FarDistanceMeshExtent/
		// FarMeshMaterial (siden FarDistanceMeshExtent er 0/av som motor-standard, og uten en "far
		// mesh" forsvant fjernt vann helt). Begge deler REVERTERT: kombinasjonen ga et enda verre
		// resultat (vannet nesten usynlig, båten så ut til å sveve i himmelen — bekreftet med
		// skjermbilde) i stedet for å fikse det opprinnelige avstandsbaserte feilmatchet brukeren
		// rapporterte (nærvann ved øyer riktig, fjernvann feil). Dette krever dypere Water-plugin-
		// ekspertise (trolig interaktiv tuning i editoren) enn det som er trygt å gjette seg til
		// blindt via automatiserte PIE-sykluser. Sonen bruker dermed sin enkle, opprinnelige
		// helsone-tessellering igjen (flatt utseende, men uten avstands-mismatchen).
	}
}

void AOceanWaterSetupActor::SpawnOceanBody()
{
	UWorld* World = GetWorld();
	if (!World || !SpawnedZone)
	{
		return;
	}

	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

	// AFjordOceanBodyActor bruker UFjordOceanBodyComponent i stedet for standard
	// UWaterBodyOceanComponent, slik at CollisionExtents (protected + editor-only setter i
	// motoren) kan settes fra spillkode. Se FjordOceanBodyActor.h for hvorfor dette er nødvendig.
	SpawnedOcean = World->SpawnActor<AFjordOceanBodyActor>(AFjordOceanBodyActor::StaticClass(),
		FVector(ZoneCenter.X, ZoneCenter.Y, WaterLevel), FRotator::ZeroRotator, Params);
	if (!SpawnedOcean)
	{
		return;
	}

	UWaterBodyComponent* OceanComp = SpawnedOcean->GetWaterBodyComponent();
	if (OceanComp)
	{
		OceanComp->SetWaterZoneOverride(SpawnedZone);

		// KRITISK: havets faktiske kollisjonsboks (OceanBoxCollisionComponent, bygget i
		// UWaterBodyOceanComponent::OnUpdateBody) bruker CollisionExtents direkte i verdensrom,
		// UAVHENGIG av spline-form eller aktør-skalering (bekreftet ved å lese motorens kildekode:
		// "No matter the scale, OceanCollisionExtents is always specified in world-space"). Uten
		// dette forblir kollisjonsboksen fastlåst til motorens default (~500x500x200 m) forankret i
		// aktørens origo — milevis unna der båten faktisk seiler, og oppdriften får aldri en sjanse
		// (verifisert i PIE: båten falt fritt, ingen overlap oppdaget). Må settes FØR UpdateAll().
		if (UFjordOceanBodyComponent* FjordOceanComp = Cast<UFjordOceanBodyComponent>(OceanComp))
		{
			FjordOceanComp->SetRuntimeCollisionExtents(FVector(ZoneExtent.X * 0.5f, ZoneExtent.Y * 0.5f, 5000.0f));
			// Havets visuelle utstrekning = hele sonen (motorens default er kun 512x512 m).
			FjordOceanComp->SetRuntimeOceanExtents(ZoneExtent);
		}

		// VIKTIG: i AWaterBodyOcean beskriver splinen en ØY — vannet genereres UTENFOR splinen, ut
		// til OceanExtents (se UWaterBodyOceanComponent::GenerateWaterBodyMesh: "central island").
		// Et tidligere oppsett la splinen som et rektangel rundt HELE sonen, som dermed definerte
		// hele fjorden som "øy" uten vann (verifisert i PIE: WaterInfoMesh dekket kun ±256 m ved
		// sonesenteret, båten hang i løse lufta 14 km unna). Fjordens land er egne
		// ProceduralMeshComponent-masser, så splinen trenger ikke følge noen kyst: vi legger en
		// bitteliten (2x2 m) pliktøy helt i sonens hjørne, langt unna der man seiler.
		if (UWaterSplineComponent* Spline = OceanComp->GetWaterSpline())
		{
			Spline->ClearSplinePoints(false);
			const FVector Corner(ZoneCenter.X - ZoneExtent.X * 0.5f + 1000.0f, ZoneCenter.Y - ZoneExtent.Y * 0.5f + 1000.0f, WaterLevel);
			constexpr float R = 100.0f;
			Spline->AddSplinePoint(Corner + FVector(-R, -R, 0.0f), ESplineCoordinateSpace::World, false);
			Spline->AddSplinePoint(Corner + FVector( R, -R, 0.0f), ESplineCoordinateSpace::World, false);
			Spline->AddSplinePoint(Corner + FVector( R,  R, 0.0f), ESplineCoordinateSpace::World, false);
			Spline->AddSplinePoint(Corner + FVector(-R,  R, 0.0f), ESplineCoordinateSpace::World, false);
			for (int32 i = 0; i < Spline->GetNumberOfSplinePoints(); ++i)
			{
				Spline->SetSplinePointType(i, ESplinePointType::Linear, false);
			}
			Spline->SetClosedLoop(true, false);
			Spline->UpdateSpline();
		}

		// Bruker Water-pluginets ferdige Single Layer Water-havmateriale som utgangspunkt i
		// stedet for å måtte konstruere en ny material-graf (som krever en editor-tilkobling).
		// Fargetuning mot prosjektets DeepColor/MidColor/ShallowColor gjøres i en senere
		// polish-pass når materialets faktiske parameternavn kan inspiseres i editoren.
		// Prosjektkopi av pluginets havmateriale (/Game/Materials/Water/MI_FjordOcean -> M_FjordWater)
		// med én tilføyelse: en boksmaske på Opacity Mask (HullMaskPos/Fwd/HalfExtent) som klipper
		// bort vannflaten innenfor skrogets fotavtrykk. Single Layer Water vet ikke at båten
		// fortrenger vann, så uten masken tegnes vannflaten tvers gjennom cockpiten (gulvet ligger
		// under vannlinjen) og båten ser vannfylt ut. Parametrene settes av ASailboatPawn hver frame.
		// Faller tilbake til pluginets originale materiale hvis kopien mangler.
		UMaterialInterface* OceanMat = LoadObject<UMaterialInterface>(nullptr,
			TEXT("/Game/Materials/Water/MI_FjordOcean.MI_FjordOcean"));
		if (!OceanMat)
		{
			OceanMat = LoadObject<UMaterialInterface>(nullptr,
				TEXT("/Water/Materials/WaterSurface/Water_Material_Ocean.Water_Material_Ocean"));
		}
		if (OceanMat)
		{
			OceanComp->SetWaterMaterial(OceanMat);
		}
	}

	if (OceanComp)
	{
		FOnWaterBodyChangedParams ChangedParams;
		ChangedParams.bShapeOrPositionChanged = true;
		OceanComp->UpdateAll(ChangedParams);

		// Vannmeshen (quadtree-flisene) bygges fra havets Bounds. Bygges den før bounds er riktige,
		// tegnes vannflaten bare i en del av sonen resten av økten (se UFjordOceanBodyComponent-
		// konstruktøren). Oppdater bounds og tving én ny bygging av mesh + water info.
		OceanComp->UpdateBounds();
		SpawnedZone->MarkForRebuild(EWaterZoneRebuildFlags::All, this);

		const FBox2D OceanBox(FVector2D(OceanComp->Bounds.Origin - OceanComp->Bounds.BoxExtent),
			FVector2D(OceanComp->Bounds.Origin + OceanComp->Bounds.BoxExtent));
		const FBox2D ZoneBox = SpawnedZone->GetZoneBounds2D();
		const bool bCovers = OceanBox.ExpandBy(100.0).IsInside(ZoneBox);
		bOceanBoundsCoveredZoneAtSetup = bCovers;
		UE_LOG(LogTemp, Log, TEXT("[VANN] havbounds X %.0f..%.0f Y %.0f..%.0f | sone X %.0f..%.0f Y %.0f..%.0f | %s"),
			OceanBox.Min.X, OceanBox.Max.X, OceanBox.Min.Y, OceanBox.Max.Y,
			ZoneBox.Min.X, ZoneBox.Max.X, ZoneBox.Min.Y, ZoneBox.Max.Y,
			bCovers ? TEXT("dekker sonen") : TEXT("DEKKER IKKE SONEN — vannflaten mangler utenfor havbounds"));
		if (!bCovers)
		{
			UE_LOG(LogTemp, Warning, TEXT("[VANN] Havets bounds dekker ikke vannsonen."));
		}
	}
}

void AOceanWaterSetupActor::BuildAndAssignWaves()
{
	if (!SpawnedOcean)
	{
		return;
	}

	RuntimeWaves = NewObject<UGerstnerWaterWaves>(this, TEXT("FjordGerstnerWaves"));
	WaveGenerator = NewObject<UFjordWaveGenerator>(RuntimeWaves, TEXT("FjordWaveGenerator"));
	WaveGenerator->NumWaves = NumWaves;
	WaveGenerator->MinWavelength = MinWavelength;
	WaveGenerator->MaxWavelength = MaxWavelength;
	WaveGenerator->MinAmplitude = MinAmplitude;
	WaveGenerator->MaxAmplitude = MaxAmplitude;
	WaveGenerator->SmallWaveSteepness = SmallWaveSteepness;
	WaveGenerator->LargeWaveSteepness = LargeWaveSteepness;
	WaveGenerator->DirectionSpreadDeg = WaveDirectionSpreadDeg;
	WaveGenerator->Sets = { UFjordWaveGenerator::FWaveSet{ 0.0f, 1.0f } };   // retning settes ved første synk
	RuntimeWaves->GerstnerWaveGenerator = WaveGenerator;
	RuntimeWaves->RecomputeWaves(/*bAllowBPScript=*/false);

	SpawnedOcean->SetWaterWaves(RuntimeWaves);
}

void AOceanWaterSetupActor::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	TimeSinceWindSync += DeltaTime;
	if (TimeSinceWindSync >= WindSyncInterval)
	{
		TimeSinceWindSync = 0.0f;
		SyncWavesWithWind();
		ApplyWaterLook();
	}
	TickWaveCrossfade(DeltaTime);
}

void AOceanWaterSetupActor::SyncWavesWithWind()
{
	AWindActor* Wind = FindWind();
	if (!Wind || !RuntimeWaves || !WaveGenerator || WaveGenerator->Sets.Num() == 0)
	{
		return;
	}

	// Bølgene går MED vinden: AWindActor sine vektorer peker MOT vindkilden (se CLAUDE.md,
	// vindkonvensjon), og en Gerstner-bølge forplanter seg langs +Direction — så retningen er
	// −vindvektoren. Tidligere ble +vindvektoren brukt, og sjøen gikk mot vinden. Middelvinden
	// (uten de korte vriene) brukes: sjøen følger ikke hvert vindkast.
	const FVector WindDir = Wind->GetMeanWindDirection();
	float TargetAngleDeg = FMath::RadiansToDegrees(FMath::Atan2(-WindDir.Y, -WindDir.X));
	if (GetWorld()->GetTimeSeconds() > 10.0)
	{
		TargetAngleDeg = FMath::UnwindDegrees(TargetAngleDeg + CVarWaveTestTurnDeg.GetValueOnGameThread());
	}
	const float WindStrengthNorm = FMath::Clamp(Wind->GetWindStrength() / FMath::Max(1.0f, Wind->BaseWindStrength), 0.2f, 1.5f);

	const bool bFirst = AppliedWindStrengthNorm < 0.0f;
	bool bChanged = false;
	if (bFirst)
	{
		WaveGenerator->Sets = { UFjordWaveGenerator::FWaveSet{ TargetAngleDeg, 1.0f } };
		AppliedWindStrengthNorm = WindStrengthNorm;
		bChanged = true;
	}
	else
	{
		// Amplituden følger vindstyrken i små steg (amplitude alene endrer ikke fasen).
		const float Step = FMath::Clamp(WindStrengthNorm - AppliedWindStrengthNorm, -MaxAmplitudeStepPerSync, MaxAmplitudeStepPerSync);
		if (FMath::Abs(Step) >= 0.005f)
		{
			AppliedWindStrengthNorm += Step;
			bChanged = true;
		}

		// Ny retning: IKKE drei det eksisterende settet (det flytter hele sjøen), men ton inn et nytt
		// sett i vindretningen. Én overtoning om gangen; neste vurderes når denne er ferdig.
		const float CurrentAngleDeg = WaveGenerator->Sets.Last().AngleDeg;
		const float Delta = FMath::Abs(FMath::FindDeltaAngleDegrees(CurrentAngleDeg, TargetAngleDeg));
		if (!bWaveCrossfading && Delta >= WaveDirectionChangeDeg)
		{
			CrossfadeMaxJumpCm = 0.0f;
			UE_LOG(LogTemp, Log, TEXT("[BOLGER] Vinden har dreid %.0f° — toner inn ny bølgeretning %.0f° over %.0f s."),
				Delta, TargetAngleDeg, WaveCrossfadeSeconds);
			WaveGenerator->Sets = { WaveGenerator->Sets.Last(), UFjordWaveGenerator::FWaveSet{ TargetAngleDeg, 0.0f } };
			bWaveCrossfading = true;
			WaveCrossfadeAlpha = 0.0f;
			TimeSinceCrossfadeUpdate = 0.0f;
		}
	}

	if (bChanged)
	{
		WaveGenerator->AmplitudeScale = AppliedWindStrengthNorm * WindAmplitudeScale;
		RuntimeWaves->RecomputeWaves(/*bAllowBPScript=*/false);
	}
}

void AOceanWaterSetupActor::TickWaveCrossfade(float DeltaTime)
{
	if (!bWaveCrossfading || !WaveGenerator || !RuntimeWaves || WaveGenerator->Sets.Num() != 2)
	{
		return;
	}
	WaveCrossfadeAlpha = FMath::Min(1.0f, WaveCrossfadeAlpha + DeltaTime / WaveCrossfadeSeconds);
	TimeSinceCrossfadeUpdate += DeltaTime;
	if (TimeSinceCrossfadeUpdate < WaveCrossfadeUpdateInterval && WaveCrossfadeAlpha < 1.0f)
	{
		return;
	}
	TimeSinceCrossfadeUpdate = 0.0f;

	// Glatt S-kurve: ingen brå start/slutt i hvor fort sjøen skifter karakter.
	const float W = FMath::SmoothStep(0.0f, 1.0f, WaveCrossfadeAlpha);
	WaveGenerator->Sets[0].Weight = 1.0f - W;
	WaveGenerator->Sets[1].Weight = W;
	const bool bDone = WaveCrossfadeAlpha >= 1.0f;
	if (bDone)
	{
		// Det gamle settet har vekt 0 (null amplitude og krapphet) og fjernes usynlig.
		WaveGenerator->Sets = { WaveGenerator->Sets[1] };
		bWaveCrossfading = false;
	}

	// Mål hvor mye vannflaten ved båten flytter seg i SELVE oppdateringen (samme tidspunkt, før og
	// etter): det er «hoppet» spilleren ville sett. Ren overtoning skal gi millimeter per steg.
	const float Before = SampleWaterHeightAtPlayer();
	RuntimeWaves->RecomputeWaves(/*bAllowBPScript=*/false);
	const float After = SampleWaterHeightAtPlayer();
	if (Before > -1e5f && After > -1e5f)
	{
		CrossfadeMaxJumpCm = FMath::Max(CrossfadeMaxJumpCm, FMath::Abs(After - Before));
	}
	if (bDone)
	{
		UE_LOG(LogTemp, Log, TEXT("[BOLGER] Ny bølgeretning %.0f° ferdig tonet inn. Største sprang i vannflaten ved båten per steg: %.1f cm."),
			WaveGenerator->Sets[0].AngleDeg, CrossfadeMaxJumpCm);
		CrossfadeMaxJumpCm = 0.0f;
	}
}

float AOceanWaterSetupActor::SampleWaterHeightAtPlayer() const
{
	const APlayerController* PC = GetWorld() ? GetWorld()->GetFirstPlayerController() : nullptr;
	const APawn* Pawn = PC ? PC->GetPawn() : nullptr;
	UWaterBodyComponent* Ocean = SpawnedOcean ? SpawnedOcean->GetWaterBodyComponent() : nullptr;
	if (!Pawn || !Ocean)
	{
		return -1e6f;
	}
	const auto Q = Ocean->TryQueryWaterInfoClosestToWorldLocation(Pawn->GetActorLocation(),
		EWaterBodyQueryFlags::ComputeLocation | EWaterBodyQueryFlags::IncludeWaves);
	return Q.HasValue() ? static_cast<float>(Q.GetValue().GetWaterSurfaceLocation().Z) : -1e6f;
}

AWindActor* AOceanWaterSetupActor::FindWind()
{
	if (CachedWind.IsValid())
	{
		return CachedWind.Get();
	}

	TArray<AActor*> Found;
	UGameplayStatics::GetAllActorsOfClass(GetWorld(), AWindActor::StaticClass(), Found);
	if (Found.Num() > 0)
	{
		CachedWind = Cast<AWindActor>(Found[0]);
		return CachedWind.Get();
	}
	return nullptr;
}
