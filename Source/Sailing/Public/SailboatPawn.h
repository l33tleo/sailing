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

	// Components – RootScene er rot slik at kapsel, båt og kamera flyttes sammen
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<USceneComponent> RootScene;

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

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sailing|Tuning")
	float WaveAmplitude = 8.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sailing|Tuning")
	float WaveFrequency = 0.5f;

	/** Visual offset to keep boat mesh slightly above water surface. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sailing|Visual")
	float BoatVisualZOffset = 38.0f;

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

	UPROPERTY()
	TWeakObjectPtr<AWindActor> CachedWind;

	AWindActor* FindWind() const;

	// Spray-state
	TArray<FSprayParticle> SprayParticles;
	float SprayEmitAccumulator = 0.0f;
	bool bSprayInitialized = false;

	void InitSpray();
	void UpdateSpray(float DeltaTime, const FVector& Forward, float Time);
	void EmitSprayParticle(const FVector& Pos, const FVector& Vel, float SizeScale);
};
