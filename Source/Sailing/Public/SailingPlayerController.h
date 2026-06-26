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

	/** Om fullskjerm-kartet vises (M-tast). */
	bool IsMapViewShown() const { return bShowMapView; }
	/** Lukk fullskjerm-kartet. */
	void CloseMapView();

	virtual void Tick(float DeltaTime) override;

protected:
	virtual void BeginPlay() override;

private:
	void TogglePauseMenu();
	void ToggleMapView();
	void OnPauseMenuClick();
	void OnMapMousePressed();
	void OnMapMouseReleased();
	void OnMapScrollUp();
	void OnMapScrollDown();
	bool bMapMouseDown = false;

	bool bShowPauseMenu = false;
	bool bShowMapView = false;
	UPROPERTY()
	TObjectPtr<UInputMappingContext> SailingMappingContext;

	UPROPERTY()
	TObjectPtr<UInputAction> TurnAction;

	UPROPERTY()
	TObjectPtr<UInputAction> CameraAction;

	void CreateInputAssets();
};
