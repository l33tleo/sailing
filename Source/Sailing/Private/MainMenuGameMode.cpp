#include "MainMenuGameMode.h"
#include "MainMenuHUD.h"
#include "MainMenuPlayerController.h"

AMainMenuGameMode::AMainMenuGameMode()
{
	PlayerControllerClass = AMainMenuPlayerController::StaticClass();
	HUDClass = AMainMenuHUD::StaticClass();
	DefaultPawnClass = nullptr;
}

void AMainMenuGameMode::BeginPlay()
{
	Super::BeginPlay();
}
