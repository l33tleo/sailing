#pragma once

#include "CoreMinimal.h"
#include "GameFramework/HUD.h"
#include "SailingHUD.generated.h"

class AIslandActor;

UCLASS()
class SAILING_API ASailingHUD : public AHUD
{
	GENERATED_BODY()

public:
	virtual void BeginPlay() override;
	virtual void DrawHUD() override;

	// Show island discovery popup
	void ShowDiscoveryPopup(const FString& IslandName);

	/** Kalles fra PlayerController ved museklikk når pause-meny er åpen. */
	void OnPauseMenuClick(float X, float Y);

	/** Vindstyrke (spill-enheter) × denne faktoren = m/s (f.eks. 0.01 → 1000 gir 10 m/s). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HUD|Units", meta = (ClampMin = "0.0001"))
	float WindStrengthToMs = 0.01f;

	/** Fart (spill-enheter/s) × denne faktoren = knop (f.eks. 0.00625 → 800 gir 5 kn). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HUD|Units", meta = (ClampMin = "0.0001"))
	float SpeedToKnots = 0.00625f;

	// Oversiktskart
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HUD|Map", meta = (ClampMin = "1000"))
	float MapWorldRadius = 15000.0f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HUD|Map", meta = (ClampMin = "50"))
	float MapSizePixels = 220.0f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HUD|Map")
	float MapOffsetRight = 20.0f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HUD|Map")
	float MapOffsetBottom = 20.0f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HUD|Map")
	FLinearColor MapSeaColor = FLinearColor(0.0f, 0.08f, 0.2f, 0.85f);
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HUD|Map")
	FLinearColor MapIslandColor = FLinearColor(0.2f, 0.9f, 0.3f, 1.0f);
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HUD|Map")
	FLinearColor MapPlayerColor = FLinearColor(1.0f, 0.9f, 0.2f, 1.0f);

private:
	// Discovery popup state
	FString CurrentDiscoveryName;
	float DiscoveryPopupTimer = 0.0f;
	float DiscoveryPopupDuration = 4.0f;
	bool bShowingDiscoveryPopup = false;

	// Bind to island discoveries
	void BindToIslandDiscoveries();

	UFUNCTION()
	void OnIslandDiscovered(AIslandActor* Island, const FString& IslandName);

	// Draw compass and wind indicator
	void DrawCompass();

	// Draw wind and speed info panel
	void DrawWindAndSpeed();

	// Draw discovery popup
	void DrawDiscoveryPopup(float DeltaTime);

	// Draw discovery counter
	void DrawDiscoveryCounter();

	// Oversiktskart (spiller sentrum, oppdagede øyer som prikker)
	void DrawOverviewMap();

	void DrawPauseMenu();
	bool PauseMenuButtonHit(float X, float Y, float Bx, float By, float Bw, float Bh) const;
	float PauseCenterX = 0.0f;
	float PauseCenterY = 0.0f;
	float PauseButtonW = 260.0f;
	float PauseButtonH = 44.0f;
	float PauseButtonSpacing = 12.0f;
	float PauseResumeY = 0.0f;
	float PauseSaveY = 0.0f;
	float PauseQuitY = 0.0f;
};
