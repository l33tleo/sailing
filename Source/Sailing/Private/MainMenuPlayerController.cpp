#include "MainMenuPlayerController.h"
#include "MainMenuHUD.h"

void AMainMenuPlayerController::BeginPlay()
{
	Super::BeginPlay();
	SetShowMouseCursor(true);
	FInputModeUIOnly InputMode;
	SetInputMode(InputMode);
}

void AMainMenuPlayerController::SetupInputComponent()
{
	Super::SetupInputComponent();
	if (InputComponent)
	{
		InputComponent->BindKey(EKeys::LeftMouseButton, EInputEvent::IE_Pressed, this, &AMainMenuPlayerController::OnClick);
	}
}

void AMainMenuPlayerController::OnClick()
{
	float MouseX, MouseY;
	if (GetMousePosition(MouseX, MouseY))
	{
		if (AMainMenuHUD* MenuHUD = Cast<AMainMenuHUD>(GetHUD()))
		{
			MenuHUD->OnMouseClick(MouseX, MouseY);
		}
	}
}
