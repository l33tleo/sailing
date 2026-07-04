#include "SailboatPawn.h"
#include "SailingPlayerController.h"
#include "WindActor.h"
#include "Components/SceneComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Materials/MaterialInterface.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "GameFramework/SpringArmComponent.h"
#include "Camera/CameraComponent.h"
#include "EnhancedInputComponent.h"
#include "Kismet/GameplayStatics.h"

ASailboatPawn::ASailboatPawn()
{
	PrimaryActorTick.bCanEverTick = true;

	// Felles rot slik at kapsel, båt og kamera flyttes sammen
	RootScene = CreateDefaultSubobject<USceneComponent>(TEXT("RootScene"));
	RootComponent = RootScene;

	CapsuleComp = CreateDefaultSubobject<UCapsuleComponent>(TEXT("Capsule"));
	CapsuleComp->InitCapsuleSize(100.0f, 50.0f);
	CapsuleComp->SetCollisionProfileName(TEXT("Pawn"));
	CapsuleComp->SetupAttachment(RootScene);

	// Kombinert Optimist-båt (alle deler i ett mesh fra Blender)
	BoatMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("BoatMesh"));
	BoatMesh->SetupAttachment(RootScene);
	BoatMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	BoatMesh->SetCastShadow(true);

	// Plan bak i båten (stern) – ugjennomtrengelig, så man ikke ser «inn» bakfra
	SternShield = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("SternShield"));
	SternShield->SetupAttachment(RootScene);
	SternShield->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	SternShield->SetCastShadow(true);
	// Posisjon og skala settes i BeginPlay ut fra båt-mesh bounds (måling)
	SternShield->SetRelativeRotation(FRotator(90.0f, 0.0f, 0.0f));   // Plan i YZ, normal -X (mot kamera bakfra)

	// Målpunkt over båten: kamera ser mot dette punktet, så båten havner lavere i bildet
	CameraTarget = CreateDefaultSubobject<USceneComponent>(TEXT("CameraTarget"));
	CameraTarget->SetupAttachment(RootScene);
	CameraTarget->SetRelativeLocation(FVector(0.0f, 0.0f, 120.0f));

	// Spring arm for tredjepersonskamera (arm og pitch slik at hele masten er synlig som standard)
	SpringArm = CreateDefaultSubobject<USpringArmComponent>(TEXT("SpringArm"));
	SpringArm->SetupAttachment(CameraTarget);
	SpringArm->TargetArmLength = 650.0f;
	SpringArm->SetRelativeRotation(FRotator(-6.0f, 0.0f, 0.0f));
	SpringArm->bUsePawnControlRotation = false;
	SpringArm->bDoCollisionTest = false;

	// Kamera
	Camera = CreateDefaultSubobject<UCameraComponent>(TEXT("Camera"));
	Camera->SetupAttachment(SpringArm);

	// Skum/spray-pool (instanced mesh). Mesh/materiale settes i InitSpray ved BeginPlay.
	SprayMesh = CreateDefaultSubobject<UInstancedStaticMeshComponent>(TEXT("SprayMesh"));
	SprayMesh->SetupAttachment(RootScene);
	SprayMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	SprayMesh->SetCastShadow(false);
	SprayMesh->SetMobility(EComponentMobility::Movable);
}

void ASailboatPawn::BeginPlay()
{
	Super::BeginPlay();

	// Last kombinert Optimist-mesh (ny modell fra Blender)
	UStaticMesh* BoatCombinedMesh = LoadObject<UStaticMesh>(nullptr,
		TEXT("/Game/ModelsV2/Optimist3735.Optimist3735"));

	if (BoatCombinedMesh && BoatCombinedMesh->GetNumLODs() > 0 && BoatMesh)
	{
		UE_LOG(LogTemp, Log, TEXT("SailboatPawn: Bruker kombinert Optimist-mesh fra ModelsV2: %s"), *BoatCombinedMesh->GetPathName());
		BoatMesh->SetStaticMesh(BoatCombinedMesh);
		BoatMesh->SetVisibility(true);
		BoatMesh->SetHiddenInGame(false);
		BoatMesh->SetRelativeLocation(FVector(0.0f, 0.0f, BoatVisualZOffset));
		BoatMesh->SetRelativeRotation(FRotator::ZeroRotator);
		BoatMesh->SetRelativeScale3D(FVector(1.0f, 1.0f, 1.0f));
		// FBX-materialer fra Blender brukes direkte (M_Hull, M_Sail, M_Wood, M_Board)
	}
	else
	{
		UE_LOG(LogTemp, Error, TEXT("SailboatPawn: Kunne ikke laste /Game/ModelsV2/Optimist3735!"));
	}

	// SternShield fjernet: bakdelen dekkes nå av båt-meshet fra Blender (ror, rorkult, skrog).
	// Skjul den gamle «dekkingen» (flat plan) så den ikke brukes.
	if (SternShield)
	{
		SternShield->SetVisibility(false);
		SternShield->SetHiddenInGame(true);
	}
}

void ASailboatPawn::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);

	if (UEnhancedInputComponent* EIC = Cast<UEnhancedInputComponent>(PlayerInputComponent))
	{
		if (TurnAction)
		{
			EIC->BindAction(TurnAction, ETriggerEvent::Triggered, this, &ASailboatPawn::HandleTurn);
		}
		if (CameraAction)
		{
			EIC->BindAction(CameraAction, ETriggerEvent::Triggered, this, &ASailboatPawn::HandleCamera);
		}
	}
}

void ASailboatPawn::HandleTurn(const FInputActionValue& Value)
{
	TurnInput = Value.Get<float>();
}

void ASailboatPawn::HandleCamera(const FInputActionValue& Value)
{
	FVector2D Delta = Value.Get<FVector2D>();
	CameraYawInput = Delta.X;
	CameraPitchInput = Delta.Y;
}

void ASailboatPawn::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	// Stopp bevegelse når fullskjerm-kart er åpent
	if (ASailingPlayerController* PC = Cast<ASailingPlayerController>(GetController()))
	{
		if (PC->IsMapViewShown())
		{
			TurnInput = 0.0f;
			return;
		}
	}

	// 1. Sving (yaw)
	FRotator CurrentRot = GetActorRotation();
	CurrentRot.Yaw += TurnInput * TurnSpeed * DeltaTime;
	SetActorRotation(CurrentRot);
	TurnInput = 0.0f;

	// 2. Tilsynelatende vind (apparent wind) og polar-kurve
	FVector Forward = GetActorForwardVector();
	AWindActor* Wind = FindWind();
	if (Wind)
	{
		// Lokal vind ved båtens posisjon: fanger kast-flekker og vri der båten faktisk er.
		FVector TrueWindVec = Wind->GetWindVelocityAt(GetActorLocation());
		FVector BoatVelocity = Forward * CurrentSpeed;
		FVector ApparentWindVec = TrueWindVec - BoatVelocity;
		float ApparentWindStr = ApparentWindVec.Size();

		FVector WindDir;
		float WindStr;
		if (ApparentWindStr < 1.0f)
		{
			WindDir = Wind->GetWindDirection();
			WindStr = 0.0f;
		}
		else
		{
			WindDir = ApparentWindVec.GetSafeNormal();
			WindStr = ApparentWindStr;
		}

		float CosAngle = FVector::DotProduct(Forward, WindDir);
		float AngleToWind = FMath::RadiansToDegrees(FMath::Acos(FMath::Clamp(CosAngle, -1.0f, 1.0f)));

		float ForceMultiplier = 0.0f;
		if (AngleToWind < NoGoZoneAngle)
			ForceMultiplier = 0.0f;
		else if (AngleToWind < 90.0f)
		{
			float T = (AngleToWind - NoGoZoneAngle) / (90.0f - NoGoZoneAngle);
			ForceMultiplier = FMath::Lerp(CloseHauledForce, BeamReachForce, T);
		}
		else if (AngleToWind < 135.0f)
		{
			float T = (AngleToWind - 90.0f) / 45.0f;
			ForceMultiplier = FMath::Lerp(BeamReachForce, BroadReachForce, T);
		}
		else
		{
			float T = (AngleToWind - 135.0f) / 45.0f;
			ForceMultiplier = FMath::Lerp(BroadReachForce, RunningForce, T);
		}

		CurrentSailForce = WindStr * FMath::Pow(ForceMultiplier, PolarSharpness);
	}
	else
	{
		CurrentSailForce = 0.0f;
	}

	// 3. Hullmotstand og integrert fart
	float Drag = DragCoefficient * FMath::Square(CurrentSpeed);
	float Acceleration = CurrentSailForce - Drag;
	float NewSpeed = FMath::Clamp(CurrentSpeed + Acceleration * DeltaTime, 0.0f, MaxBoatSpeed);
	FVector Movement = Forward * NewSpeed * DeltaTime;
	AddActorWorldOffset(Movement, false);
	CurrentSpeed = NewSpeed;

	// 4. Lås til vannoverflate med enkel sinus-heave
	FVector Loc = GetActorLocation();
	float Time = GetWorld()->GetTimeSeconds();
	Loc.Z = WaterZ + FMath::Sin(Time * WaveFrequency * 2.0f * PI) * WaveAmplitude;
	SetActorLocation(Loc, false);

	// 4b. Skum/spray
	if (bEnableSpray)
	{
		UpdateSpray(DeltaTime, Forward, Time);
	}

	// 5. Kamera-orbit
	if (SpringArm)
	{
		FRotator ArmRot = SpringArm->GetRelativeRotation();
		ArmRot.Yaw += CameraYawInput * 2.0f;
		ArmRot.Pitch = FMath::Clamp(ArmRot.Pitch + CameraPitchInput * 2.0f, -50.0f, 15.0f);
		SpringArm->SetRelativeRotation(ArmRot);
	}
	CameraYawInput = 0.0f;
	CameraPitchInput = 0.0f;
}

AWindActor* ASailboatPawn::FindWind() const
{
	if (CachedWind.IsValid())
	{
		return CachedWind.Get();
	}

	TArray<AActor*> Found;
	UGameplayStatics::GetAllActorsOfClass(GetWorld(), AWindActor::StaticClass(), Found);
	if (Found.Num() > 0)
	{
		const_cast<ASailboatPawn*>(this)->CachedWind = Cast<AWindActor>(Found[0]);
		return CachedWind.Get();
	}
	return nullptr;
}

void ASailboatPawn::InitSpray()
{
	if (bSprayInitialized || !SprayMesh)
	{
		return;
	}
	bSprayInitialized = true;

	// Små kuler i stedet for flate plan (unngår «papirlapp»-utseendet). Ingen egne assets nødvendig.
	UStaticMesh* DropMesh = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Sphere.Sphere"));
	if (DropMesh)
	{
		SprayMesh->SetStaticMesh(DropMesh);
	}
	UMaterialInterface* BaseMat = LoadObject<UMaterialInterface>(nullptr,
		TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial"));
	if (BaseMat)
	{
		// Dynamisk instans for å tinte skummet hvitt (param-navn ignoreres hvis det ikke finnes).
		UMaterialInstanceDynamic* FoamMID = UMaterialInstanceDynamic::Create(BaseMat, this);
		if (FoamMID)
		{
			FLinearColor Foam(0.92f, 0.96f, 1.0f, 1.0f);
			FoamMID->SetVectorParameterValue(TEXT("Color"), Foam);
			FoamMID->SetVectorParameterValue(TEXT("BaseColor"), Foam);
			SprayMesh->SetMaterial(0, FoamMID);
		}
		else
		{
			SprayMesh->SetMaterial(0, BaseMat);
		}
	}

	// Forhåndsallokér hele poolen som usynlige (null-skala) instanser.
	SprayParticles.SetNum(FMath::Max(8, SprayPoolSize));
	SprayMesh->ClearInstances();
	FTransform Hidden(FRotator::ZeroRotator, FVector::ZeroVector, FVector::ZeroVector);
	for (int32 i = 0; i < SprayParticles.Num(); ++i)
	{
		SprayMesh->AddInstance(Hidden, /*bWorldSpace=*/true);
	}
}

void ASailboatPawn::EmitSprayParticle(const FVector& Pos, const FVector& Vel, float SizeScale)
{
	// Finn en ledig partikkel i poolen.
	for (FSprayParticle& P : SprayParticles)
	{
		if (P.Life <= 0.0f)
		{
			P.Position = Pos;
			P.Velocity = Vel;
			P.MaxLife = SprayParticleLife * FMath::FRandRange(0.7f, 1.2f);
			P.Life = P.MaxLife;
			P.Size = SprayParticleSize * SizeScale * FMath::FRandRange(0.7f, 1.3f);
			P.Yaw = FMath::FRandRange(0.0f, 360.0f);
			return;
		}
	}
}

void ASailboatPawn::UpdateSpray(float DeltaTime, const FVector& Forward, float Time)
{
	InitSpray();
	if (!SprayMesh || SprayParticles.Num() == 0)
	{
		return;
	}

	const FVector BoatLoc = GetActorLocation();
	AWindActor* Wind = FindWind();

	// --- Emisjon: baug-skum når farten er høy ---
	if (CurrentSpeed > SpraySpeedThreshold)
	{
		float SpeedFrac = (CurrentSpeed - SpraySpeedThreshold) / FMath::Max(1.0f, MaxBoatSpeed - SpraySpeedThreshold);
		float Rate = 45.0f * FMath::Clamp(SpeedFrac, 0.0f, 1.0f); // partikler/sekund
		SprayEmitAccumulator += Rate * DeltaTime;

		FVector Bow = BoatLoc + Forward * 130.0f;
		FVector Right = FVector::CrossProduct(FVector::UpVector, Forward).GetSafeNormal();

		while (SprayEmitAccumulator >= 1.0f)
		{
			SprayEmitAccumulator -= 1.0f;
			float Side = FMath::FRandRange(-1.0f, 1.0f);
			FVector SpawnPos = Bow + Right * (Side * 70.0f);
			// Sprut: bakover + utover til siden + opp, skalert med fart.
			FVector V = -Forward * (CurrentSpeed * 0.35f)
				+ Right * (Side * CurrentSpeed * 0.25f)
				+ FVector(0.0f, 0.0f, FMath::FRandRange(180.0f, 320.0f) * (0.5f + SpeedFrac));
			EmitSprayParticle(SpawnPos, V, 0.8f + SpeedFrac * 0.6f);
		}
	}

	// --- Emisjon: vinddrift i sterke kast rundt båten ---
	if (Wind)
	{
		float Gust = Wind->GetGustFactorAt(BoatLoc);
		if (Gust > 0.25f && FMath::FRand() < Gust * 0.6f)
		{
			FVector WindDir = Wind->GetWindDirection();
			FVector Offset(FMath::FRandRange(-600.0f, 600.0f), FMath::FRandRange(-600.0f, 600.0f), 0.0f);
			FVector SpawnPos = BoatLoc + Offset;
			SpawnPos.Z = WaterZ;
			// Driver nedvinds (vekk fra kilden) + litt opp.
			FVector V = -WindDir * (300.0f + Gust * 400.0f) + FVector(0.0f, 0.0f, FMath::FRandRange(60.0f, 160.0f));
			EmitSprayParticle(SpawnPos, V, 0.6f + Gust * 0.5f);
		}
	}

	// --- Simulering + skriv instans-transformer ---
	for (int32 i = 0; i < SprayParticles.Num(); ++i)
	{
		FSprayParticle& P = SprayParticles[i];
		FTransform Xform;
		if (P.Life > 0.0f)
		{
			P.Velocity.Z -= SprayGravity * DeltaTime;
			P.Position += P.Velocity * DeltaTime;
			P.Life -= DeltaTime;

			// Krymp mot slutten av levetiden som «fade».
			float LifeFrac = FMath::Clamp(P.Life / FMath::Max(0.01f, P.MaxLife), 0.0f, 1.0f);
			float Scale = P.Size * FMath::Sin(LifeFrac * PI); // opp og ned: dukker opp og forsvinner

			if (P.Position.Z < WaterZ)
			{
				P.Life = 0.0f; // landet på vannet → resirkuler
			}
			Xform = FTransform(FRotator(0.0f, P.Yaw, 0.0f), P.Position, FVector(Scale));
		}
		else
		{
			Xform = FTransform(FRotator::ZeroRotator, FVector::ZeroVector, FVector::ZeroVector);
		}
		SprayMesh->UpdateInstanceTransform(i, Xform, /*bWorldSpace=*/true, /*bMarkRenderStateDirty=*/false, /*bTeleport=*/true);
	}
	SprayMesh->MarkRenderStateDirty();
}
