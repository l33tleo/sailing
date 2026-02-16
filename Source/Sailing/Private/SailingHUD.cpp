#include "SailingHUD.h"
#include "WindActor.h"
#include "SailboatPawn.h"
#include "IslandActor.h"
#include "SailingGameMode.h"
#include "SailingPlayerController.h"
#include "SaveGameSailing.h"
#include "Systems/SailingCoreSubsystems.h"
#include "UI/SailingHUDOverlayWidget.h"
#include "UI/PortMissionBoardWidget.h"
#include "Engine/Canvas.h"
#include "Engine/Font.h"
#include "Kismet/GameplayStatics.h"

void ASailingHUD::BeginPlay()
{
	Super::BeginPlay();
	EnsureOverlayWidget();
	EnsurePortMissionBoardWidget();

	// Defer binding to allow islands to spawn
	FTimerHandle TimerHandle;
	GetWorldTimerManager().SetTimer(TimerHandle, this, &ASailingHUD::BindToIslandDiscoveries, 1.0f, false);
}

void ASailingHUD::EnsureOverlayWidget()
{
	if (OverlayWidget || !OverlayWidgetClass || !PlayerOwner)
	{
		return;
	}

	OverlayWidget = CreateWidget<USailingHUDOverlayWidget>(PlayerOwner, OverlayWidgetClass);
	if (OverlayWidget)
	{
		OverlayWidget->AddToViewport(1);
	}
}

void ASailingHUD::EnsurePortMissionBoardWidget()
{
	if (PortMissionBoardWidget || !PortMissionBoardWidgetClass || !PlayerOwner)
	{
		return;
	}

	PortMissionBoardWidget = CreateWidget<UPortMissionBoardWidget>(PlayerOwner, PortMissionBoardWidgetClass);
	if (PortMissionBoardWidget)
	{
		PortMissionBoardWidget->OnAcceptMissionRequested.AddDynamic(this, &ASailingHUD::HandleMissionAcceptedRequest);
		PortMissionBoardWidget->OnCloseRequested.AddDynamic(this, &ASailingHUD::HandleMissionBoardCloseRequest);
		PortMissionBoardWidget->AddToViewport(2);
		PortMissionBoardWidget->SetVisibility(ESlateVisibility::Hidden);
	}
}

void ASailingHUD::ShowPortMissionBoard(FName PortId, const FText& PortDisplayName,
	const TArray<FName>& OfferedMissionIds, FName CurrentMissionId)
{
	EnsurePortMissionBoardWidget();
	if (!PortMissionBoardWidget)
	{
		return;
	}

	FPortMissionBoardData Data;
	Data.PortId = PortId;
	Data.PortDisplayName = PortDisplayName;
	Data.OfferedMissionIds = OfferedMissionIds;
	Data.CurrentMissionId = CurrentMissionId;
	PortMissionBoardWidget->PushMissionBoardData(Data);
	PortMissionBoardWidget->SetVisibility(ESlateVisibility::Visible);

	LastMissionBoardPortId = PortId;
	LastMissionBoardPortDisplayName = PortDisplayName;
	LastMissionBoardOfferedIds = OfferedMissionIds;

	GetWorldTimerManager().ClearTimer(MissionBoardHideTimer);
	GetWorldTimerManager().SetTimer(MissionBoardHideTimer, this, &ASailingHUD::HidePortMissionBoard, 5.0f, false);
}

bool ASailingHUD::AcceptMissionFromBoard(FName MissionId)
{
	if (MissionId.IsNone())
	{
		return false;
	}

	if (LastMissionBoardOfferedIds.Num() > 0 && !LastMissionBoardOfferedIds.Contains(MissionId))
	{
		return false;
	}

	UGameInstance* GI = GetWorld() ? GetWorld()->GetGameInstance() : nullptr;
	if (!GI)
	{
		return false;
	}

	UMissionSubsystem* MissionSubsystem = GI->GetSubsystem<UMissionSubsystem>();
	if (!MissionSubsystem)
	{
		return false;
	}

	const bool bActivated = MissionSubsystem->ActivateMissionFromCandidates({ MissionId }, false);
	if (bActivated)
	{
		ShowDiscoveryPopup(FString::Printf(TEXT("Oppdrag valgt: %s"), *MissionId.ToString()));
		if (ASailingGameMode* GM = Cast<ASailingGameMode>(UGameplayStatics::GetGameMode(GetWorld())))
		{
			GM->SaveGame_();
		}
		CloseMissionBoard();
	}
	return bActivated;
}

void ASailingHUD::CloseMissionBoard()
{
	HidePortMissionBoard();
	LastMissionBoardOfferedIds.Reset();
	LastMissionBoardPortId = NAME_None;
	LastMissionBoardPortDisplayName = FText::GetEmpty();
}

void ASailingHUD::HidePortMissionBoard()
{
	if (PortMissionBoardWidget)
	{
		PortMissionBoardWidget->SetVisibility(ESlateVisibility::Hidden);
	}
}

void ASailingHUD::HandleMissionAcceptedRequest(FName MissionId)
{
	AcceptMissionFromBoard(MissionId);
}

void ASailingHUD::HandleMissionBoardCloseRequest()
{
	CloseMissionBoard();
}

void ASailingHUD::PushOverlayData(int32 DiscoveredIslands, int32 Credits, FName ActiveMissionId,
	const FText& ActiveMissionTitle, float ObjectiveDistanceMeters, int32 BoatConditionPercent,
	float ObjectiveBearingDegrees)
{
	EnsureOverlayWidget();
	if (!OverlayWidget)
	{
		return;
	}

	FSailingHUDOverlayData OverlayData;
	OverlayData.DiscoveredIslands = DiscoveredIslands;
	OverlayData.Credits = Credits;
	OverlayData.ActiveMissionId = ActiveMissionId;
	OverlayData.ActiveMissionTitle = ActiveMissionTitle;
	OverlayData.ObjectiveDistanceMeters = ObjectiveDistanceMeters;
	OverlayData.BoatConditionPercent = BoatConditionPercent;
	OverlayData.ObjectiveBearingDegrees = ObjectiveBearingDegrees;
	OverlayWidget->PushOverlayData(OverlayData);
}

void ASailingHUD::BindToIslandDiscoveries()
{
	TArray<AActor*> Islands;
	UGameplayStatics::GetAllActorsOfClass(GetWorld(), AIslandActor::StaticClass(), Islands);

	for (AActor* Actor : Islands)
	{
		if (AIslandActor* Island = Cast<AIslandActor>(Actor))
		{
			Island->OnDiscovered.AddDynamic(this, &ASailingHUD::OnIslandDiscovered);
		}
	}

	FTimerHandle TimerHandle;
	GetWorldTimerManager().SetTimer(TimerHandle, this, &ASailingHUD::BindToIslandDiscoveries, 2.0f, false);
}

void ASailingHUD::OnIslandDiscovered(AIslandActor* Island, const FString& IslandName)
{
	ShowDiscoveryPopup(IslandName);
}

void ASailingHUD::ShowDiscoveryPopup(const FString& IslandName)
{
	CurrentDiscoveryName = IslandName;
	DiscoveryPopupTimer = DiscoveryPopupDuration;
	bShowingDiscoveryPopup = true;
}

void ASailingHUD::DrawHUD()
{
	Super::DrawHUD();

	if (!Canvas) return;

	ASailingPlayerController* PC = Cast<ASailingPlayerController>(PlayerOwner);
	if (PC && PC->IsPauseMenuShown())
	{
		DrawPauseMenu();
		return;
	}

	DrawCompass();
	DrawWindAndSpeed();
	DrawOverviewMap();
	DrawDiscoveryCounter();

	float DeltaTime = GetWorld()->GetDeltaSeconds();
	if (bShowingDiscoveryPopup)
	{
		DrawDiscoveryPopup(DeltaTime);
	}
}

void ASailingHUD::DrawCompass()
{
	APawn* Pawn = GetOwningPawn();
	if (!Pawn) return;

	AWindActor* Wind = nullptr;
	TArray<AActor*> Found;
	UGameplayStatics::GetAllActorsOfClass(GetWorld(), AWindActor::StaticClass(), Found);
	if (Found.Num() > 0)
	{
		Wind = Cast<AWindActor>(Found[0]);
	}

	// Compass position and size - BIG and visible
	float CenterX = Canvas->ClipX * 0.5f;
	float CenterY = 90.0f;
	float Radius = 65.0f;

	// Dark background circle
	DrawRect(FLinearColor(0.0f, 0.05f, 0.15f, 0.7f),
		CenterX - Radius - 10, CenterY - Radius - 10,
		(Radius + 10) * 2, (Radius + 10) * 2);

	// Compass ring - thick white circle
	const int32 Segments = 48;
	for (int32 i = 0; i < Segments; ++i)
	{
		float Angle1 = (float)i / Segments * 2.0f * PI;
		float Angle2 = (float)(i + 1) / Segments * 2.0f * PI;
		// Draw 3 lines close together for thickness
		for (float Offset = -1.0f; Offset <= 1.0f; Offset += 1.0f)
		{
			DrawLine(
				CenterX + FMath::Cos(Angle1) * (Radius + Offset),
				CenterY + FMath::Sin(Angle1) * (Radius + Offset),
				CenterX + FMath::Cos(Angle2) * (Radius + Offset),
				CenterY + FMath::Sin(Angle2) * (Radius + Offset),
				FLinearColor(0.7f, 0.8f, 0.9f, 0.9f),
				1.0f);
		}
	}

	// N/S/E/W labels - relative to boat heading
	// Offset by -PI/2 so that "ahead" maps to screen-up (matching the forward triangle)
	float BoatYaw = FMath::DegreesToRadians(Pawn->GetActorRotation().Yaw);

	struct FCardinal { const TCHAR* Label; float WorldAngle; FLinearColor Color; };
	FCardinal Cardinals[] = {
		{ TEXT("N"), 0.0f, FLinearColor::Red },
		{ TEXT("E"), PI * 0.5f, FLinearColor::White },
		{ TEXT("S"), PI, FLinearColor::White },
		{ TEXT("W"), PI * 1.5f, FLinearColor::White }
	};

	for (const auto& C : Cardinals)
	{
		float Angle = C.WorldAngle - BoatYaw - PI * 0.5f;
		float LabelX = CenterX + FMath::Cos(Angle) * (Radius + 18.0f) - 6.0f;
		float LabelY = CenterY + FMath::Sin(Angle) * (Radius + 18.0f) - 8.0f;
		DrawText(C.Label, C.Color, LabelX, LabelY, nullptr, 1.5f);
	}

	// Boat forward indicator - white triangle at top of compass
	{
		float FwdAngle = -PI * 0.5f;
		float FwdX = CenterX + FMath::Cos(FwdAngle) * (Radius + 2.0f);
		float FwdY = CenterY + FMath::Sin(FwdAngle) * (Radius + 2.0f);
		float TriSize = 8.0f;
		// Draw filled triangle pointing inward
		for (float f = 0.0f; f <= 1.0f; f += 0.1f)
		{
			float HalfW = TriSize * (1.0f - f);
			float Y = FwdY - TriSize + f * TriSize * 2.0f;
			DrawRect(FLinearColor::White, FwdX - HalfW, Y, HalfW * 2.0f, 1.5f);
		}
	}

	// Wind direction arrow - large filled yellow arrow
	if (Wind)
	{
		FVector WindDir = Wind->GetWindDirection();
		FVector BoatFwd = Pawn->GetActorForwardVector();
		// Offset by -PI/2 so that "aligned with boat forward" maps to screen-up
		float WindAngle = FMath::Atan2(WindDir.Y, WindDir.X) - FMath::Atan2(BoatFwd.Y, BoatFwd.X) - PI * 0.5f;

		FLinearColor WindColor(1.0f, 0.85f, 0.0f, 1.0f);
		FLinearColor WindColorDark(0.8f, 0.6f, 0.0f, 1.0f);

		// Arrow shaft - draw as filled rectangle along the wind direction
		float ShaftLen = Radius * 0.55f;
		float ShaftHalfWidth = 4.0f;
		float CosA = FMath::Cos(WindAngle);
		float SinA = FMath::Sin(WindAngle);
		float PerpX = -SinA;
		float PerpY = CosA;

		// Draw shaft as many short segments for fill
		int32 ShaftSteps = 20;
		for (int32 s = 0; s < ShaftSteps; ++s)
		{
			float T0 = (float)s / ShaftSteps;
			float T1 = (float)(s + 1) / ShaftSteps;
			float X0 = CenterX + CosA * ShaftLen * T0;
			float Y0 = CenterY + SinA * ShaftLen * T0;
			float X1 = CenterX + CosA * ShaftLen * T1;
			float Y1 = CenterY + SinA * ShaftLen * T1;
			for (float w = -ShaftHalfWidth; w <= ShaftHalfWidth; w += 1.0f)
			{
				DrawLine(X0 + PerpX * w, Y0 + PerpY * w,
					X1 + PerpX * w, Y1 + PerpY * w,
					WindColor, 1.0f);
			}
		}

		// Large filled arrowhead
		float HeadLen = 25.0f;
		float HeadHalfWidth = 14.0f;
		float TipX = CenterX + CosA * (Radius - 5.0f);
		float TipY = CenterY + SinA * (Radius - 5.0f);
		float BaseX = TipX - CosA * HeadLen;
		float BaseY = TipY - SinA * HeadLen;

		// Fill arrowhead with lines from base edges to tip
		int32 HeadSteps = 30;
		for (int32 h = 0; h <= HeadSteps; ++h)
		{
			float T = (float)h / HeadSteps;
			float HalfW = HeadHalfWidth * (1.0f - T);
			float PtX = BaseX + (TipX - BaseX) * T;
			float PtY = BaseY + (TipY - BaseY) * T;
			DrawLine(PtX + PerpX * HalfW, PtY + PerpY * HalfW,
				PtX - PerpX * HalfW, PtY - PerpY * HalfW,
				WindColor, 1.5f);
		}

		// Arrowhead outline for definition
		float LeftX = BaseX + PerpX * HeadHalfWidth;
		float LeftY = BaseY + PerpY * HeadHalfWidth;
		float RightX = BaseX - PerpX * HeadHalfWidth;
		float RightY = BaseY - PerpY * HeadHalfWidth;
		DrawLine(TipX, TipY, LeftX, LeftY, WindColorDark, 2.0f);
		DrawLine(TipX, TipY, RightX, RightY, WindColorDark, 2.0f);
		DrawLine(LeftX, LeftY, RightX, RightY, WindColorDark, 2.0f);

		// "VIND" label at the arrow base
		float LabelX = CenterX - CosA * (Radius * 0.15f) - 14.0f;
		float LabelY = CenterY - SinA * (Radius * 0.15f) - 8.0f;
		DrawText(TEXT("VIND"), WindColor, LabelX, LabelY, nullptr, 1.0f);

		// Small circle at center
		for (int32 i = 0; i < 24; ++i)
		{
			float A1 = (float)i / 24.0f * 2.0f * PI;
			float A2 = (float)(i + 1) / 24.0f * 2.0f * PI;
			float R = 5.0f;
			DrawLine(CenterX + FMath::Cos(A1) * R, CenterY + FMath::Sin(A1) * R,
				CenterX + FMath::Cos(A2) * R, CenterY + FMath::Sin(A2) * R,
				WindColor, 2.0f);
		}
	}

	// Mission objective marker on compass ring (distinct from wind arrow)
	if (UGameInstance* GI = GetWorld()->GetGameInstance())
	{
		if (UMissionSubsystem* MissionSubsystem = GI->GetSubsystem<UMissionSubsystem>())
		{
			FVector ObjectiveLocation = FVector::ZeroVector;
			if (MissionSubsystem->GetActiveMissionObjectiveLocation(ObjectiveLocation))
			{
				const FVector ToObjective = ObjectiveLocation - Pawn->GetActorLocation();
				const FVector ToObjectiveFlat(ToObjective.X, ToObjective.Y, 0.0f);
				if (!ToObjectiveFlat.IsNearlyZero())
				{
					const FVector BoatFwd = Pawn->GetActorForwardVector();
					const float ObjectiveAngle = FMath::Atan2(ToObjectiveFlat.Y, ToObjectiveFlat.X) - FMath::Atan2(BoatFwd.Y, BoatFwd.X) - PI * 0.5f;

					const FLinearColor MissionColor(1.0f, 0.3f, 0.85f, 1.0f);
					const float MarkerRadius = Radius + 2.0f;
					const float MarkerX = CenterX + FMath::Cos(ObjectiveAngle) * MarkerRadius;
					const float MarkerY = CenterY + FMath::Sin(ObjectiveAngle) * MarkerRadius;
					const float MarkerSize = 7.0f;

					// Diamond marker
					DrawLine(MarkerX, MarkerY - MarkerSize, MarkerX + MarkerSize, MarkerY, MissionColor, 2.0f);
					DrawLine(MarkerX + MarkerSize, MarkerY, MarkerX, MarkerY + MarkerSize, MissionColor, 2.0f);
					DrawLine(MarkerX, MarkerY + MarkerSize, MarkerX - MarkerSize, MarkerY, MissionColor, 2.0f);
					DrawLine(MarkerX - MarkerSize, MarkerY, MarkerX, MarkerY - MarkerSize, MissionColor, 2.0f);

					const float DistanceMeters = ToObjectiveFlat.Size() * 0.01f;
					const FString ObjectiveText = FString::Printf(TEXT("MAL %.0fm"), DistanceMeters);
					DrawText(ObjectiveText, MissionColor, MarkerX + 10.0f, MarkerY - 8.0f, nullptr, 0.8f);
				}
			}
		}
	}
}

void ASailingHUD::DrawWindAndSpeed()
{
	APawn* Pawn = GetOwningPawn();
	if (!Pawn) return;

	AWindActor* Wind = nullptr;
	TArray<AActor*> Found;
	UGameplayStatics::GetAllActorsOfClass(GetWorld(), AWindActor::StaticClass(), Found);
	if (Found.Num() > 0)
	{
		Wind = Cast<AWindActor>(Found[0]);
	}

	// Info panel below compass
	float PanelX = Canvas->ClipX * 0.5f - 100.0f;
	float PanelY = 175.0f;
	float PanelW = 200.0f;
	float PanelH = 85.0f;

	// Dark background
	DrawRect(FLinearColor(0.0f, 0.05f, 0.15f, 0.7f), PanelX, PanelY, PanelW, PanelH);

	// Wind strength and point of sail (enheter: m/s og knop)
	const ASailboatPawn* Boat = Cast<ASailboatPawn>(Pawn);
	if (Wind)
	{
		float WindMs = Wind->GetWindStrength() * WindStrengthToMs;
		FString WindText = FString::Printf(TEXT("VIND: %.1f m/s"), WindMs);
		DrawText(WindText, FLinearColor(0.5f, 0.9f, 1.0f, 1.0f),
			PanelX + 15.0f, PanelY + 8.0f, nullptr, 1.3f);

		// Point of sail label
		if (Boat)
		{
			FVector BoatFwd = Pawn->GetActorForwardVector();
			FVector WindDir = Wind->GetWindDirection();
			// WindDir peker mot vindkilden: 0° = mot vind, 180° = med vind
			float CosA = FVector::DotProduct(BoatFwd, WindDir);
			float Angle = FMath::RadiansToDegrees(FMath::Acos(FMath::Clamp(CosA, -1.0f, 1.0f)));

			FString PointOfSail;
			FLinearColor PointColor;
			if (Angle < Boat->NoGoZoneAngle)
			{
				PointOfSail = TEXT("I JERN");
				PointColor = FLinearColor(1.0f, 0.3f, 0.3f, 1.0f);
			}
			else if (Angle < 60.0f)
			{
				PointOfSail = TEXT("BIDEVIND");
				PointColor = FLinearColor(1.0f, 0.7f, 0.3f, 1.0f);
			}
			else if (Angle < 80.0f)
			{
				PointOfSail = TEXT("SLOR");
				PointColor = FLinearColor(1.0f, 1.0f, 0.3f, 1.0f);
			}
			else if (Angle < 100.0f)
			{
				PointOfSail = TEXT("HALV VIND");
				PointColor = FLinearColor(0.2f, 1.0f, 0.2f, 1.0f);
			}
			else if (Angle < 135.0f)
			{
				PointOfSail = TEXT("ROMSKJOETS");
				PointColor = FLinearColor(1.0f, 1.0f, 0.3f, 1.0f);
			}
			else
			{
				PointOfSail = TEXT("LENS");
				PointColor = FLinearColor(1.0f, 0.7f, 0.3f, 1.0f);
			}

			DrawText(PointOfSail, PointColor,
				PanelX + 15.0f, PanelY + 32.0f, nullptr, 1.3f);
		}
	}

	// Speed (knop)
	if (Boat)
	{
		float SpeedKn = Boat->CurrentSpeed * SpeedToKnots;
		FLinearColor SpeedColor;
		if (Boat->CurrentSpeed > 500.0f)
			SpeedColor = FLinearColor(0.2f, 1.0f, 0.2f, 1.0f); // Green = fast
		else if (Boat->CurrentSpeed > 100.0f)
			SpeedColor = FLinearColor(1.0f, 1.0f, 0.3f, 1.0f); // Yellow = medium
		else
			SpeedColor = FLinearColor(1.0f, 0.4f, 0.3f, 1.0f); // Red = slow/stopped

		FString SpeedText = FString::Printf(TEXT("FART: %.1f kn"), SpeedKn);
		DrawText(SpeedText, SpeedColor,
			PanelX + 15.0f, PanelY + 56.0f, nullptr, 1.3f);
	}
}

void ASailingHUD::DrawDiscoveryPopup(float DeltaTime)
{
	DiscoveryPopupTimer -= DeltaTime;
	if (DiscoveryPopupTimer <= 0.0f)
	{
		bShowingDiscoveryPopup = false;
		return;
	}

	float Alpha = 1.0f;
	if (DiscoveryPopupTimer < 1.0f)
	{
		Alpha = DiscoveryPopupTimer;
	}
	float TimeElapsed = DiscoveryPopupDuration - DiscoveryPopupTimer;
	if (TimeElapsed < 0.5f)
	{
		Alpha = TimeElapsed * 2.0f;
	}

	float CenterX = Canvas->ClipX * 0.5f;
	float CenterY = Canvas->ClipY * 0.3f;

	float BoxWidth = 350.0f;
	float BoxHeight = 90.0f;
	FLinearColor BoxColor(0.0f, 0.1f, 0.2f, 0.85f * Alpha);
	DrawRect(BoxColor, CenterX - BoxWidth * 0.5f, CenterY - BoxHeight * 0.5f, BoxWidth, BoxHeight);

	// Gold border
	FLinearColor BorderColor(0.9f, 0.7f, 0.2f, Alpha);
	float BT = 3.0f;
	DrawRect(BorderColor, CenterX - BoxWidth * 0.5f, CenterY - BoxHeight * 0.5f, BoxWidth, BT);
	DrawRect(BorderColor, CenterX - BoxWidth * 0.5f, CenterY + BoxHeight * 0.5f - BT, BoxWidth, BT);
	DrawRect(BorderColor, CenterX - BoxWidth * 0.5f, CenterY - BoxHeight * 0.5f, BT, BoxHeight);
	DrawRect(BorderColor, CenterX + BoxWidth * 0.5f - BT, CenterY - BoxHeight * 0.5f, BT, BoxHeight);

	// Header
	FLinearColor HeaderColor(0.9f, 0.7f, 0.2f, Alpha);
	DrawText(TEXT("NY OY OPPDAGET!"), HeaderColor,
		CenterX - 80.0f, CenterY - 30.0f, nullptr, 1.5f);

	// Island name
	FLinearColor NameColor(1.0f, 1.0f, 1.0f, Alpha);
	float NameOffset = CurrentDiscoveryName.Len() * 5.0f;
	DrawText(CurrentDiscoveryName, NameColor,
		CenterX - NameOffset, CenterY + 5.0f, nullptr, 1.8f);
}

void ASailingHUD::DrawOverviewMap()
{
	APawn* Pawn = GetOwningPawn();
	if (!Pawn || !Canvas) return;

	ASailingGameMode* GM = Cast<ASailingGameMode>(UGameplayStatics::GetGameMode(GetWorld()));
	if (!GM) return;
	USaveGameSailing* SaveGame = GM->GetSaveGame();
	if (!SaveGame) return;

	FVector PlayerLoc = Pawn->GetActorLocation();
	const float PlayerX = PlayerLoc.X;
	const float PlayerY = PlayerLoc.Y;

	// Kartpanel nederst til høyre
	const float MapLeft = Canvas->ClipX - MapOffsetRight - MapSizePixels;
	const float MapTop = Canvas->ClipY - MapOffsetBottom - MapSizePixels;
	const float MapCenterX = MapLeft + MapSizePixels * 0.5f;
	const float MapCenterY = MapTop + MapSizePixels * 0.5f;
	const float Scale = (MapSizePixels * 0.5f) / FMath::Max(1.0f, MapWorldRadius);

	// 1. Bakgrunn (hav)
	DrawRect(MapSeaColor, MapLeft, MapTop, MapSizePixels, MapSizePixels);

	// 2. Oppdagede øyer innenfor radius
	for (const auto& Pair : SaveGame->DiscoveredIslands)
	{
		const FVector& Loc = Pair.Value.WorldLocation;
		float Dx = Loc.X - PlayerX;
		float Dy = Loc.Y - PlayerY;
		if (FMath::Sqrt(Dx * Dx + Dy * Dy) > MapWorldRadius) continue;

		float Px = MapCenterX + Dx * Scale;
		float Py = MapCenterY - Dy * Scale; // nord = +Y i verden = opp på skjerm
		const float DotSize = 4.0f;
		DrawRect(MapIslandColor, Px - DotSize * 0.5f, Py - DotSize * 0.5f, DotSize, DotSize);
	}

	// 2b. Aktivt oppdragsmål (hvis lokasjonsbasert)
	if (UGameInstance* GI = GetWorld()->GetGameInstance())
	{
		// Port markers
		if (UWorldStreamingSubsystem* WorldSubsystem = GI->GetSubsystem<UWorldStreamingSubsystem>())
		{
			for (const FName& PortId : WorldSubsystem->GetRegisteredPortIds())
			{
				FVector PortLoc;
				if (!WorldSubsystem->GetPortLocation(PortId, PortLoc))
				{
					continue;
				}

				const float Dx = PortLoc.X - PlayerX;
				const float Dy = PortLoc.Y - PlayerY;
				if (FMath::Sqrt(Dx * Dx + Dy * Dy) > MapWorldRadius)
				{
					continue;
				}

				const float Px = MapCenterX + Dx * Scale;
				const float Py = MapCenterY - Dy * Scale;
				const float DotSize = 5.0f;
				DrawRect(MapPortColor, Px - DotSize * 0.5f, Py - DotSize * 0.5f, DotSize, DotSize);
			}
		}

		if (UMissionSubsystem* MissionSubsystem = GI->GetSubsystem<UMissionSubsystem>())
		{
			FVector ObjectiveLoc;
			if (MissionSubsystem->GetActiveMissionObjectiveLocation(ObjectiveLoc))
			{
				const float Dx = ObjectiveLoc.X - PlayerX;
				const float Dy = ObjectiveLoc.Y - PlayerY;
				const float Dist = FMath::Sqrt(Dx * Dx + Dy * Dy);
				if (Dist <= MapWorldRadius)
				{
					const float Px = MapCenterX + Dx * Scale;
					const float Py = MapCenterY - Dy * Scale;
					DrawLine(MapCenterX, MapCenterY, Px, Py, FLinearColor(MapMissionColor.R, MapMissionColor.G, MapMissionColor.B, 0.7f), 1.2f);
					const float DotSize = 7.0f;
					DrawRect(MapMissionColor, Px - DotSize * 0.5f, Py - DotSize * 0.5f, DotSize, DotSize);
				}
			}
		}
	}

	// 3. Spiller som liten trekant (retning fra GetActorForwardVector)
	FVector Forward = Pawn->GetActorForwardVector();
	float Angle = FMath::Atan2(Forward.X, Forward.Y); // vinkel fra nord (verden +Y)
	const float TriR = 8.0f;
	const float TriW = 4.0f;
	float TipX = MapCenterX + FMath::Sin(Angle) * TriR;
	float TipY = MapCenterY - FMath::Cos(Angle) * TriR;
	float B1X = MapCenterX - FMath::Sin(Angle) * TriR + FMath::Cos(Angle) * TriW;
	float B1Y = MapCenterY + FMath::Cos(Angle) * TriR + FMath::Sin(Angle) * TriW;
	float B2X = MapCenterX - FMath::Sin(Angle) * TriR - FMath::Cos(Angle) * TriW;
	float B2Y = MapCenterY + FMath::Cos(Angle) * TriR - FMath::Sin(Angle) * TriW;
	DrawLine(TipX, TipY, B1X, B1Y, MapPlayerColor, 2.0f);
	DrawLine(TipX, TipY, B2X, B2Y, MapPlayerColor, 2.0f);
	DrawLine(B1X, B1Y, B2X, B2Y, MapPlayerColor, 2.0f);
}

void ASailingHUD::DrawDiscoveryCounter()
{
	ASailingGameMode* GM = Cast<ASailingGameMode>(UGameplayStatics::GetGameMode(GetWorld()));
	if (!GM) return;

	USaveGameSailing* SaveGame = GM->GetSaveGame();
	if (!SaveGame) return;

	// Top-left with background
	float BoxX = 10.0f;
	float BoxY = 10.0f;
	float BoxW = 320.0f;
	float BoxH = 104.0f;
	DrawRect(FLinearColor(0.0f, 0.05f, 0.15f, 0.7f), BoxX, BoxY, BoxW, BoxH);

	FString CounterText = FString::Printf(TEXT("OYER: %d oppdaget"), SaveGame->TotalIslandsDiscovered);
	DrawText(CounterText, FLinearColor(0.5f, 0.9f, 1.0f, 1.0f),
		BoxX + 10.0f, BoxY + 8.0f, nullptr, 1.2f);

	int32 Credits = 0;
	int32 BoatCondition = 100;
	FName ActiveMissionId = NAME_None;
	FText ActiveMissionTitle = FText::GetEmpty();
	float ObjectiveDistanceMeters = -1.0f;
	float ObjectiveBearingDegrees = -999.0f;
	if (UGameInstance* GI = GetWorld()->GetGameInstance())
	{
		if (UEconomySubsystem* EconomySubsystem = GI->GetSubsystem<UEconomySubsystem>())
		{
			Credits = EconomySubsystem->GetCredits();
			BoatCondition = EconomySubsystem->GetBoatConditionPercent();
		}
		if (UMissionSubsystem* MissionSubsystem = GI->GetSubsystem<UMissionSubsystem>())
		{
			ActiveMissionId = MissionSubsystem->GetActiveMissionId();
			ActiveMissionTitle = MissionSubsystem->GetActiveMissionDisplayName();
			FVector ObjectiveLocation;
			if (MissionSubsystem->GetActiveMissionObjectiveLocation(ObjectiveLocation))
			{
				if (APawn* Pawn = GetOwningPawn())
				{
					const FVector ToObjective = ObjectiveLocation - Pawn->GetActorLocation();
					const FVector ToObjectiveFlat(ToObjective.X, ToObjective.Y, 0.0f);
					const float DistCm = ToObjectiveFlat.Size();
					ObjectiveDistanceMeters = DistCm * 0.01f;

					if (!ToObjectiveFlat.IsNearlyZero())
					{
						const FVector BoatFwd = Pawn->GetActorForwardVector();
						const float RelativeBearingRad = FMath::Atan2(ToObjectiveFlat.Y, ToObjectiveFlat.X) - FMath::Atan2(BoatFwd.Y, BoatFwd.X);
						ObjectiveBearingDegrees = FMath::UnwindDegrees(FMath::RadiansToDegrees(RelativeBearingRad));
					}
				}
			}
		}
	}

	const FString CreditsText = FString::Printf(TEXT("KREDITTER: %d"), Credits);
	DrawText(CreditsText, FLinearColor(1.0f, 0.9f, 0.35f, 1.0f),
		BoxX + 10.0f, BoxY + 32.0f, nullptr, 1.2f);

	const FString MissionText = ActiveMissionId.IsNone()
		? FString(TEXT("AKTIVT OPPDRAG: Ingen"))
		: FString::Printf(TEXT("AKTIVT OPPDRAG: %s"), *ActiveMissionId.ToString());
	DrawText(MissionText, FLinearColor(0.7f, 1.0f, 0.8f, 1.0f),
		BoxX + 10.0f, BoxY + 56.0f, nullptr, 1.0f);

	if (!ActiveMissionTitle.IsEmpty())
	{
		const FString TitleText = FString::Printf(TEXT("MÅL: %s"), *ActiveMissionTitle.ToString());
		DrawText(TitleText, FLinearColor(0.8f, 0.9f, 1.0f, 1.0f),
			BoxX + 170.0f, BoxY + 56.0f, nullptr, 0.9f);
	}

	if (ObjectiveDistanceMeters >= 0.0f)
	{
		const FString DistText = FString::Printf(TEXT("DIST: %.0f m"), ObjectiveDistanceMeters);
		DrawText(DistText, FLinearColor(1.0f, 0.95f, 0.55f, 1.0f),
			BoxX + 170.0f, BoxY + 32.0f, nullptr, 1.0f);
	}

	if (ObjectiveBearingDegrees > -999.0f && ObjectiveBearingDegrees <= 999.0f)
	{
		const FString BearingText = FString::Printf(TEXT("PEILING: %+0.0f°"), ObjectiveBearingDegrees);
		DrawText(BearingText, FLinearColor(0.95f, 0.75f, 1.0f, 1.0f),
			BoxX + 170.0f, BoxY + 80.0f, nullptr, 0.95f);
	}

	const FLinearColor ConditionColor = BoatCondition > 70
		? FLinearColor(0.3f, 1.0f, 0.4f, 1.0f)
		: (BoatCondition > 35 ? FLinearColor(1.0f, 0.9f, 0.3f, 1.0f) : FLinearColor(1.0f, 0.35f, 0.35f, 1.0f));
	const FString ConditionText = FString::Printf(TEXT("SKROGTILSTAND: %d%%"), BoatCondition);
	DrawText(ConditionText, ConditionColor, BoxX + 10.0f, BoxY + 80.0f, nullptr, 1.0f);

	PushOverlayData(SaveGame->TotalIslandsDiscovered, Credits, ActiveMissionId, ActiveMissionTitle, ObjectiveDistanceMeters, BoatCondition, ObjectiveBearingDegrees);
}

bool ASailingHUD::PauseMenuButtonHit(float X, float Y, float Bx, float By, float Bw, float Bh) const
{
	return X >= Bx && X <= Bx + Bw && Y >= By && Y <= By + Bh;
}

void ASailingHUD::DrawPauseMenu()
{
	PauseCenterX = Canvas->ClipX * 0.5f;
	PauseCenterY = Canvas->ClipY * 0.5f;
	const float TotalH = PauseButtonH * 3.0f + PauseButtonSpacing * 2.0f;
	float Y = PauseCenterY - TotalH * 0.5f;
	PauseResumeY = Y;
	PauseSaveY = Y + PauseButtonH + PauseButtonSpacing;
	PauseQuitY = Y + (PauseButtonH + PauseButtonSpacing) * 2.0f;
	const float Bx = PauseCenterX - PauseButtonW * 0.5f;

	// Halvtransparent bakgrunn
	DrawRect(FLinearColor(0.0f, 0.0f, 0.0f, 0.7f), 0.0f, 0.0f, Canvas->ClipX, Canvas->ClipY);
	DrawRect(FLinearColor(0.0f, 0.08f, 0.18f, 0.95f), Bx - 24.0f, PauseResumeY - 24.0f, PauseButtonW + 48.0f, TotalH + 48.0f);

	FLinearColor BtnColor(0.2f, 0.5f, 0.8f, 0.9f);
	DrawRect(BtnColor, Bx, PauseResumeY, PauseButtonW, PauseButtonH);
	DrawText(TEXT("Fortsett"), FLinearColor::White, Bx + 20.0f, PauseResumeY + 10.0f, nullptr, 1.4f);
	DrawRect(BtnColor, Bx, PauseSaveY, PauseButtonW, PauseButtonH);
	DrawText(TEXT("Lagre"), FLinearColor::White, Bx + 20.0f, PauseSaveY + 10.0f, nullptr, 1.4f);
	DrawRect(BtnColor, Bx, PauseQuitY, PauseButtonW, PauseButtonH);
	DrawText(TEXT("Avslutt til meny"), FLinearColor::White, Bx + 20.0f, PauseQuitY + 10.0f, nullptr, 1.4f);
}

void ASailingHUD::OnPauseMenuClick(float X, float Y)
{
	ASailingPlayerController* PC = Cast<ASailingPlayerController>(PlayerOwner);
	if (!PC || !PC->IsPauseMenuShown()) return;

	const float Bx = PauseCenterX - PauseButtonW * 0.5f;

	if (PauseMenuButtonHit(X, Y, Bx, PauseResumeY, PauseButtonW, PauseButtonH))
	{
		PC->ClosePauseMenu();
		return;
	}
	if (PauseMenuButtonHit(X, Y, Bx, PauseSaveY, PauseButtonW, PauseButtonH))
	{
		if (ASailingGameMode* GM = Cast<ASailingGameMode>(UGameplayStatics::GetGameMode(GetWorld())))
		{
			GM->SaveGame_();
		}
		return;
	}
	if (PauseMenuButtonHit(X, Y, Bx, PauseQuitY, PauseButtonW, PauseButtonH))
	{
		PC->ClosePauseMenu();
		UGameplayStatics::OpenLevel(this, FName(TEXT("MainMenu")));
	}
}
