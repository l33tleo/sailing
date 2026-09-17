#include "OceanWaterSetupActor.h"
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
	}
}

void AOceanWaterSetupActor::BuildAndAssignWaves()
{
	if (!SpawnedOcean)
	{
		return;
	}

	RuntimeWaves = NewObject<UGerstnerWaterWaves>(this, TEXT("FjordGerstnerWaves"));
	UGerstnerWaterWaveGeneratorSimple* Generator = NewObject<UGerstnerWaterWaveGeneratorSimple>(RuntimeWaves, TEXT("FjordGerstnerWaveGenerator"));
	Generator->NumWaves = NumWaves;
	Generator->MinWavelength = MinWavelength;
	Generator->MaxWavelength = MaxWavelength;
	Generator->MinAmplitude = MinAmplitude;
	Generator->MaxAmplitude = MaxAmplitude;
	Generator->SmallWaveSteepness = SmallWaveSteepness;
	Generator->LargeWaveSteepness = LargeWaveSteepness;
	RuntimeWaves->GerstnerWaveGenerator = Generator;
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
}

void AOceanWaterSetupActor::SyncWavesWithWind()
{
	AWindActor* Wind = FindWind();
	if (!Wind || !RuntimeWaves)
	{
		return;
	}

	UGerstnerWaterWaveGeneratorSimple* Generator = Cast<UGerstnerWaterWaveGeneratorSimple>(RuntimeWaves->GerstnerWaveGenerator);
	if (!Generator)
	{
		return;
	}

	// Oppdater havets dominerende bølgeretning og -styrke fra global vind. Kastenes lokale
	// "cat's paw"-mørkning (tidligere GustDarkening) er en materialeffekt, ikke en asset-endring,
	// og migreres separat siden Gerstner-asset-nivået kun støtter global retning/styrke.
	const FVector WindDir = Wind->GetWindDirection();
	const float NewAngleDeg = FMath::RadiansToDegrees(FMath::Atan2(WindDir.Y, WindDir.X));
	const float WindStrengthNorm = FMath::Clamp(Wind->GetWindStrength() / FMath::Max(1.0f, Wind->BaseWindStrength), 0.2f, 1.5f);

	// VIKTIG: bølgefrontenes fase er forankret i verdens origo. En ny WindAngleDeg flytter derfor
	// hele bølgemønsteret flere meter der båten seiler (~2 km fra origo): sjøen HOPPER momentant,
	// og spilleren opplever det som at båten «hopper litt tilbake». Målt i fullskjerm 2026-09-17:
	// 157 sprang på 20–65 cm i vannflaten under båten på 41 min (~4/min) med den gamle terskelen
	// (15° / 0.25 styrke) — spilltråden var ellers helt jevn (60 fps, ingen posisjonshopp).
	// Derfor: retningen settes én gang (ev. på nytt først ved WaveDirectionResyncDeg), mens
	// amplituden — som IKKE endrer fase (samme Seed/retning/bølgelengder) — følger vinden i små steg.
	const bool bFirst = AppliedWindStrengthNorm < 0.0f;
	const float AngleDelta = FMath::Abs(FMath::FindDeltaAngleDegrees(AppliedWindAngleDeg, NewAngleDeg));
	const bool bResyncDirection = bFirst || (WaveDirectionResyncDeg > 0.0f && AngleDelta >= WaveDirectionResyncDeg);

	float NewStrengthNorm = WindStrengthNorm;
	if (!bFirst)
	{
		const float Step = FMath::Clamp(WindStrengthNorm - AppliedWindStrengthNorm, -MaxAmplitudeStepPerSync, MaxAmplitudeStepPerSync);
		NewStrengthNorm = AppliedWindStrengthNorm + Step;
		if (!bResyncDirection && FMath::Abs(Step) < 0.005f)
		{
			return;
		}
	}

	if (bResyncDirection)
	{
		if (!bFirst)
		{
			UE_LOG(LogTemp, Warning, TEXT("[BOLGER] Bølgeretning synket på nytt (%.0f° -> %.0f°) — sjøen hopper én gang."),
				AppliedWindAngleDeg, NewAngleDeg);
		}
		AppliedWindAngleDeg = NewAngleDeg;
		Generator->WindAngleDeg = NewAngleDeg;
	}
	AppliedWindStrengthNorm = NewStrengthNorm;

	Generator->MinAmplitude = MinAmplitude * NewStrengthNorm * WindAmplitudeScale;
	Generator->MaxAmplitude = MaxAmplitude * NewStrengthNorm * WindAmplitudeScale;

	RuntimeWaves->RecomputeWaves(/*bAllowBPScript=*/false);
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
