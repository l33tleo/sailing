#include "SailboatPawn.h"
#include "WindActor.h"
#include "Components/SceneComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/StaticMeshComponent.h"
#include "GameFramework/SpringArmComponent.h"
#include "Camera/CameraComponent.h"
#include "EnhancedInputComponent.h"
#include "Kismet/GameplayStatics.h"
#include "UObject/ConstructorHelpers.h"

ASailboatPawn::ASailboatPawn()
{
	PrimaryActorTick.bCanEverTick = true;

	// Felles rot slik at kapsel, båt og kamera flyttes sammen (fikser «kjører fra båten»)
	RootScene = CreateDefaultSubobject<USceneComponent>(TEXT("RootScene"));
	RootComponent = RootScene;

	CapsuleComp = CreateDefaultSubobject<UCapsuleComponent>(TEXT("Capsule"));
	CapsuleComp->InitCapsuleSize(100.0f, 50.0f);
	CapsuleComp->SetCollisionProfileName(TEXT("Pawn"));
	CapsuleComp->SetupAttachment(RootScene);

	// Laser-båt fra Blender (LaserBoat.fbx, combine_meshes=false). Prøv begge navnekonvensjoner.
	static ConstructorHelpers::FObjectFinder<UStaticMesh> LaserHull(
		TEXT("/Game/ModelsV2/LaserHull"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> LaserHullAlt(
		TEXT("/Game/Models/LaserHull"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> LaserHullAlt2(
		TEXT("/Game/Models/LaserBoat_LaserHull"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> LaserMast(
		TEXT("/Game/ModelsV2/LaserMast"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> LaserMastAlt(
		TEXT("/Game/Models/LaserMast"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> LaserMastAlt2(
		TEXT("/Game/Models/LaserBoat_LaserMast"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> LaserBoom(
		TEXT("/Game/ModelsV2/LaserBoom"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> LaserBoomAlt(
		TEXT("/Game/Models/LaserBoom"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> LaserBoomAlt2(
		TEXT("/Game/Models/LaserBoat_LaserBoom"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> LaserSail(
		TEXT("/Game/ModelsV2/LaserSail"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> LaserSailAlt(
		TEXT("/Game/Models/LaserSail"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> LaserSailAlt2(
		TEXT("/Game/Models/LaserBoat_LaserSail"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> LaserRudder(
		TEXT("/Game/ModelsV2/LaserRudder"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> LaserRudderAlt(
		TEXT("/Game/Models/LaserRudder"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> LaserRudderAlt2(
		TEXT("/Game/Models/LaserBoat_LaserRudder"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> LaserTiller(
		TEXT("/Game/ModelsV2/LaserTiller"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> LaserTillerAlt(
		TEXT("/Game/Models/LaserTiller"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> LaserTillerAlt2(
		TEXT("/Game/Models/LaserBoat_LaserTiller"));

	auto GetLaserHull = [&]() { return LaserHull.Succeeded() ? LaserHull.Object : (LaserHullAlt.Succeeded() ? LaserHullAlt.Object : LaserHullAlt2.Object); };
	auto GetLaserMast = [&]() { return LaserMast.Succeeded() ? LaserMast.Object : (LaserMastAlt.Succeeded() ? LaserMastAlt.Object : LaserMastAlt2.Object); };
	auto GetLaserBoom = [&]() { return LaserBoom.Succeeded() ? LaserBoom.Object : (LaserBoomAlt.Succeeded() ? LaserBoomAlt.Object : LaserBoomAlt2.Object); };
	auto GetLaserSail = [&]() { return LaserSail.Succeeded() ? LaserSail.Object : (LaserSailAlt.Succeeded() ? LaserSailAlt.Object : LaserSailAlt2.Object); };
	auto GetLaserRudder = [&]() { return LaserRudder.Succeeded() ? LaserRudder.Object : (LaserRudderAlt.Succeeded() ? LaserRudderAlt.Object : LaserRudderAlt2.Object); };
	auto GetLaserTiller = [&]() { return LaserTiller.Succeeded() ? LaserTiller.Object : (LaserTillerAlt.Succeeded() ? LaserTillerAlt.Object : LaserTillerAlt2.Object); };
	bool bHasLaser = (LaserHull.Succeeded() || LaserHullAlt.Succeeded() || LaserHullAlt2.Succeeded()) &&
		(LaserMast.Succeeded() || LaserMastAlt.Succeeded() || LaserMastAlt2.Succeeded()) &&
		(LaserBoom.Succeeded() || LaserBoomAlt.Succeeded() || LaserBoomAlt2.Succeeded()) &&
		(LaserSail.Succeeded() || LaserSailAlt.Succeeded() || LaserSailAlt2.Succeeded()) &&
		(LaserRudder.Succeeded() || LaserRudderAlt.Succeeded() || LaserRudderAlt2.Succeeded()) &&
		(LaserTiller.Succeeded() || LaserTillerAlt.Succeeded() || LaserTillerAlt2.Succeeded());

	// Hull (skrog) – attach til RootScene så båten flytter seg med spilleren
	BoatMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("BoatMesh"));
	BoatMesh->SetupAttachment(RootScene);
	if (bHasLaser)
	{
		BoatMesh->SetStaticMesh(GetLaserHull());
		BoatMesh->SetRelativeScale3D(FVector(1.0f, 1.0f, 1.0f));
		BoatMesh->SetRelativeRotation(FRotator(0.0f, 0.0f, 0.0f));
	}
	BoatMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	// Do not hide BoatMesh in constructor; BeginPlay enforces final runtime mesh selection.

	// Bow brukes ikke for Laser
	BowMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("BowMesh"));
	BowMesh->SetupAttachment(BoatMesh);
	BowMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	BowMesh->SetVisibility(false);
	BowMesh->SetHiddenInGame(true);

	// Mast
	MastMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("MastMesh"));
	MastMesh->SetupAttachment(BoatMesh);
	MastMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	// Boom
	BoomMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("BoomMesh"));
	BoomMesh->SetupAttachment(BoatMesh);
	BoomMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	// Main sail
	MainSailMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("MainSailMesh"));
	MainSailMesh->SetupAttachment(BoatMesh);
	MainSailMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	// Sprit
	SpritMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("SpritMesh"));
	SpritMesh->SetupAttachment(BoatMesh);
	SpritMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	// Daggerboard (keel)
	KeelMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("KeelMesh"));
	KeelMesh->SetupAttachment(BoatMesh);
	KeelMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	// Rudder
	RudderMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("RudderMesh"));
	RudderMesh->SetupAttachment(BoatMesh);
	RudderMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	// Tiller
	TillerMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("TillerMesh"));
	TillerMesh->SetupAttachment(BoatMesh);
	TillerMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	// Sail number
	SailNumberMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("SailNumberMesh"));
	SailNumberMesh->SetupAttachment(BoatMesh);
	SailNumberMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	if (bHasLaser)
	{
		MastMesh->SetStaticMesh(GetLaserMast());
		BoomMesh->SetStaticMesh(GetLaserBoom());
		MainSailMesh->SetStaticMesh(GetLaserSail());
		RudderMesh->SetStaticMesh(GetLaserRudder());
		TillerMesh->SetStaticMesh(GetLaserTiller());
		// Laser-meshene er eksportert med riktig relative plassering. Unngå dobbel offset.
		MastMesh->SetRelativeLocation(FVector::ZeroVector);
		MastMesh->SetRelativeScale3D(FVector(1.0f, 1.0f, 1.0f));
		MastMesh->SetRelativeRotation(FRotator::ZeroRotator);
		BoomMesh->SetRelativeLocation(FVector::ZeroVector);
		BoomMesh->SetRelativeScale3D(FVector(1.0f, 1.0f, 1.0f));
		BoomMesh->SetRelativeRotation(FRotator::ZeroRotator);
		MainSailMesh->SetRelativeLocation(FVector::ZeroVector);
		MainSailMesh->SetRelativeScale3D(FVector(1.0f, 1.0f, 1.0f));
		MainSailMesh->SetRelativeRotation(FRotator::ZeroRotator);
		RudderMesh->SetRelativeLocation(FVector::ZeroVector);
		RudderMesh->SetRelativeScale3D(FVector(1.0f, 1.0f, 1.0f));
		RudderMesh->SetRelativeRotation(FRotator::ZeroRotator);
		TillerMesh->SetRelativeLocation(FVector::ZeroVector);
		TillerMesh->SetRelativeScale3D(FVector(1.0f, 1.0f, 1.0f));
		TillerMesh->SetRelativeRotation(FRotator::ZeroRotator);
		KeelMesh->SetVisibility(false);
		KeelMesh->SetHiddenInGame(true);
		SailNumberMesh->SetVisibility(false);
		SailNumberMesh->SetHiddenInGame(true);
		SpritMesh->SetVisibility(false);
		SpritMesh->SetHiddenInGame(true);
	}
	else
	{
		MastMesh->SetVisibility(false);
		BoomMesh->SetVisibility(false);
		MainSailMesh->SetVisibility(false);
		RudderMesh->SetVisibility(false);
		TillerMesh->SetVisibility(false);
		KeelMesh->SetVisibility(false);
		SailNumberMesh->SetVisibility(false);
		SpritMesh->SetVisibility(false);

		MastMesh->SetHiddenInGame(true);
		BoomMesh->SetHiddenInGame(true);
		MainSailMesh->SetHiddenInGame(true);
		RudderMesh->SetHiddenInGame(true);
		TillerMesh->SetHiddenInGame(true);
		KeelMesh->SetHiddenInGame(true);
		SailNumberMesh->SetHiddenInGame(true);
		SpritMesh->SetHiddenInGame(true);
	}

	// Spring arm for third-person camera
	SpringArm = CreateDefaultSubobject<USpringArmComponent>(TEXT("SpringArm"));
	SpringArm->SetupAttachment(RootScene);
	SpringArm->TargetArmLength = 500.0f;
	SpringArm->SetRelativeRotation(FRotator(-12.0f, 0.0f, 0.0f));
	SpringArm->bUsePawnControlRotation = false;
	SpringArm->bDoCollisionTest = false;

	// Camera
	Camera = CreateDefaultSubobject<UCameraComponent>(TEXT("Camera"));
	Camera->SetupAttachment(SpringArm);
}

void ASailboatPawn::BeginPlay()
{
	Super::BeginPlay();

	// Preferred runtime path: one combined ILCA mesh.
	UStaticMesh* LaserCombinedMesh = LoadObject<UStaticMesh>(nullptr, TEXT("/Game/ModelsV2/LaserBoatCombined.LaserBoatCombined"));
	const bool bValidCombined = LaserCombinedMesh && (LaserCombinedMesh->GetNumLODs() > 0);
	if (bValidCombined && BoatMesh)
	{
		UE_LOG(LogTemp, Log, TEXT("SailboatPawn: Using combined mesh /Game/ModelsV2/LaserBoatCombined"));
		BoatMesh->SetStaticMesh(LaserCombinedMesh);
		BoatMesh->SetVisibility(true);
		BoatMesh->SetHiddenInGame(false);
		BoatMesh->SetRelativeLocation(FVector(0.0f, 0.0f, BoatVisualZOffset));
		BoatMesh->SetRelativeRotation(FRotator::ZeroRotator);
		BoatMesh->SetRelativeScale3D(FVector(1.0f, 1.0f, 1.0f));

		auto HideComp = [](UStaticMeshComponent* C)
		{
			if (!C) return;
			C->SetVisibility(false);
			C->SetHiddenInGame(true);
		};
		HideComp(BowMesh);
		HideComp(MastMesh);
		HideComp(BoomMesh);
		HideComp(MainSailMesh);
		HideComp(RudderMesh);
		HideComp(TillerMesh);
		HideComp(SpritMesh);
		HideComp(KeelMesh);
		HideComp(SailNumberMesh);

		UMaterialInterface* BoatMat = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/Materials/M_Boat.M_Boat"));
		UMaterialInterface* SailMat = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/Models/M_Sail.M_Sail"));
		if (BoatMat)
		{
			BoatMesh->SetMaterial(0, BoatMat);
		}
		if (SailMat && BoatMesh->GetNumMaterials() > 1)
		{
			// Keep sail readable on the combined mesh.
			BoatMesh->SetMaterial(1, SailMat);
		}
		return;
	}
	if (LaserCombinedMesh && !bValidCombined)
	{
		UE_LOG(LogTemp, Warning, TEXT("SailboatPawn: LaserBoatCombined exists but is invalid (no LODs). Falling back to separated Laser meshes."));
	}

	// Force Laser mesh setup at runtime to override any stale Blueprint/instance overrides.
	UStaticMesh* LaserHullMesh = LoadObject<UStaticMesh>(nullptr, TEXT("/Game/ModelsV2/LaserHull.LaserHull"));
	UStaticMesh* LaserMastMesh = LoadObject<UStaticMesh>(nullptr, TEXT("/Game/ModelsV2/LaserMast.LaserMast"));
	UStaticMesh* LaserBoomMesh = LoadObject<UStaticMesh>(nullptr, TEXT("/Game/ModelsV2/LaserBoom.LaserBoom"));
	UStaticMesh* LaserSailMesh = LoadObject<UStaticMesh>(nullptr, TEXT("/Game/ModelsV2/LaserSail.LaserSail"));
	UStaticMesh* LaserRudderMesh = LoadObject<UStaticMesh>(nullptr, TEXT("/Game/ModelsV2/LaserRudder.LaserRudder"));
	UStaticMesh* LaserTillerMesh = LoadObject<UStaticMesh>(nullptr, TEXT("/Game/ModelsV2/LaserTiller.LaserTiller"));

	if (!LaserHullMesh) LaserHullMesh = LoadObject<UStaticMesh>(nullptr, TEXT("/Game/Models/LaserHull.LaserHull"));
	if (!LaserMastMesh) LaserMastMesh = LoadObject<UStaticMesh>(nullptr, TEXT("/Game/Models/LaserMast.LaserMast"));
	if (!LaserBoomMesh) LaserBoomMesh = LoadObject<UStaticMesh>(nullptr, TEXT("/Game/Models/LaserBoom.LaserBoom"));
	if (!LaserSailMesh) LaserSailMesh = LoadObject<UStaticMesh>(nullptr, TEXT("/Game/Models/LaserSail.LaserSail"));
	if (!LaserRudderMesh) LaserRudderMesh = LoadObject<UStaticMesh>(nullptr, TEXT("/Game/Models/LaserRudder.LaserRudder"));
	if (!LaserTillerMesh) LaserTillerMesh = LoadObject<UStaticMesh>(nullptr, TEXT("/Game/Models/LaserTiller.LaserTiller"));

	if (!LaserHullMesh) LaserHullMesh = LoadObject<UStaticMesh>(nullptr, TEXT("/Game/Models/LaserBoat_LaserHull.LaserBoat_LaserHull"));
	if (!LaserMastMesh) LaserMastMesh = LoadObject<UStaticMesh>(nullptr, TEXT("/Game/Models/LaserBoat_LaserMast.LaserBoat_LaserMast"));
	if (!LaserBoomMesh) LaserBoomMesh = LoadObject<UStaticMesh>(nullptr, TEXT("/Game/Models/LaserBoat_LaserBoom.LaserBoat_LaserBoom"));
	if (!LaserSailMesh) LaserSailMesh = LoadObject<UStaticMesh>(nullptr, TEXT("/Game/Models/LaserBoat_LaserSail.LaserBoat_LaserSail"));
	if (!LaserRudderMesh) LaserRudderMesh = LoadObject<UStaticMesh>(nullptr, TEXT("/Game/Models/LaserBoat_LaserRudder.LaserBoat_LaserRudder"));
	if (!LaserTillerMesh) LaserTillerMesh = LoadObject<UStaticMesh>(nullptr, TEXT("/Game/Models/LaserBoat_LaserTiller.LaserBoat_LaserTiller"));

	const bool bHasLaser = LaserHullMesh && LaserMastMesh && LaserBoomMesh && LaserSailMesh && LaserRudderMesh && LaserTillerMesh;
	if (!bHasLaser)
	{
		UE_LOG(LogTemp, Warning, TEXT("SailboatPawn: Laser meshes missing (checked ModelsV2/Models/LaserBoat_*)."));
		return;
	}
	UE_LOG(LogTemp, Log, TEXT("SailboatPawn: Using separated Laser meshes (ModelsV2/Models)."));

	auto ApplyLaserMesh = [](UStaticMeshComponent* Comp, UStaticMesh* Mesh)
	{
		if (!Comp || !Mesh) return;
		Comp->SetStaticMesh(Mesh);
		Comp->SetVisibility(true);
		Comp->SetHiddenInGame(false);
		Comp->SetRelativeScale3D(FVector(1.0f, 1.0f, 1.0f));
	};

	ApplyLaserMesh(BoatMesh, LaserHullMesh);
	ApplyLaserMesh(MastMesh, LaserMastMesh);
	ApplyLaserMesh(BoomMesh, LaserBoomMesh);
	ApplyLaserMesh(MainSailMesh, LaserSailMesh);
	ApplyLaserMesh(RudderMesh, LaserRudderMesh);
	ApplyLaserMesh(TillerMesh, LaserTillerMesh);

	// Relative transforms matching Blender-authored Laser layout (cm).
	if (BoatMesh)
	{
		BoatMesh->SetRelativeLocation(FVector(0.0f, 0.0f, BoatVisualZOffset));
		BoatMesh->SetRelativeRotation(FRotator::ZeroRotator);
	}
	if (MastMesh)
	{
		MastMesh->SetRelativeLocation(FVector(120.0f, 0.0f, 25.0f));
		MastMesh->SetRelativeRotation(FRotator::ZeroRotator);
	}
	if (BoomMesh)
	{
		BoomMesh->SetRelativeLocation(FVector(120.0f, 0.0f, 280.0f));
		BoomMesh->SetRelativeRotation(FRotator::ZeroRotator);
	}
	if (MainSailMesh)
	{
		MainSailMesh->SetRelativeLocation(FVector(120.0f, 37.0f, 313.0f));
		MainSailMesh->SetRelativeRotation(FRotator::ZeroRotator);
		// Slightly larger so the sail is clearly visible from gameplay camera.
		MainSailMesh->SetRelativeScale3D(FVector(1.35f, 1.35f, 1.35f));
	}
	if (RudderMesh)
	{
		RudderMesh->SetRelativeLocation(FVector(-200.0f, 0.0f, -20.0f));
		RudderMesh->SetRelativeRotation(FRotator::ZeroRotator);
	}
	if (TillerMesh)
	{
		TillerMesh->SetRelativeLocation(FVector(-160.0f, 35.0f, 15.0f));
		TillerMesh->SetRelativeRotation(FRotator::ZeroRotator);
	}

	if (BowMesh) { BowMesh->SetVisibility(false); BowMesh->SetHiddenInGame(true); }
	if (SpritMesh) { SpritMesh->SetVisibility(false); SpritMesh->SetHiddenInGame(true); }
	if (KeelMesh) { KeelMesh->SetVisibility(false); KeelMesh->SetHiddenInGame(true); }
	if (SailNumberMesh) { SailNumberMesh->SetVisibility(false); SailNumberMesh->SetHiddenInGame(true); }

	UMaterialInterface* BoatMat = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/Materials/M_Boat.M_Boat"));
	// M_Sail can be semi-transparent depending on import pipeline; force a visible material.
	UMaterialInterface* SailMat = LoadObject<UMaterialInterface>(nullptr, TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial"));
	if (BoatMat)
	{
		if (BoatMesh) BoatMesh->SetMaterial(0, BoatMat);
		if (MastMesh) MastMesh->SetMaterial(0, BoatMat);
		if (BoomMesh) BoomMesh->SetMaterial(0, BoatMat);
		if (RudderMesh) RudderMesh->SetMaterial(0, BoatMat);
		if (TillerMesh) TillerMesh->SetMaterial(0, BoatMat);
	}
	if (SailMat && MainSailMesh)
	{
		MainSailMesh->SetMaterial(0, SailMat);
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

	// 1. Apply turn (yaw)
	FRotator CurrentRot = GetActorRotation();
	CurrentRot.Yaw += TurnInput * TurnSpeed * DeltaTime;
	SetActorRotation(CurrentRot);
	TurnInput = 0.0f;

	// 2. Tilsynelatende vind (apparent wind) og polar-kurve
	// V_apparent = V_true_wind - V_boat. Seilet drives av tilsynelatende vind.
	FVector Forward = GetActorForwardVector();
	AWindActor* Wind = FindWind();
	if (Wind)
	{
		FVector TrueWindVec = Wind->GetWindDirection() * Wind->GetWindStrength();
		FVector BoatVelocity = Forward * CurrentSpeed;
		FVector ApparentWindVec = TrueWindVec - BoatVelocity;
		float ApparentWindStr = ApparentWindVec.Size();

		FVector WindDir;
		float WindStr;
		if (ApparentWindStr < 1.0f)
		{
			// Nesten ingen tilsynelatende vind (f.eks. lens i vind)
			WindDir = Wind->GetWindDirection();
			WindStr = 0.0f;
		}
		else
		{
			WindDir = ApparentWindVec.GetSafeNormal();
			WindStr = ApparentWindStr;
		}

		// Vinkel båt–vind (tilsynelatende): CosAngle = 1 → mot vind, 0 → halv vind, -1 → lens
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

	// 3. Hullmotstand og integrert fart: akselerasjon = seilkraft - drag
	float Drag = DragCoefficient * FMath::Square(CurrentSpeed);
	float Acceleration = CurrentSailForce - Drag;
	float NewSpeed = FMath::Clamp(CurrentSpeed + Acceleration * DeltaTime, 0.0f, MaxBoatSpeed);
	FVector Movement = Forward * NewSpeed * DeltaTime;
	AddActorWorldOffset(Movement, false);
	CurrentSpeed = NewSpeed;

	// 4. Lock to water surface with simple sine heave
	FVector Loc = GetActorLocation();
	float Time = GetWorld()->GetTimeSeconds();
	Loc.Z = WaterZ + FMath::Sin(Time * WaveFrequency * 2.0f * PI) * WaveAmplitude;
	SetActorLocation(Loc, false);

	// Debug: log movement every 2 seconds
	static float DebugTimer = 0.0f;
	DebugTimer += DeltaTime;
	if (DebugTimer > 2.0f)
	{
		UE_LOG(LogTemp, Log, TEXT("Boat: Force=%.1f Speed=%.1f Movement=(%.1f,%.1f,%.1f) Pos=(%.0f,%.0f,%.0f)"),
			CurrentSailForce, CurrentSpeed, Movement.X, Movement.Y, Movement.Z, Loc.X, Loc.Y, Loc.Z);
		DebugTimer = 0.0f;
	}

	// 5. Camera orbit
	if (SpringArm)
	{
		FRotator ArmRot = SpringArm->GetRelativeRotation();
		ArmRot.Yaw += CameraYawInput * 2.0f;
		ArmRot.Pitch = FMath::Clamp(ArmRot.Pitch + CameraPitchInput * 2.0f, -60.0f, -5.0f);
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
