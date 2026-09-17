#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Pawn.h"
#include "InputActionValue.h"
#include "SailboatPawn.generated.h"

class USceneComponent;
class UCapsuleComponent;
class USpringArmComponent;
class UCameraComponent;
class UInputAction;
class AWindActor;
class UInstancedStaticMeshComponent;
class UPrimitiveComponent;
class UWaterBodyComponent;

/** Én skum-/spray-partikkel (verdensrom). Simuleres på CPU, tegnes via instanced mesh. */
struct FSprayParticle
{
	FVector Position = FVector::ZeroVector;
	FVector Velocity = FVector::ZeroVector;
	float Life = 0.0f;     // gjenstående levetid (s); <= 0 = inaktiv
	float MaxLife = 1.0f;
	float Size = 1.0f;     // grunnskala
	float Yaw = 0.0f;
};

UCLASS()
class SAILING_API ASailboatPawn : public APawn
{
	GENERATED_BODY()

public:
	ASailboatPawn();

	virtual void BeginPlay() override;
	virtual void Tick(float DeltaTime) override;
	virtual void SetupPlayerInputComponent(UInputComponent* PlayerInputComponent) override;

	// Components – kapselen er rot slik at den sveiper mot land, og båt/kamera flyttes med den
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UCapsuleComponent> CapsuleComp;

	/** Kombinert Optimist-båt mesh (skrog, mast, seil, ror osv. i ett). */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UStaticMeshComponent> BoatMesh;

	/** Plan bak i båten (stern) med ugjennomtrengelig materiale – blokkerer «innsikt» bakfra. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UStaticMeshComponent> SternShield;

	/** Målpunkt for kamera (over båten) slik at båten havner lavere i bildet. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<USceneComponent> CameraTarget;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<USpringArmComponent> SpringArm;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UCameraComponent> Camera;

	/** Instanced mesh-pool for skum/spray (baug-skum + vinddrift i kast). */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UInstancedStaticMeshComponent> SprayMesh;

	// Spray-tuning. Av som standard: skum-kulene (Engine-sfærer) så urealistiske ut og
	// flimret (hundrevis som popper inn/ut). Kan slås på igjen i editoren om ønskelig.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sailing|Spray")
	bool bEnableSpray = false;

	/** Antall partikler i poolen (forhåndsallokert). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sailing|Spray", meta = (ClampMin = "8", ClampMax = "256"))
	int32 SprayPoolSize = 96;

	/** Fart (enheter/s) der baug-skum begynner å danne seg. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sailing|Spray", meta = (ClampMin = "0"))
	float SpraySpeedThreshold = 280.0f;

	/** Levetid per partikkel (s). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sailing|Spray", meta = (ClampMin = "0.1"))
	float SprayParticleLife = 0.9f;

	/** Grunnstørrelse (skala på kule-mesh, 1 ≈ 100 enheter diameter). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sailing|Spray", meta = (ClampMin = "0.02"))
	float SprayParticleSize = 0.18f;

	/** «Tyngdekraft» som trekker spray ned igjen (enheter/s²). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sailing|Spray", meta = (ClampMin = "0"))
	float SprayGravity = 900.0f;

	// Tuning
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sailing|Tuning")
	float TurnSpeed = 90.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sailing|Tuning")
	float MaxBoatSpeed = 800.0f;

	/** Andel av farten som beholdes ved frontal grunnstøting (0 = full stopp, 1 = ingen brems). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sailing|Tuning", meta = (ClampMin = "0", ClampMax = "1"))
	float GroundingSpeedRetain = 0.1f;

	/** Verdens-Z terrenget må stikke over for å regnes som «land» i IsOverLand (vannflaten er
	 *  z=100, bølgetopper ~169). Bakt terreng fortsetter som sjøbunn under dette nivået. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sailing|Grounding")
	float OverLandMinZ = 170.0f;

	/** Hullmotstand: drag = DragCoefficient * Speed^2 (enheter: 1/lengde). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sailing|Tuning", meta = (ClampMin = "0"))
	float DragCoefficient = 0.0012f;

	// Polar curve tuning — kraftmultiplikator per kurs relativt til vind
	// No-go zone: vinkel fra vindretning der seilet ikke kan generere kraft
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sailing|WindModel", meta = (ClampMin = "30", ClampMax = "60"))
	float NoGoZoneAngle = 50.0f;

	// Kraft ved close-hauled (rett utenfor no-go zone)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sailing|WindModel", meta = (ClampMin = "0", ClampMax = "1"))
	float CloseHauledForce = 0.18f;

	// Kraft ved beam reach (90° fra vind) — maksimal ytelse
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sailing|WindModel", meta = (ClampMin = "0", ClampMax = "1"))
	float BeamReachForce = 1.0f;

	// Kraft ved broad reach (~135° fra vind)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sailing|WindModel", meta = (ClampMin = "0", ClampMax = "1"))
	float BroadReachForce = 0.5f;

	// Kraft ved running (180° fra vind, ren drag)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sailing|WindModel", meta = (ClampMin = "0", ClampMax = "1"))
	float RunningForce = 0.22f;

	/** Potens på kraftmultiplikator (1 = lineær; >1 gir skarpere polar, dårlige vinkler tregere). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sailing|WindModel", meta = (ClampMin = "0.5", ClampMax = "2"))
	float PolarSharpness = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sailing|Tuning")
	float WaterZ = 100.0f;

	/** Løfter båt-meshet (vannlinje = mesh z=0, jf. scripts/blender/CLAUDE.md) opp til vannflaten.
	 *  Kapselsenteret hviler i likevekt BoatMassKg*980/(4*BuoyancySpringStrength) ≈ 11 uu UNDER
	 *  vannflaten (målt i PIE: kapsel z≈89 ved vann z=100), så offseten må matche det. Forrige verdi
	 *  (38) stammet fra før den fysikkbaserte oppdriften og lot skroget sveve ~27 cm over vannet.
	 *  Endres masse eller fjærstyrke, må denne følge etter. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sailing|Visual")
	float BoatVisualZOffset = 11.0f;

	/** Halv lengde/bredde (uu, langs/tvers av båten) på boksen der vannflaten klippes bort så
	 *  cockpiten ikke ser vannfylt ut. Må ligge INNENFOR skrogets vannlinje-fotavtrykk (ellers
	 *  synes et hull i sjøen rundt båten) — tunet mot Optimist3735 med skjermbilder i PIE. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sailing|Visual")
	FVector2D HullMaskHalfExtent = FVector2D(109.0f, 50.0f);

	/** Forskyvning av maskeboksens senter langs båtens lengdeakse (uu, + = mot baug). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sailing|Visual")
	float HullMaskCenterOffsetX = -3.0f;

	// --- Fysikkbasert oppdrift (egen fjær/demper-oppdrift per pongtong, sample av Water System sin
	// bølgehøyde direkte — se Tick(). UBuoyancyComponents interne overlap-avhengige "er jeg i vann"-
	// deteksjon viste seg upålitelig for et egendefinert vann-oppsett (Chaos-simulerte kropper
	// genererer ikke pålitelige overlap-events mot Water-pluginets QUERY_ONLY-kollisjon — et kjent
	// samspillsproblem), så oppdriften beregnes i stedet direkte fra vannoverflatens Z-høyde. ---

	/** Båtens masse for fysikksimulering (kg). Optimist-skrog + rigg/mannskap for gameplay-stabilitet. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sailing|Physics", meta = (ClampMin = "10"))
	float BoatMassKg = 55.0f;

	/** Fjærstyrke i oppdriftsmodellen: kraft = nedsenkningsdybde * BuoyancySpringStrength per pongtong.
	 *  Beregnet mot likevekt: BoatMassKg*980/(~2 nedsenkede pongtonger * ~25 uu typisk nedsenkning) —
	 *  20000 var ~20x for sterkt (ga en ustabil, eskalerende fjær-oscillering fra kald start i PIE). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sailing|Physics", meta = (ClampMin = "0"))
	float BuoyancySpringStrength = 1200.0f;

	/** Demping mot vertikal hastighet i hver nedsenket pongtong (hindrer evig humping/oscillering). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sailing|Physics", meta = (ClampMin = "0"))
	float BuoyancyDamping = 900.0f;

	/** Pongtong-"radius" (cm) — maks nedsenkningsdybde før kraften flater ut. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sailing|Physics", meta = (ClampMin = "1"))
	float PontoonRadius = 60.0f;

	/** Vertikal senking av tyngdepunkt (cm, negativ = ned) — hindrer urealistisk kantring i kast. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sailing|Physics")
	float CenterOfMassZOffset = -30.0f;

	/** Skalering fra CurrentSailForce (uendret polar-modell) til fremdriftsakselerasjon. Massefri
	 *  (AddForce med bAccelChange), så fremdriftsfølelsen forblir lik den kinematiske modellen. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sailing|Physics", meta = (ClampMin = "0"))
	float SailForceAccelScale = 1.0f;

	/** Høyde over kapselsenteret der seilkraften angriper — gir naturlig krengningsmoment. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sailing|Physics")
	float SailEffortHeight = 120.0f;

	/** Skalerer krengningsmomentet fra seilkraften (separat fra fremdriften, se Tick). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sailing|Physics", meta = (ClampMin = "0"))
	float HeelTorqueScale = 1.0f;

	/** Rull-vinkel (grader) der en rettende motkraft begynner å virke, for å hindre kantring i sterke kast. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sailing|Physics", meta = (ClampMin = "0", ClampMax = "89"))
	float MaxHeelAngleDeg = 35.0f;

	/** Styrke på den rettende motkraften utover MaxHeelAngleDeg. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sailing|Physics", meta = (ClampMin = "0"))
	float RightingTorqueStrength = 6.0f;

	/** Samme rettende mekanisme som MaxHeelAngleDeg/RightingTorqueStrength, men for stamping
	 *  (pitch) i stedet for krengning (roll). Uten dette kan pongtong-oppdriften (som ikke er
	 *  perfekt symmetrisk fram/bak) sette seg fast i en stor, vedvarende stamping-vinkel i stedet
	 *  for å rette seg opp (verifisert i PIE: pitch stabiliserte på ~22-29° i stille vann). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sailing|Physics", meta = (ClampMin = "0", ClampMax = "89"))
	float MaxPitchAngleDeg = 15.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sailing|Physics", meta = (ClampMin = "0"))
	float PitchRightingTorqueStrength = 6.0f;

	// Input actions (set by PlayerController)
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Input")
	TObjectPtr<UInputAction> TurnAction;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Input")
	TObjectPtr<UInputAction> CameraAction;

	// State
	UPROPERTY(BlueprintReadOnly, Category = "Sailing|State")
	float CurrentSailForce = 0.0f;

	/** Nåværende fart fremover (enheter/s). */
	UPROPERTY(BlueprintReadOnly, Category = "Sailing|State")
	float CurrentSpeed = 0.0f;

	// Input handlers (public so PlayerController can bind them)
	void HandleTurn(const FInputActionValue& Value);
	void HandleCamera(const FInputActionValue& Value);

private:

	float TurnInput = 0.0f;
	float CameraYawInput = 0.0f;
	float CameraPitchInput = 0.0f;

	// DEBUG (midlertidig): strupe posisjonslogg til ~4 ganger i sekundet
	float LastDebugLogTime = 0.0f;

	UPROPERTY()
	TWeakObjectPtr<AWindActor> CachedWind;

	AWindActor* FindWind() const;

	/** Sant hvis punktet ligger over/inne i en landmasse (nedstråle treffer land). */
	bool IsOverLand(const FVector& Loc) const;

	/** Siste posisjon som var i åpent vann — sikkerhetsnett mot å havne inne i land. */
	FVector LastSafeLoc = FVector::ZeroVector;
	bool bHasSafeLoc = false;

	// Spray-state
	TArray<FSprayParticle> SprayParticles;
	float SprayEmitAccumulator = 0.0f;
	bool bSprayInitialized = false;

	void InitSpray();
	void UpdateSpray(float DeltaTime, const FVector& Forward, float Time);
	void EmitSprayParticle(const FVector& Pos, const FVector& Vel, float SizeScale);

	/** Grunnstøtingsrespons: demper fart proporsjonalt med treffets frontalitet (fysikkløseren
	 *  håndterer selve utskyvingen siden landmassene har BlockAll-kollisjon). */
	UFUNCTION()
	void HandleCapsuleHit(UPrimitiveComponent* HitComp, AActor* OtherActor, UPrimitiveComponent* OtherComp,
		FVector NormalImpulse, const FHitResult& Hit);

	UPROPERTY()
	TWeakObjectPtr<UWaterBodyComponent> CachedOceanBody;

	/** Finner (og cacher) havets UWaterBodyComponent — brukes til direkte høyde-spørring i Tick(). */
	UWaterBodyComponent* FindOceanBody();

	/** Fjær/demper-oppdrift: sampler vannoverflatens Z-høyde ved hvert pongtong-punkt og påfører
	 *  kraft proporsjonalt med nedsenkningsdybde, pluss demping mot lokal vertikal hastighet. */
	void ApplyPontoonBuoyancy(float DeltaTime);
	void UpdateHullWaterMask();

	/** Gjenbrukt buffer + "levde forrige frame"-flagg for sprut-instansene (se UpdateSpray). */
	TArray<FTransform> SprayTransforms;
	bool bSprayWasAlive = false;

	/** Forrige frames bølgehøyde per pongtong (for bølgeflatens vertikalfart, se ApplyPontoonBuoyancy). */
	float PrevPontoonSurfaceZ[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
	bool bPontoonSurfaceValid[4] = { false, false, false, false };
};
