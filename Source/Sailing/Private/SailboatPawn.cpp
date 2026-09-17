#include "SailboatPawn.h"
#include "Sailing.h"
#include "SailingPlayerController.h"
#include "WindActor.h"
#include "Components/SceneComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "WaterBodyComponent.h"
#include "WaterBodyOceanActor.h"
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

	// Kapselen er rot: den flyter fysikkbasert (egen pongtong-oppdrift i Tick, se
	// ApplyPontoonBuoyancy), og båt/kamera flyttes med den. Radius ~70 gir en tettere passform
	// rundt Optimist-skroget (~1,13 m bredt) og mindre «klebing» i kyst-hjørner enn en bredere sirkel.
	CapsuleComp = CreateDefaultSubobject<UCapsuleComponent>(TEXT("Capsule"));
	CapsuleComp->InitCapsuleSize(70.0f, 50.0f);
	CapsuleComp->SetCollisionProfileName(TEXT("Pawn"));
	CapsuleComp->SetSimulatePhysics(true);
	CapsuleComp->SetEnableGravity(true);
	CapsuleComp->SetNotifyRigidBodyCollision(true); // trengs for at OnComponentHit skal fyres under fysikksimulering
	CapsuleComp->BodyInstance.bLockXRotation = false; // rull fri (krengning)
	CapsuleComp->BodyInstance.bLockYRotation = false; // pitch fri (stamping i sjøgang)
	// Fire uavhengige pongtong-krefter (ApplyPontoonBuoyancy) som hver reagerer på lokal hastighet
	// kan lett sette opp en høyfrekvent rulle-/stampe-risting seg imellom uten egen fysikkdemping
	// utover selve fjær/demper-modellen (bekreftet visuelt i PIE-skjermbilder — synlig risting).
	// Standard motor-demping er nær null; angulær demping satt vesentlig høyere enn lineær siden
	// det er rotasjonsristingen som er mest synlig.
	CapsuleComp->BodyInstance.LinearDamping = 0.5f;
	CapsuleComp->BodyInstance.AngularDamping = 4.0f;
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

	// Fysikkmasse + senket tyngdepunkt (hindrer urealistisk kantring i kast).
	CapsuleComp->SetMassOverrideInKg(NAME_None, BoatMassKg, true);
	CapsuleComp->BodyInstance.COMNudge = FVector(0.0f, 0.0f, CenterOfMassZOffset);
	CapsuleComp->BodyInstance.UpdateMassProperties();
	CapsuleComp->OnComponentHit.AddDynamic(this, &ASailboatPawn::HandleCapsuleHit);
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
			// Kapselen er fysikksimulert, så en ren `return` her slo av oppdriften mens tyngdekraften
			// fortsatte: båten falt fritt så lenge kartet var åpent (målt i PIE: 2,6 s med kart ->
			// z=-1968, 20 m under vann, før den spratt opp igjen). Hold oppdriften i gang og brems
			// kun den horisontale farten, slik at båten ligger rolig mens man ser på kartet.
			ApplyPontoonBuoyancy(DeltaTime);
			const FVector Vel = CapsuleComp->GetPhysicsLinearVelocity();
			CapsuleComp->AddForce(FVector(-Vel.X, -Vel.Y, 0.0f) * 3.0f, NAME_None, /*bAccelChange=*/true);
			return;
		}
	}

	UpdateHullWaterMask();

	// 1. Sving: sett Z-komponenten av vinkelhastigheten direkte. Rull/pitch (X/Y) forblir
	//    UENDRET av dette — de styres kun av Buoyancy/bølger (naturlig krengning/stamping).
	FVector AngVel = CapsuleComp->GetPhysicsAngularVelocityInDegrees();
	AngVel.Z = TurnInput * TurnSpeed;
	CapsuleComp->SetPhysicsAngularVelocityInDegrees(AngVel);
	TurnInput = 0.0f;

	// 2. Tilsynelatende vind (apparent wind) og polar-kurve — uendret logikk, men CurrentSpeed
	//    leses nå fra fysikkhastigheten i stedet for en egen integrert variabel.
	FVector Forward = GetActorForwardVector();
	CurrentSpeed = FVector::DotProduct(CapsuleComp->GetPhysicsLinearVelocity(), Forward);

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

	// 3. Fremdrift som massefri akselerasjon (bAccelChange) — bevarer dagens akselerasjonsfølelse
	//    uavhengig av BoatMassKg. Krengningsmoment er et SEPARAT, mindre bidrag (AddTorque) fra
	//    samme seilkraft, slik at fremdrift og krengning kan tunes/verifiseres uavhengig av hverandre.
	float Drag = DragCoefficient * FMath::Square(FMath::Max(0.0f, CurrentSpeed));
	CapsuleComp->AddForce(Forward * (CurrentSailForce - Drag) * SailForceAccelScale, NAME_None, /*bAccelChange=*/true);

	const FVector Right = FVector::CrossProduct(FVector::UpVector, Forward).GetSafeNormal();
	const float HeelTorqueSign = (Wind ? FVector::DotProduct(Right, Wind->GetWindDirection()) : 0.0f) >= 0.0f ? 1.0f : -1.0f;
	CapsuleComp->AddTorqueInDegrees(Forward * CurrentSailForce * HeelTorqueScale * HeelTorqueSign,
		NAME_None, /*bAccelChange=*/false);

	// Rettende motkraft: hindrer kantring når rull-vinkelen overstiger MaxHeelAngleDeg.
	const float HeelDeg = GetActorRotation().Roll;
	if (FMath::Abs(HeelDeg) > MaxHeelAngleDeg)
	{
		const float Excess = FMath::Abs(HeelDeg) - MaxHeelAngleDeg;
		// Fortegn: i UE er positiv Roll en rotasjon om -Forward (verifisert i PIE: 10° om +Forward
		// gir Roll=-10), så positiv krengning rettes med moment om +Forward. Tidligere fortegn var
		// omvendt og FORSTERKET utslaget utover grensen.
		const float RightingSign = HeelDeg > 0.0f ? 1.0f : -1.0f;
		CapsuleComp->AddTorqueInDegrees(Forward * Excess * RightingTorqueStrength * RightingSign,
			NAME_None, /*bAccelChange=*/false);
	}

	// Samme rettende mekanisme for stamping (pitch) — pongtong-oppdriften er ikke perfekt
	// symmetrisk fram/bak, og uten dette kan båten sette seg i en stor, vedvarende stampe-vinkel.
	const float PitchDeg = GetActorRotation().Pitch;
	if (FMath::Abs(PitchDeg) > MaxPitchAngleDeg)
	{
		const float PitchExcess = FMath::Abs(PitchDeg) - MaxPitchAngleDeg;
		// Samme fortegnsregel: positiv Pitch er en rotasjon om -Right (10° om +Right gir Pitch=-10).
		// Omvendt fortegn her var årsaken til den "vedvarende, store stampe-vinkelen": straks pitch
		// passerte MaxPitchAngleDeg dyttet "rettingen" båten VIDERE ut (målt: -23° til +41°).
		const float PitchRightingSign = PitchDeg > 0.0f ? 1.0f : -1.0f;
		CapsuleComp->AddTorqueInDegrees(Right * PitchExcess * PitchRightingTorqueStrength * PitchRightingSign,
			NAME_None, /*bAccelChange=*/false);
	}

	// Myk hastighetsgrense (physics-vennlig motkraft, ikke hard clamp).
	if (CurrentSpeed > MaxBoatSpeed)
	{
		CapsuleComp->AddForce(Forward * (MaxBoatSpeed - CurrentSpeed) * 4.0f, NAME_None, /*bAccelChange=*/true);
	}

	// Oppdrift/bølge-heave/krengning: egen fjær/demper-pongtongmodell som sampler Water System sin
	// bølgehøyde direkte (se ApplyPontoonBuoyancy). UBuoyancyComponents interne overlap-avhengige
	// deteksjon viste seg upålitelig mot et egendefinert vann-oppsett (Chaos-simulerte kropper
	// genererer ikke pålitelige overlap-events mot Water-pluginets QUERY_ONLY-kollisjon).
	ApplyPontoonBuoyancy(DeltaTime);

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
			UE_LOG(LogTemp, Warning, TEXT("[REDNING] Over land ved (%.0f,%.0f) — satt tilbake til siste trygge (%.0f,%.0f), fart nullstilt."),
				GetActorLocation().X, GetActorLocation().Y, LastSafeLoc.X, LastSafeLoc.Y);
			SetActorLocation(LastSafeLoc, /*bSweep=*/false);
			CapsuleComp->SetPhysicsLinearVelocity(FVector::ZeroVector);
			CapsuleComp->SetPhysicsAngularVelocityInDegrees(FVector::ZeroVector);
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
						CapsuleComp->SetPhysicsLinearVelocity(FVector::ZeroVector);
						CapsuleComp->SetPhysicsAngularVelocityInDegrees(FVector::ZeroVector);
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

	const float Time = GetWorld()->GetTimeSeconds();

	// === DEBUG (midlertidig) — fast avlesning av fart og posisjon hver frame ===
	if (GEngine)
	{
		const FVector DebugLoc = GetActorLocation();
		GEngine->AddOnScreenDebugMessage(100, 0.0f, FColor::Cyan,
			FString::Printf(TEXT("fart=%.0f  pos=(%.0f, %.0f, %.0f)"),
				CurrentSpeed, DebugLoc.X, DebugLoc.Y, DebugLoc.Z));
	}
	// Strupet bane-logg (~4/sek) for å spore hele ruten, også der ingen grunnstøt skjer.
	if (Time - LastDebugLogTime > 0.25f)
	{
		LastDebugLogTime = Time;
		float SurfaceZ = WaterZ;
		if (UWaterBodyComponent* Ocean = FindOceanBody())
		{
			const auto Q = Ocean->TryQueryWaterInfoClosestToWorldLocation(GetActorLocation(),
				EWaterBodyQueryFlags::ComputeLocation | EWaterBodyQueryFlags::IncludeWaves);
			if (Q.HasValue())
			{
				SurfaceZ = Q.GetValue().GetWaterSurfaceLocation().Z;
			}
		}
		UE_LOG(LogTemp, Log, TEXT("[BAATPOS] pos=(%.0f,%.0f,%.0f)  fart=%.0f  pitch=%.1f rull=%.1f  neds=%.1f"),
			GetActorLocation().X, GetActorLocation().Y, GetActorLocation().Z, CurrentSpeed,
			GetActorRotation().Pitch, GetActorRotation().Roll, SurfaceZ - GetActorLocation().Z);
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

void ASailboatPawn::HandleCapsuleHit(UPrimitiveComponent* HitComp, AActor* OtherActor, UPrimitiveComponent* OtherComp,
	FVector NormalImpulse, const FHitResult& Hit)
{
	// Bare fjordens land (øyer/kystlinje, bakt eller prosedural) ligger på FjordLand-kanalen — samme
	// filter som IsOverLand() bruker, slik at vannkollisjon/andre aktører ikke trigger grunnstøtingsrespons.
	if (!OtherComp || OtherComp->GetCollisionObjectType() != ECC_FjordLand)
	{
		return;
	}

	const FVector Forward = GetActorForwardVector();
	const float Frontalness = FMath::Clamp(FVector::DotProduct(Forward, -Hit.ImpactNormal), 0.0f, 1.0f);
	const FVector Vel = CapsuleComp->GetPhysicsLinearVelocity();
	CapsuleComp->SetPhysicsLinearVelocity(Vel * FMath::Lerp(1.0f, GroundingSpeedRetain, Frontalness));

	const FString HitName = OtherActor ? OtherActor->GetName() : TEXT("<ukjent>");
	UE_LOG(LogTemp, Warning, TEXT("[GRUNNSTOT] traff=%s frontal=%.2f"), *HitName, Frontalness);
	if (GEngine)
	{
		GEngine->AddOnScreenDebugMessage(101, 2.0f, FColor::Red,
			FString::Printf(TEXT("GRUNNSTOT: %s  frontal=%.2f"), *HitName, Frontalness));
	}
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
	// Kun FjordLand-objekter: nivåets Landscape-backdrop (WorldStatic) skal IKKE telle som «land»
	// her, ellers tror båten den er inne i land overalt og blir stående fast.
	if (!World->LineTraceSingleByObjectType(Hit, Start, End, FCollisionObjectQueryParams(ECC_FjordLand), Params))
	{
		return false;
	}
	// Bakt terreng fortsetter som sjøbunn under vannflaten; bare terreng som stikker tydelig opp
	// over vannet (over bølgetoppene, ~0,7 m) er «land». De hule prosedurale skallene har toppen
	// på z≥190 og passerer også.
	return Hit.ImpactPoint.Z > OverLandMinZ;
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

UWaterBodyComponent* ASailboatPawn::FindOceanBody()
{
	if (CachedOceanBody.IsValid())
	{
		return CachedOceanBody.Get();
	}

	TArray<AActor*> Found;
	UGameplayStatics::GetAllActorsOfClass(GetWorld(), AWaterBodyOcean::StaticClass(), Found);
	if (Found.Num() > 0)
	{
		if (AWaterBodyOcean* Ocean = Cast<AWaterBodyOcean>(Found[0]))
		{
			CachedOceanBody = Ocean->GetWaterBodyComponent();
			return CachedOceanBody.Get();
		}
	}
	return nullptr;
}

void ASailboatPawn::UpdateHullWaterMask()
{
	// Se AOceanWaterSetupActor::SpawnOceanBody: havmaterialet klipper bort vannflaten i en boks
	// rundt skroget, så cockpiten ikke ser vannfylt ut. Boksen følger båtens posisjon og kurs.
	UWaterBodyComponent* Ocean = FindOceanBody();
	UMaterialInstanceDynamic* WaterMID = Ocean ? Ocean->GetWaterMaterialInstance() : nullptr;
	if (!WaterMID || !BoatMesh)
	{
		return;
	}

	FVector Fwd = GetActorForwardVector();
	Fwd.Z = 0.0f;
	Fwd = Fwd.GetSafeNormal();
	const FVector Center = BoatMesh->GetComponentLocation() + Fwd * HullMaskCenterOffsetX;
	WaterMID->SetVectorParameterValue(TEXT("HullMaskPos"), FLinearColor(Center.X, Center.Y, Center.Z, 0.0f));
	WaterMID->SetVectorParameterValue(TEXT("HullMaskFwd"), FLinearColor(Fwd.X, Fwd.Y, 0.0f, 0.0f));
	WaterMID->SetVectorParameterValue(TEXT("HullMaskHalfExtent"), FLinearColor(HullMaskHalfExtent.X, HullMaskHalfExtent.Y, 0.0f, 0.0f));
}

void ASailboatPawn::ApplyPontoonBuoyancy(float DeltaTime)
{
	UWaterBodyComponent* Ocean = FindOceanBody();
	if (!Ocean)
	{
		return;
	}

	// Symmetrisk diamant-plassering (Optimist ~230x113 uu fotavtrykk). Forrige oppsett hadde babord/
	// styrbord forskjøvet mot akter (X=-10 i stedet for X=0) og kun ett punkt ved baugen — denne
	// fram/bak-asymmetrien ga en vedvarende, stor stampe-vinkel (~22-29°) i stille vann i stedet for
	// å balansere rundt 0° (verifisert i PIE). Alle punkter krysser nå X=0/Y=0 symmetrisk.
	static const FVector PontoonOffsets[] = {
		FVector(115.0f, 0.0f, 0.0f),   // baug
		FVector(-115.0f, 0.0f, 0.0f),  // akter
		FVector(0.0f, -56.0f, 0.0f),   // babord
		FVector(0.0f, 56.0f, 0.0f),    // styrbord
	};

	// Numerisk stabilitet: kreftene påføres én gang per frame (eksplisitt integrasjon), og et
	// dempingsledd c er da bare stabilt når c*dt/m_eff < 2, der m_eff er den effektive massen sett
	// fra pongtongpunktet: 1/m_eff = 1/M + x²/Iyy + y²/Ixx. For baug/akter (115 cm arm, Iyy≈1,1e5)
	// er m_eff bare ~7 kg, så BuoyancyDamping=900 ved ~37 fps gir c*dt/m_eff≈3,4 — dempingen
	// OVERSKYTER hver frame og pisker opp stampingen (målt i PIE: ±25° pitch, 152°/s, uavhengig av
	// bølgehøyde, og verre jo lavere fps). Dempingen begrenses derfor per pongtong til en trygg
	// andel av m_eff/dt.
	const float SafeDt = FMath::Clamp(DeltaTime, 1.0f / 240.0f, 1.0f / 15.0f);
	const FVector Inertia = CapsuleComp->GetInertiaTensor();
	const float InvMass = 1.0f / FMath::Max(1.0f, CapsuleComp->GetMass());

	const FTransform ActorTransform = GetActorTransform();
	for (int32 PontoonIdx = 0; PontoonIdx < UE_ARRAY_COUNT(PontoonOffsets); ++PontoonIdx)
	{
		const FVector& LocalOffset = PontoonOffsets[PontoonIdx];
		const FVector WorldPos = ActorTransform.TransformPosition(LocalOffset);

		const float InvEffMass = InvMass
			+ FMath::Square(LocalOffset.X) / FMath::Max(1.0f, Inertia.Y)
			+ FMath::Square(LocalOffset.Y) / FMath::Max(1.0f, Inertia.X);
		const float StableDamping = FMath::Min(BuoyancyDamping, 0.6f / (InvEffMass * SafeDt));

		// NB: bekvemmelighetsfunksjonen GetWaterSurfaceInfoAtLocation() setter ALDRI IncludeWaves-
		// flagget og returnerer derfor det flate vannplanet (verifisert i PIE: nøyaktig z=100 på alle
		// pongtonger mens det visuelle havet bølget — båten hang over bølgedaler og skar gjennom
		// topper). Spør derfor direkte med IncludeWaves, så fysikken følger de samme Gerstner-bølgene
		// som tegnes.
		const TValueOrError<FWaterBodyQueryResult, EWaterBodyQueryError> Query = Ocean->TryQueryWaterInfoClosestToWorldLocation(
			WorldPos, EWaterBodyQueryFlags::ComputeLocation | EWaterBodyQueryFlags::IncludeWaves);
		if (!Query.HasValue())
		{
			continue;
		}
		const FVector SurfaceLoc = Query.GetValue().GetWaterSurfaceLocation();

		// Bølgeflatens vertikalfart slik pongtongen opplever den (endelig differanse, inkluderer at
		// båten selv flytter seg gjennom bølgene). Dempingen virker mot RELATIV fart: demping mot
		// absolutt fart bremset også den ønskede hiv-bevegelsen, så skroget hang etter sjøen
		// (målt: nedsenkning 11 ± 6 cm, topper på 23 cm — nær ripa).
		float SurfaceVelZ = 0.0f;
		if (bPontoonSurfaceValid[PontoonIdx])
		{
			SurfaceVelZ = FMath::Clamp((SurfaceLoc.Z - PrevPontoonSurfaceZ[PontoonIdx]) / SafeDt, -400.0f, 400.0f);
		}
		PrevPontoonSurfaceZ[PontoonIdx] = SurfaceLoc.Z;
		bPontoonSurfaceValid[PontoonIdx] = true;

		// Nedsenkningsdybde: positiv når pongtong-punktet er under den (bølge-forstyrrede) vannflaten.
		const float Submersion = FMath::Clamp(SurfaceLoc.Z - WorldPos.Z, 0.0f, PontoonRadius * 2.0f);
		if (Submersion <= 0.0f)
		{
			continue;
		}

		// Fjærkraft proporsjonal med dybde, dempet mot lokal vertikal hastighet (hindrer evig humping).
		// Dempingsleddet er ubegrenset i hastighet: treffer en pongtong vannet med høy fallfart (f.eks.
		// etter en lengre innledende fall-transient), blir dempingskraften alene enorm og skyter båten
		// voldsomt oppover som en trampoline (verifisert i PIE — eskalerende oscillasjon). Klampet til
		// et par ganger båtens vekt per pongtong for å hindre dette, uten å fjerne selve dempingseffekten.
		const float PointVelocityZ = CapsuleComp->GetPhysicsLinearVelocityAtPoint(WorldPos).Z;
		const float RawForceZ = Submersion * BuoyancySpringStrength - (PointVelocityZ - SurfaceVelZ) * StableDamping;
		const float MaxForceZ = BoatMassKg * 980.0f * 2.0f; // ~2x båtens vekt, per pongtong
		const float ForceZ = FMath::Clamp(RawForceZ, -MaxForceZ, MaxForceZ);
		CapsuleComp->AddForceAtLocation(FVector(0.0f, 0.0f, ForceZ), WorldPos);
	}
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
	// NB ytelse: et eksplisitt SprayMesh->MarkRenderStateDirty() hver frame GJENSKAPER hele
	// render-proxyen for instansene (målt med `stat dumphitches`: klynger av 80–135 ms hitcher i
	// UpdatePrimitive/FlushPendingRHICommands straks spruten slo inn ved høy fart). Én
	// BatchUpdateInstancesTransforms markerer bare instansdataene som endret. Når ingen partikler
	// lever (og ingen levde forrige frame) hoppes oppdateringen helt over.
	bool bAnyAlive = false;
	SprayTransforms.SetNum(SprayParticles.Num(), EAllowShrinking::No);
	for (int32 i = 0; i < SprayParticles.Num(); ++i)
	{
		FSprayParticle& P = SprayParticles[i];
		FTransform Xform;
		if (P.Life > 0.0f)
		{
			bAnyAlive = true;
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
		SprayTransforms[i] = Xform;
	}

	if (bAnyAlive || bSprayWasAlive)
	{
		SprayMesh->BatchUpdateInstancesTransforms(0, SprayTransforms, /*bWorldSpace=*/true,
			/*bMarkRenderStateDirty=*/true, /*bTeleport=*/true);
	}
	bSprayWasAlive = bAnyAlive;
}
