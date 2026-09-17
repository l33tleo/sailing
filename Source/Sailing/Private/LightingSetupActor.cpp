#include "LightingSetupActor.h"
#include "Engine/PostProcessVolume.h"
#include "Engine/DirectionalLight.h"
#include "Components/DirectionalLightComponent.h"
#include "Components/SkyAtmosphereComponent.h"
#include "Components/VolumetricCloudComponent.h"
#include "Kismet/GameplayStatics.h"

ALightingSetupActor::ALightingSetupActor()
{
	PrimaryActorTick.bCanEverTick = false;
}

void ALightingSetupActor::BeginPlay()
{
	Super::BeginPlay();

	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	// --- PostProcessVolume (mangler helt i nivået i dag) ---
	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	if (APostProcessVolume* PPV = World->SpawnActor<APostProcessVolume>(APostProcessVolume::StaticClass(),
		FVector::ZeroVector, FRotator::ZeroRotator, Params))
	{
		PPV->bUnbound = true;
		PPV->Priority = 0.0f;
		PPV->BlendWeight = 1.0f;

		FPostProcessSettings& S = PPV->Settings;

		S.bOverride_AutoExposureBias = true;
		S.AutoExposureBias = AutoExposureBias;

		S.bOverride_BloomIntensity = true;
		S.BloomIntensity = BloomIntensity;

		S.bOverride_VignetteIntensity = true;
		S.VignetteIntensity = VignetteIntensity;

		S.bOverride_ColorSaturation = true;
		S.ColorSaturation = FVector4(ColorSaturation, ColorSaturation, ColorSaturation, 1.0f);
	}

	// --- Juster eksisterende DirectionalLight (allerede plassert i MainOcean.umap) ---
	TArray<AActor*> Lights;
	UGameplayStatics::GetAllActorsOfClass(World, ADirectionalLight::StaticClass(), Lights);
	if (Lights.Num() > 0)
	{
		if (ADirectionalLight* Sun = Cast<ADirectionalLight>(Lights[0]))
		{
			if (UDirectionalLightComponent* LightComp = Sun->FindComponentByClass<UDirectionalLightComponent>())
			{
				LightComp->SetIntensity(SunIntensityLux);
				LightComp->bUseTemperature = true;
				LightComp->SetTemperature(SunTemperatureK);
			}

			FRotator SunRot = Sun->GetActorRotation();
			SunRot.Pitch = -SunElevationDeg; // negativ pitch = sollys peker nedover mot bakken
			Sun->SetActorRotation(SunRot);
		}
	}

	// --- Juster eksisterende SkyAtmosphere (allerede plassert i MainOcean.umap) ---
	TArray<AActor*> Atmospheres;
	UGameplayStatics::GetAllActorsOfClass(World, ASkyAtmosphere::StaticClass(), Atmospheres);
	if (Atmospheres.Num() > 0)
	{
		if (ASkyAtmosphere* Atmosphere = Cast<ASkyAtmosphere>(Atmospheres[0]))
		{
			if (USkyAtmosphereComponent* AtmosphereComp = Atmosphere->FindComponentByClass<USkyAtmosphereComponent>())
			{
				AtmosphereComp->SetAerialPespectiveViewDistanceScale(AerialPerspectiveViewDistanceScale);
			}
		}
	}

	// --- Ytelse: VolumetricCloud ray-march (målt med ProfileGPU i PIE: CloudView 20 ms -> 3,4 ms,
	// 27 -> 46 fps ved skala 1.0 -> 0.25). Kun sample-antall/skyggedistanse røres — skyenes
	// utseende (tetthet/lag/materiale) tunes fortsatt visuelt i editoren. ---
	TArray<AActor*> Clouds;
	UGameplayStatics::GetAllActorsOfClass(World, AVolumetricCloud::StaticClass(), Clouds);
	for (AActor* CloudActor : Clouds)
	{
		if (UVolumetricCloudComponent* CloudComp = CloudActor->FindComponentByClass<UVolumetricCloudComponent>())
		{
			CloudComp->SetViewSampleCountScale(CloudViewSampleCountScale);
			CloudComp->SetShadowViewSampleCountScale(CloudViewSampleCountScale);
			CloudComp->SetShadowTracingDistance(CloudShadowTracingDistanceKm);
		}
	}
}
