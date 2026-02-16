#pragma once

#include "CoreMinimal.h"
#include "GameFramework/HUD.h"
#include "SailingHUD.generated.h"

class AIslandActor;
class UTexture2D;

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

	/** Fullskjerm-kart (M-tast) */
	void SetFullMapVisible(bool bVisible);
	void OnFullMapClick(float X, float Y);
	void OnFullMapDrag(float X, float Y);
	void OnFullMapDragDelta(float DeltaX, float DeltaY);
	void OnFullMapRelease();
	void OnFullMapScroll(float Delta);
	bool IsFullMapVisible() const { return bShowFullMap; }

	/** Vindstyrke (spill-enheter) × denne faktoren = m/s (f.eks. 0.01 → 1000 gir 10 m/s). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HUD|Units", meta = (ClampMin = "0.0001"))
	float WindStrengthToMs = 0.01f;

	/** Fart (spill-enheter/s) × denne faktoren = knop (f.eks. 0.00625 → 800 gir 5 kn). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HUD|Units", meta = (ClampMin = "0.0001"))
	float SpeedToKnots = 0.00625f;

	// Oversiktskart
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HUD|Map", meta = (ClampMin = "1000"))
	float MapWorldRadius = 1500000.0f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HUD|Map", meta = (ClampMin = "50"))
	float MapSizePixels = 300.0f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HUD|Map")
	float MapOffsetRight = 20.0f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HUD|Map")
	float MapOffsetBottom = 20.0f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HUD|Map")
	FLinearColor MapSeaColor = FLinearColor(0.05f, 0.2f, 0.5f, 0.9f);
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HUD|Map")
	FLinearColor MapIslandColor = FLinearColor(0.2f, 0.9f, 0.3f, 1.0f);
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HUD|Map")
	FLinearColor MapPlayerColor = FLinearColor(1.0f, 0.9f, 0.2f, 1.0f);
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HUD|Map")
	FLinearColor MapUndiscoveredIslandColor = FLinearColor(0.5f, 0.5f, 0.5f, 0.6f);

	/** Piksel-radius for rund radar-minikart */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HUD|Map", meta = (ClampMin = "30"))
	float RadarRadius = 120.0f;

	/** Kartverket chart texture for oversiktskart (fjordmodus). Hvis satt og bUseFjordMap, brukes som bakgrunn. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HUD|Map")
	TObjectPtr<UTexture2D> ChartTexture;

	/** WGS84 bbox som ChartTexture dekker: X=min_lon, Y=min_lat, Z=max_lon, W=max_lat. Samme som ved henting av kart. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HUD|Map")
	FVector4 ChartLonLatBox = FVector4(10.4f, 59.7f, 11.2f, 60.0f);

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

	// Rund radar-minikart (spiller sentrum, oppdagede øyer som prikker)
	void DrawRadarMinimap();

	// Fullskjerm-kart med pan/zoom
	void DrawFullMap();

	// Helpers for drawing circular elements
	void DrawCircleOutline(float CX, float CY, float R, FLinearColor Color, float Thickness = 1.0f, int32 Segments = 48);

	// Full map state
	bool bShowFullMap = false;
	FVector2D FullMapPanPixels = FVector2D::ZeroVector;      // Pan i skjermpiksler
	FVector2D DragStartPanPixels = FVector2D::ZeroVector;    // Snapshot ved drag-start
	float FullMapZoom = 1.0f;
	bool bIsDraggingMap = false;
	FVector2D DragStartMouse = FVector2D::ZeroVector;

	/** Øyer vi allerede har bunden OnDiscovered til (unngår dobbelt AddDynamic). */
	TSet<TWeakObjectPtr<AIslandActor>> BoundIslandDiscoveries;

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
