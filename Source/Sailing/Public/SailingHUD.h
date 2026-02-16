#pragma once

#include "CoreMinimal.h"
#include "GameFramework/HUD.h"
#include "UI/PortMissionBoardWidget.h"
#include "SailingHUD.generated.h"

class AIslandActor;
class USailingHUDOverlayWidget;

UCLASS()
class SAILING_API ASailingHUD : public AHUD
{
	GENERATED_BODY()

public:
	virtual void BeginPlay() override;
	virtual void DrawHUD() override;

	// Show island discovery popup
	void ShowDiscoveryPopup(const FString& IslandName);

	/** Shows mission board data from current harbor interaction. */
	void ShowPortMissionBoard(FName PortId, const FText& PortDisplayName,
		bool bOfferMissionBoard,
		const TArray<FName>& OfferedMissionIds, FName CurrentMissionId,
		bool bMissionBoardOnCooldown, float CooldownRemainingSeconds,
		bool bAutoRepairAtPort, int32 RepairCostPerPercentPoint,
		bool bOfferUpgradeService, const TArray<FName>& OfferedUpgradeIds);

	UFUNCTION(BlueprintCallable, Category = "HUD|MissionBoard")
	bool AcceptMissionFromBoard(FName MissionId);

	UFUNCTION(BlueprintCallable, Category = "HUD|MissionBoard")
	bool RequestRepairFromBoard();

	UFUNCTION(BlueprintCallable, Category = "HUD|MissionBoard")
	bool RequestUpgradePurchaseFromBoard(FName UpgradeId);

	UFUNCTION(BlueprintCallable, Category = "HUD|MissionBoard")
	void CloseMissionBoard();

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
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HUD|Map")
	FLinearColor MapMissionColor = FLinearColor(1.0f, 0.8f, 0.1f, 1.0f);
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HUD|Map")
	FLinearColor MapPortColor = FLinearColor(0.8f, 0.6f, 1.0f, 1.0f);

	/** Optional UMG overlay class used in parallel while Canvas HUD is active. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HUD|UMG")
	TSubclassOf<USailingHUDOverlayWidget> OverlayWidgetClass;

	/** Optional mission board widget shown when docking at ports. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HUD|UMG")
	TSubclassOf<UPortMissionBoardWidget> PortMissionBoardWidgetClass;

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

	void EnsureOverlayWidget();
	void EnsurePortMissionBoardWidget();
	void HidePortMissionBoard();
	UFUNCTION()
	void HandleMissionAcceptedRequest(FName MissionId);
	UFUNCTION()
	void HandleMissionBoardCloseRequest();
	UFUNCTION()
	void HandleRepairRequest();
	UFUNCTION()
	void HandleUpgradePurchaseRequest(FName UpgradeId);
	void PushOverlayData(int32 DiscoveredIslands, int32 Credits, FName ActiveMissionId,
		const FText& ActiveMissionTitle, float ObjectiveDistanceMeters, int32 BoatConditionPercent,
		float ObjectiveBearingDegrees, FName LastVisitedPortId);

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

	UPROPERTY()
	TObjectPtr<USailingHUDOverlayWidget> OverlayWidget;

	UPROPERTY()
	TObjectPtr<UPortMissionBoardWidget> PortMissionBoardWidget;

	FPortMissionBoardData LastMissionBoardData;

	FTimerHandle MissionBoardHideTimer;
	TArray<FName> LastMissionBoardOfferedIds;
	FName LastMissionBoardPortId = NAME_None;
	FText LastMissionBoardPortDisplayName;
	bool bLastMissionBoardOnCooldown = false;
	float LastMissionBoardCooldownRemainingSeconds = 0.0f;
	bool bLastMissionBoardAutoRepairAtPort = true;
	int32 LastMissionBoardRepairCostPerPercentPoint = 1;
	bool bLastMissionBoardOfferUpgradeService = false;
	TArray<FName> LastMissionBoardOfferedUpgradeIds;
};
