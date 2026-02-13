#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "SailingPlayerController.generated.h"

class UInputMappingContext;
class UInputAction;

UCLASS()
class SAILING_API ASailingPlayerController : public APlayerController
{
	GENERATED_BODY()

public:
	ASailingPlayerController();

	virtual void SetupInputComponent() override;

	/** Om pause-menyen vises (Escape). */
	bool IsPauseMenuShown() const { return bShowPauseMenu; }
	/** Lukk pause-meny (kalles fra HUD ved Fortsett). */
	void ClosePauseMenu();

protected:
	virtual void BeginPlay() override;

private:
	void TogglePauseMenu();
	void OnPauseMenuClick();

	bool bShowPauseMenu = false;
	UPROPERTY()
	TObjectPtr<UInputMappingContext> SailingMappingContext;

	UPROPERTY()
	TObjectPtr<UInputAction> TurnAction;

	UPROPERTY()
	TObjectPtr<UInputAction> CameraAction;

	void CreateInputAssets();
};
