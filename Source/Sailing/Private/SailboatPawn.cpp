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
#include "ProceduralMeshComponent.h"
#include "Engine/Engine.h"   // DEBUG (midlertidig): GEngine->AddOnScreenDebugMessage

ASailboatPawn::ASailboatPawn()
{
	PrimaryActorTick.bCanEverTick = true;

	// Kapselen er rot: den sveiper mot land (grunnstøting), og båt/kamera flyttes med den.
	// Radius ~70 gir en tettere passform rundt Optimist-skroget (~1,13 m bredt) og
	// mindre «klebing» i kyst-hjørner enn en bredere sirkel.
	CapsuleComp = CreateDefaultSubobject<UCapsuleComponent>(TEXT("Capsule"));
	CapsuleComp->InitCapsuleSize(70.0f, 50.0f);
	CapsuleComp->SetCollisionProfileName(TEXT("Pawn"));
	RootComponent = CapsuleComp;

	// Kombinert Optimist-båt (alle deler i ett mesh fra Blender)
	BoatMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("BoatMesh"));
	BoatMesh->SetupAttachment(CapsuleComp);
	BoatMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	BoatMesh->SetCastShadow(true);

	// Plan bak i båten (stern) – ugjennomtrengelig, så man ikke ser «inn» bakfra
	SternShield = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("SternShield"));
	SternShield->SetupAttachment(CapsuleComp);
	SternShield->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	SternShield->SetCastShadow(true);
	// Posisjon og skala settes i BeginPlay ut fra båt-mesh bounds (måling)
	SternShield->SetRelativeRotation(FRotator(90.0f, 0.0f, 0.0f));   // Plan i YZ, normal -X (mot kamera bakfra)

	// Målpunkt over båten: kamera ser mot dette punktet, så båten havner lavere i bildet
	CameraTarget = CreateDefaultSubobject<USceneComponent>(TEXT("CameraTarget"));
	CameraTarget->SetupAttachment(CapsuleComp);
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
	SprayMesh->SetupAttachment(CapsuleComp);
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

	// Sveip fremdriften mot land (kapselen er rot). Enkel, robust respons som ALDRI
	// låser båten fast: skyv ut av evt. penetrasjon, skli langs kysten med resten av
	// trekket, og reduser fart etter hvor frontalt treffet var (aldri helt til 0, så
	// spilleren alltid kan manøvrere seg løs).
	FHitResult Hit;
	AddActorWorldOffset(Movement, /*bSweep=*/true, &Hit);
	CurrentSpeed = NewSpeed;

	if (Hit.bBlockingHit)
	{
		const float Frontalness = FMath::Clamp(FVector::DotProduct(Forward, -Hit.ImpactNormal), 0.0f, 1.0f);

		// Skyv ut hvis kapselen har havnet delvis inne i land (uten å drepe farten).
		if (Hit.bStartPenetrating)
		{
			AddActorWorldOffset(Hit.Normal * (Hit.PenetrationDepth + 2.0f), /*bSweep=*/false);
		}

		// Skli resten av trekket langs kystflaten.
		const FVector Slide = FVector::VectorPlaneProject(Movement * (1.0f - Hit.Time), Hit.ImpactNormal);
		if (!Slide.IsNearlyZero())
		{
			AddActorWorldOffset(Slide, /*bSweep=*/true);
		}

		// Frontal grunnstøting bremser mye; skrå streif nesten ingenting.
		CurrentSpeed = NewSpeed * FMath::Lerp(1.0f, GroundingSpeedRetain, Frontalness);

		// === DEBUG (midlertidig) ===
		const FVector BLoc = GetActorLocation();
		const FString HitName = Hit.GetActor() ? Hit.GetActor()->GetName() : TEXT("<ukjent>");
		UE_LOG(LogTemp, Warning,
			TEXT("[GRUNNSTOT] traff=%s pos=(%.0f,%.0f,%.0f)  normal=(%.2f,%.2f,%.2f)  startPen=%d  frontal=%.2f  fart %.0f->%.0f"),
			*HitName, BLoc.X, BLoc.Y, BLoc.Z, Hit.Normal.X, Hit.Normal.Y, Hit.Normal.Z,
			Hit.bStartPenetrating ? 1 : 0, Frontalness, NewSpeed, CurrentSpeed);
		if (GEngine)
		{
			GEngine->AddOnScreenDebugMessage(101, 2.0f, FColor::Red,
				FString::Printf(TEXT("GRUNNSTOT: %s  startPen=%d  frontal=%.2f  fart=%.0f"),
					*HitName, Hit.bStartPenetrating ? 1 : 0, Frontalness, CurrentSpeed));
		}
	}

	// 4. Lås til vannoverflate med enkel sinus-heave
	FVector Loc = GetActorLocation();
	float Time = GetWorld()->GetTimeSeconds();
	Loc.Z = WaterZ + FMath::Sin(Time * WaveFrequency * 2.0f * PI) * WaveAmplitude;
	SetActorLocation(Loc, false);

	// 4c. Sikkerhetsnett mot å være inne i en landmasse. Landmassene er hule skall
	// (kollisjon kun langs ytterkysten), så havner båten først på innsiden — via en
	// lagret posisjon, eller en sjelden gjennomkryping — kan den seile fritt i det tomme
	// interiøret. Her fanges det: er båten over land, settes den tilbake til siste trygge
	// vann-posisjon (eller flyttes ut til nærmeste vann ved lasting inne i land).
	if (IsOverLand(GetActorLocation()))
	{
		// Bruk siste trygge posisjon bare hvis den faktisk er vann (den kan være ugyldig
		// hvis den ble satt før landkollisjonen var bygd på første frame).
		if (bHasSafeLoc && !IsOverLand(LastSafeLoc))
		{
			SetActorLocation(LastSafeLoc, /*bSweep=*/false);
			CurrentSpeed = 0.0f;
		}
		else
		{
			// Ingen gyldig trygg posisjon (typisk: lastet inne i land fra en gammel lagret
			// posisjon). Søk utover i ringer etter nærmeste åpne vann og flytt båten dit.
			const FVector Base = GetActorLocation();
			for (float R = 3000.0f; R <= 300000.0f; R += 3000.0f)
			{
				bool bRescued = false;
				for (int32 A = 0; A < 24; ++A)
				{
					const float Ang = A * (2.0f * PI / 24.0f);
					FVector Test(Base.X + FMath::Cos(Ang) * R, Base.Y + FMath::Sin(Ang) * R, WaterZ);
					if (!IsOverLand(Test))
					{
						SetActorLocation(Test, /*bSweep=*/false);
						LastSafeLoc = Test;
						bHasSafeLoc = true;
						CurrentSpeed = 0.0f;
						bRescued = true;
						UE_LOG(LogTemp, Warning, TEXT("[REDNING] Båt inne i land ved (%.0f,%.0f) — flyttet til vann (%.0f,%.0f)."),
							Base.X, Base.Y, Test.X, Test.Y);
						break;
					}
				}
				if (bRescued)
				{
					break;
				}
			}
		}
	}
	else
	{
		LastSafeLoc = GetActorLocation();
		bHasSafeLoc = true;
	}

	// === DEBUG (midlertidig) — fast avlesning av fart og posisjon hver frame ===
	if (GEngine)
	{
		GEngine->AddOnScreenDebugMessage(100, 0.0f, FColor::Cyan,
			FString::Printf(TEXT("fart=%.0f  pos=(%.0f, %.0f, %.0f)"),
				CurrentSpeed, Loc.X, Loc.Y, Loc.Z));
	}
	// Strupet bane-logg (~4/sek) for å spore hele ruten, også der ingen grunnstøt skjer.
	if (Time - LastDebugLogTime > 0.25f)
	{
		LastDebugLogTime = Time;
		UE_LOG(LogTemp, Log, TEXT("[BAATPOS] pos=(%.0f,%.0f,%.0f)  fart=%.0f"),
			GetActorLocation().X, GetActorLocation().Y, GetActorLocation().Z, CurrentSpeed);
	}

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

bool ASailboatPawn::IsOverLand(const FVector& Loc) const
{
	// Land (øyer/fastland) er WorldStatic med kollisjon; havet har ingen kollisjon.
	// En loddrett stråle gjennom båtens XY treffer land kun der båten er innenfor en
	// landmasse-fotavtrykk — da er den ved vannlinjen «under» det hule skallet.
	UWorld* World = GetWorld();
	if (!World)
	{
		return false;
	}
	const FVector Start(Loc.X, Loc.Y, 100000.0f);
	const FVector End(Loc.X, Loc.Y, -100000.0f);
	FCollisionQueryParams Params(SCENE_QUERY_STAT(BoatOverLand), /*bTraceComplex=*/true, this);
	FHitResult Hit;
	if (!World->LineTraceSingleByChannel(Hit, Start, End, ECC_WorldStatic, Params))
	{
		return false;
	}
	// Bare fjord-landmassene (øyer/kystlinje) er ProceduralMeshComponent. Nivåets
	// Landscape-backdrop er en heightfield-komponent og skal IKKE telle som «land»
	// her, ellers tror båten den er inne i land overalt og blir stående fast.
	return Hit.GetComponent() && Hit.GetComponent()->IsA(UProceduralMeshComponent::StaticClass());
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
