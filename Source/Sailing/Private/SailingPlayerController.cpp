#include "SailingPlayerController.h"
#include "SailboatPawn.h"
#include "SailingHUD.h"
#include "SailingGameMode.h"
#include "EnhancedInputSubsystems.h"
#include "EnhancedInputComponent.h"
#include "InputMappingContext.h"
#include "InputAction.h"
#include "InputModifiers.h"
#include "InputTriggers.h"
#include "Kismet/GameplayStatics.h"

ASailingPlayerController::ASailingPlayerController()
{
}

void ASailingPlayerController::SetupInputComponent()
{
	Super::SetupInputComponent();
	InputComponent->BindKey(EKeys::Escape, EInputEvent::IE_Pressed, this, &ASailingPlayerController::TogglePauseMenu);
	InputComponent->BindKey(EKeys::LeftMouseButton, EInputEvent::IE_Pressed, this, &ASailingPlayerController::OnPauseMenuClick);
}

void ASailingPlayerController::TogglePauseMenu()
{
	bShowPauseMenu = !bShowPauseMenu;
	SetShowMouseCursor(bShowPauseMenu);
	if (bShowPauseMenu)
	{
		FInputModeGameAndUI Mode;
		Mode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
		SetInputMode(Mode);
	}
	else
	{
		SetInputMode(FInputModeGameOnly());
	}
}

void ASailingPlayerController::ClosePauseMenu()
{
	if (bShowPauseMenu)
	{
		bShowPauseMenu = false;
		SetShowMouseCursor(false);
		SetInputMode(FInputModeGameOnly());
	}
}

void ASailingPlayerController::OnPauseMenuClick()
{
	if (!bShowPauseMenu) return;
	float MouseX, MouseY;
	if (GetMousePosition(MouseX, MouseY) && GetHUD())
	{
		if (ASailingHUD* HUD = Cast<ASailingHUD>(GetHUD()))
		{
			HUD->OnPauseMenuClick(MouseX, MouseY);
		}
	}
}

void ASailingPlayerController::CreateInputAssets()
{
	// Create Turn action (1D Axis - float)
	TurnAction = NewObject<UInputAction>(this, TEXT("IA_Turn"));
	TurnAction->ValueType = EInputActionValueType::Axis1D;

	// Create Camera action (2D Axis - Vector2D)
	CameraAction = NewObject<UInputAction>(this, TEXT("IA_Camera"));
	CameraAction->ValueType = EInputActionValueType::Axis2D;

	// Create Mapping Context
	SailingMappingContext = NewObject<UInputMappingContext>(this, TEXT("IMC_Sailing"));

	// Map D key to Turn (positive = turn right)
	FEnhancedActionKeyMapping& DMapping = SailingMappingContext->MapKey(TurnAction, EKeys::D);

	// Map A key to Turn with Negate modifier (negative = turn left)
	FEnhancedActionKeyMapping& AMapping = SailingMappingContext->MapKey(TurnAction, EKeys::A);
	UInputModifierNegate* NegMod = NewObject<UInputModifierNegate>(this);
	AMapping.Modifiers.Add(NegMod);

	// Map Mouse XY to Camera look
	SailingMappingContext->MapKey(CameraAction, EKeys::Mouse2D);
}

void ASailingPlayerController::BeginPlay()
{
	Super::BeginPlay();

	CreateInputAssets();

	// Add mapping context to Enhanced Input subsystem
	if (UEnhancedInputLocalPlayerSubsystem* Subsystem =
		ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(GetLocalPlayer()))
	{
		Subsystem->AddMappingContext(SailingMappingContext, 0);
	}

	// Assign input actions to the pawn and manually bind them
	if (ASailboatPawn* Boat = Cast<ASailboatPawn>(GetPawn()))
	{
		Boat->TurnAction = TurnAction;
		Boat->CameraAction = CameraAction;

		if (UEnhancedInputComponent* EIC = Cast<UEnhancedInputComponent>(Boat->InputComponent))
		{
			EIC->BindAction(TurnAction, ETriggerEvent::Triggered, Boat, &ASailboatPawn::HandleTurn);
			EIC->BindAction(CameraAction, ETriggerEvent::Triggered, Boat, &ASailboatPawn::HandleCamera);
		}
	}

	// Set input mode to game only so keyboard input works during Play
	FInputModeGameOnly InputMode;
	SetInputMode(InputMode);
	SetShowMouseCursor(false);
}
