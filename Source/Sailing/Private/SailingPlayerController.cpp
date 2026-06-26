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
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bTickEvenWhenPaused = true;
}

void ASailingPlayerController::SetupInputComponent()
{
	Super::SetupInputComponent();

	// Escape og M må fungere under pause (kart-modus pauser spillet)
	FInputKeyBinding& EscBind = InputComponent->BindKey(EKeys::Escape, EInputEvent::IE_Pressed, this, &ASailingPlayerController::TogglePauseMenu);
	EscBind.bExecuteWhenPaused = true;

	FInputKeyBinding& MousePressBind = InputComponent->BindKey(EKeys::LeftMouseButton, EInputEvent::IE_Pressed, this, &ASailingPlayerController::OnMapMousePressed);
	MousePressBind.bExecuteWhenPaused = true;

	FInputKeyBinding& MouseReleaseBind = InputComponent->BindKey(EKeys::LeftMouseButton, EInputEvent::IE_Released, this, &ASailingPlayerController::OnMapMouseReleased);
	MouseReleaseBind.bExecuteWhenPaused = true;

	FInputKeyBinding& MBind = InputComponent->BindKey(EKeys::M, EInputEvent::IE_Pressed, this, &ASailingPlayerController::ToggleMapView);
	MBind.bExecuteWhenPaused = true;

	FInputKeyBinding& ScrollUpBind = InputComponent->BindKey(EKeys::MouseScrollUp, EInputEvent::IE_Pressed, this, &ASailingPlayerController::OnMapScrollUp);
	ScrollUpBind.bExecuteWhenPaused = true;

	FInputKeyBinding& ScrollDownBind = InputComponent->BindKey(EKeys::MouseScrollDown, EInputEvent::IE_Pressed, this, &ASailingPlayerController::OnMapScrollDown);
	ScrollDownBind.bExecuteWhenPaused = true;
}

void ASailingPlayerController::TogglePauseMenu()
{
	// Escape lukker kartet først
	if (bShowMapView)
	{
		CloseMapView();
		return;
	}
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

void ASailingPlayerController::ToggleMapView()
{
	if (bShowPauseMenu) return; // Ikke åpne kart under pause-meny

	bShowMapView = !bShowMapView;
	SetShowMouseCursor(bShowMapView);
	if (bShowMapView)
	{
		FInputModeGameAndUI Mode;
		Mode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
		SetInputMode(Mode);
		if (ASailingHUD* HUD = Cast<ASailingHUD>(GetHUD()))
		{
			HUD->SetFullMapVisible(true);
		}
	}
	else
	{
		SetInputMode(FInputModeGameOnly());
		if (ASailingHUD* HUD = Cast<ASailingHUD>(GetHUD()))
		{
			HUD->SetFullMapVisible(false);
		}
	}
}

void ASailingPlayerController::CloseMapView()
{
	if (!bShowMapView) return;
	bShowMapView = false;
	bMapMouseDown = false;
	SetShowMouseCursor(false);
	SetInputMode(FInputModeGameOnly());
	if (ASailingHUD* HUD = Cast<ASailingHUD>(GetHUD()))
	{
		HUD->SetFullMapVisible(false);
	}
}

void ASailingPlayerController::OnMapScrollUp()
{
	if (!bShowMapView) return;
	if (ASailingHUD* HUD = Cast<ASailingHUD>(GetHUD()))
	{
		HUD->OnFullMapScroll(1.0f);
	}
}

void ASailingPlayerController::OnMapScrollDown()
{
	if (!bShowMapView) return;
	if (ASailingHUD* HUD = Cast<ASailingHUD>(GetHUD()))
	{
		HUD->OnFullMapScroll(-1.0f);
	}
}

void ASailingPlayerController::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	if (!bShowMapView || !bMapMouseDown) return;

	float DeltaX, DeltaY;
	GetInputMouseDelta(DeltaX, DeltaY);
	if (ASailingHUD* HUD = Cast<ASailingHUD>(GetHUD()))
	{
		HUD->OnFullMapDragDelta(DeltaX, DeltaY);
	}
}

void ASailingPlayerController::OnMapMousePressed()
{
	// Pause-meny klikk
	if (bShowPauseMenu)
	{
		float MouseX, MouseY;
		if (GetMousePosition(MouseX, MouseY) && GetHUD())
		{
			if (ASailingHUD* HUD = Cast<ASailingHUD>(GetHUD()))
			{
				HUD->OnPauseMenuClick(MouseX, MouseY);
			}
		}
		return;
	}

	// Kart-modus: start drag
	if (bShowMapView)
	{
		bMapMouseDown = true;
		float MouseX, MouseY;
		if (GetMousePosition(MouseX, MouseY))
		{
			if (ASailingHUD* HUD = Cast<ASailingHUD>(GetHUD()))
			{
				HUD->OnFullMapClick(MouseX, MouseY);
			}
		}
	}
}

void ASailingPlayerController::OnMapMouseReleased()
{
	if (bMapMouseDown)
	{
		bMapMouseDown = false;
		if (ASailingHUD* HUD = Cast<ASailingHUD>(GetHUD()))
		{
			HUD->OnFullMapRelease();
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
