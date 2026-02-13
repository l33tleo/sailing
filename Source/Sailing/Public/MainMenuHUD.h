#pragma once

#include "CoreMinimal.h"
#include "GameFramework/HUD.h"
#include "MainMenuHUD.generated.h"

UCLASS()
class SAILING_API AMainMenuHUD : public AHUD
{
	GENERATED_BODY()

public:
	virtual void DrawHUD() override;
	virtual void BeginPlay() override;

	/** Kalles fra PlayerController når museklikk detekteres (viewport-koordinater). */
	void OnMouseClick(float X, float Y);

private:
	void DrawMenu();
	bool CheckButtonHit(float X, float Y, float Bx, float By, float Bw, float Bh);

	float CenterX = 0.0f;
	float CenterY = 0.0f;
	float ButtonW = 280.0f;
	float ButtonH = 50.0f;
	float ButtonSpacing = 16.0f;
	float NewGameY = 0.0f;
	float ContinueY = 0.0f;
	float QuitY = 0.0f;
	bool bHasSave = false;
};
