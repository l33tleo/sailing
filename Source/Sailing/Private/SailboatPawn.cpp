#include "SailboatPawn.h"
#include "SailingPlayerController.h"
#include "WindActor.h"
#include "Components/SceneComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Materials/MaterialInterface.h"
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
		FVector TrueWindVec = Wind->GetWindDirection() * Wind->GetWindStrength();
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
