#include "SailingHUD.h"
#include "WindActor.h"
#include "SailboatPawn.h"
#include "IslandActor.h"
#include "SailingGameMode.h"
#include "SailingPlayerController.h"
#include "SaveGameSailing.h"
#include "Engine/Canvas.h"
#include "Engine/Font.h"
#include "Engine/Texture2D.h"
#include "Kismet/GameplayStatics.h"

namespace
{
	// Oslo origin (same as fjord_data_from_geojson.py). World X = east (m), Y = north (m).
	constexpr float OriginLat = 59.91f;
	constexpr float OriginLon = 10.75f;
	constexpr float MetersPerDegLat = 111320.0f;
	inline float MetersPerDegLon() { return 111320.0f * FMath::Cos(FMath::DegreesToRadians(OriginLat)); }

	void WorldToLonLat(float WorldX, float WorldY, float& OutLon, float& OutLat)
	{
		OutLon = OriginLon + WorldX / MetersPerDegLon();
		OutLat = OriginLat + WorldY / MetersPerDegLat;
	}
}

void ASailingHUD::BeginPlay()
{
	Super::BeginPlay();

	// Auto-load Kartverket chart texture in fjord mode when not set
	if (!ChartTexture)
	{
		if (ASailingGameMode* GM = Cast<ASailingGameMode>(UGameplayStatics::GetGameMode(GetWorld())))
		{
			if (GM->bUseFjordMap)
			{
				ChartTexture = LoadObject<UTexture2D>(nullptr, TEXT("/Game/Charts/OslofjordChart"));
				if (ChartTexture)
				{
					UE_LOG(LogTemp, Log, TEXT("SailingHUD: Loaded chart texture OslofjordChart for overview map."));
				}
			}
		}
	}

	// Defer binding to allow islands to spawn
	FTimerHandle TimerHandle;
	GetWorldTimerManager().SetTimer(TimerHandle, this, &ASailingHUD::BindToIslandDiscoveries, 1.0f, false);
}

void ASailingHUD::BindToIslandDiscoveries()
{
	TArray<AActor*> Islands;
	UGameplayStatics::GetAllActorsOfClass(GetWorld(), AIslandActor::StaticClass(), Islands);

	for (AActor* Actor : Islands)
	{
		if (AIslandActor* Island = Cast<AIslandActor>(Actor))
		{
			if (!BoundIslandDiscoveries.Contains(Island))
			{
				Island->OnDiscovered.AddDynamic(this, &ASailingHUD::OnIslandDiscovered);
				BoundIslandDiscoveries.Add(Island);
			}
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

	if (PC && PC->IsMapViewShown())
	{
		DrawFullMap();
		return;
	}

	DrawCompass();
	DrawWindAndSpeed();
	DrawRadarMinimap();

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

	// Compass position and size - bottom-left corner
	float CenterX = 90.0f;
	float CenterY = Canvas->ClipY - 170.0f;
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

	// Info panel below compass (bottom-left)
	float PanelX = 90.0f - 100.0f;
	float PanelY = Canvas->ClipY - 95.0f;
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
				PointOfSail = TEXT("SL\u00D8R");
				PointColor = FLinearColor(1.0f, 1.0f, 0.3f, 1.0f);
			}
			else if (Angle < 100.0f)
			{
				PointOfSail = TEXT("HALV VIND");
				PointColor = FLinearColor(0.2f, 1.0f, 0.2f, 1.0f);
			}
			else if (Angle < 135.0f)
			{
				PointOfSail = TEXT("ROMSKJ\u00D8TS");
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

void ASailingHUD::DrawCircleOutline(float CX, float CY, float R, FLinearColor Color, float Thickness, int32 Segments)
{
	for (int32 i = 0; i < Segments; ++i)
	{
		float A1 = (float)i / Segments * 2.0f * PI;
		float A2 = (float)(i + 1) / Segments * 2.0f * PI;
		for (float Off = -Thickness * 0.5f; Off <= Thickness * 0.5f; Off += 1.0f)
		{
			DrawLine(
				CX + FMath::Cos(A1) * (R + Off), CY + FMath::Sin(A1) * (R + Off),
				CX + FMath::Cos(A2) * (R + Off), CY + FMath::Sin(A2) * (R + Off),
				Color, 1.0f);
		}
	}
}

void ASailingHUD::DrawRadarMinimap()
{
	APawn* Pawn = GetOwningPawn();
	if (!Pawn || !Canvas) return;

	ASailingGameMode* GM = Cast<ASailingGameMode>(UGameplayStatics::GetGameMode(GetWorld()));
	if (!GM) return;

	FVector PlayerLoc = Pawn->GetActorLocation();
	const float PlayerX = PlayerLoc.X;
	const float PlayerY = PlayerLoc.Y;

	// Radar-sirkel nederst til høyre
	const float Diameter = RadarRadius * 2.0f;
	const float MapCenterX = Canvas->ClipX - MapOffsetRight - RadarRadius;
	const float MapCenterY = Canvas->ClipY - MapOffsetBottom - RadarRadius;
	const float Scale = RadarRadius / FMath::Max(1.0f, MapWorldRadius);

	// 1. Rund bakgrunn
	const bool bUseChart = GM->bUseFjordMap && ChartTexture;
	if (bUseChart)
	{
		// Tegn chart som firkant, sirkel-ramme dekker hjørnene
		float LonMin, LatMin, LonMax, LatMax;
		WorldToLonLat(PlayerX - MapWorldRadius, PlayerY - MapWorldRadius, LonMin, LatMin);
		WorldToLonLat(PlayerX + MapWorldRadius, PlayerY + MapWorldRadius, LonMax, LatMax);
		const float ChartMinLon = ChartLonLatBox.X;
		const float ChartMinLat = ChartLonLatBox.Y;
		const float ChartMaxLon = ChartLonLatBox.Z;
		const float ChartMaxLat = ChartLonLatBox.W;
		const float ChartLonSpan = FMath::Max(ChartMaxLon - ChartMinLon, 0.0001f);
		const float ChartLatSpan = FMath::Max(ChartMaxLat - ChartMinLat, 0.0001f);
		const float U0 = FMath::Clamp((LonMin - ChartMinLon) / ChartLonSpan, 0.0f, 1.0f);
		const float V0 = FMath::Clamp((ChartMaxLat - LatMax) / ChartLatSpan, 0.0f, 1.0f);
		const float U1 = FMath::Clamp((LonMax - ChartMinLon) / ChartLonSpan, 0.0f, 1.0f);
		const float V1 = FMath::Clamp((ChartMaxLat - LatMin) / ChartLatSpan, 0.0f, 1.0f);
		DrawTexture(ChartTexture,
			MapCenterX - RadarRadius, MapCenterY - RadarRadius, Diameter, Diameter,
			U0, V0, U1 - U0, V1 - V0, FLinearColor::White, EBlendMode::BLEND_Translucent);
	}
	else
	{
		// Fylt sirkel-bakgrunn via konsentriske ringer
		for (float R = RadarRadius; R > 0; R -= 1.0f)
		{
			DrawCircleOutline(MapCenterX, MapCenterY, R, MapSeaColor, 1.0f, 48);
		}
	}

	// 2. Øyer (klippet til sirkel)
	TArray<AActor*> AllIslands;
	UGameplayStatics::GetAllActorsOfClass(GetWorld(), AIslandActor::StaticClass(), AllIslands);
	for (AActor* Actor : AllIslands)
	{
		AIslandActor* Island = Cast<AIslandActor>(Actor);
		if (!Island) continue;
		FVector Loc = Island->GetActorLocation();
		float Dx = Loc.X - PlayerX;
		float Dy = Loc.Y - PlayerY;
		float PixelDx = Dx * Scale;
		float PixelDy = -Dy * Scale;
		float PixelDist = FMath::Sqrt(PixelDx * PixelDx + PixelDy * PixelDy);
		if (PixelDist > RadarRadius - 4.0f) continue; // Klipp til sirkel
		float Px = MapCenterX + PixelDx;
		float Py = MapCenterY + PixelDy;
		const float DotSize = 5.0f;
		FLinearColor Color = Island->bDiscovered ? MapIslandColor : MapUndiscoveredIslandColor;
		DrawRect(Color, Px - DotSize * 0.5f, Py - DotSize * 0.5f, DotSize, DotSize);
	}

	// 3. Spiller-trekant i sentrum
	FVector Forward = Pawn->GetActorForwardVector();
	float Angle = FMath::Atan2(Forward.X, Forward.Y);
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

	// 4. Sirkel-ramme
	DrawCircleOutline(MapCenterX, MapCenterY, RadarRadius, FLinearColor(0.7f, 0.8f, 0.9f, 0.9f), 3.0f, 64);

	// Svart ring utenfor for å maskere hjørnene av chart-texture
	if (bUseChart)
	{
		for (float R = RadarRadius + 1.0f; R < RadarRadius + 20.0f; R += 1.0f)
		{
			DrawCircleOutline(MapCenterX, MapCenterY, R, FLinearColor(0.0f, 0.0f, 0.0f, 1.0f), 1.0f, 64);
		}
	}
}

// ---- Fullskjerm-kart ----

void ASailingHUD::SetFullMapVisible(bool bVisible)
{
	bShowFullMap = bVisible;
	if (!bVisible)
	{
		bIsDraggingMap = false;
	}
}

void ASailingHUD::OnFullMapClick(float X, float Y)
{
	bIsDraggingMap = true;
	DragStartMouse = FVector2D(X, Y);
	DragStartPanPixels = FullMapPanPixels;
}

void ASailingHUD::OnFullMapDrag(float X, float Y)
{
	if (!bIsDraggingMap) return;
	FullMapPanPixels = DragStartPanPixels + FVector2D(X, Y) - DragStartMouse;
}

void ASailingHUD::OnFullMapDragDelta(float DeltaX, float DeltaY)
{
	if (!bIsDraggingMap) return;
	FullMapPanPixels.X += DeltaX;
	FullMapPanPixels.Y -= DeltaY;
}

void ASailingHUD::OnFullMapRelease()
{
	bIsDraggingMap = false;
}

void ASailingHUD::OnFullMapScroll(float Delta)
{
	float OldZoom = FullMapZoom;
	FullMapZoom = FMath::Clamp(FullMapZoom + Delta * 0.15f, 0.2f, 5.0f);
	float Ratio = FullMapZoom / OldZoom;
	FullMapPanPixels *= Ratio;
	if (bIsDraggingMap) { DragStartPanPixels *= Ratio; }
}

void ASailingHUD::DrawFullMap()
{
	APawn* Pawn = GetOwningPawn();
	if (!Pawn || !Canvas) return;

	ASailingGameMode* GM = Cast<ASailingGameMode>(UGameplayStatics::GetGameMode(GetWorld()));
	if (!GM) return;

	FVector PlayerLoc = Pawn->GetActorLocation();

	const float ScreenW = Canvas->ClipX;
	const float ScreenH = Canvas->ClipY;
	const float MapPixelRadius = FMath::Min(ScreenW, ScreenH) * 0.4f;
	const float EffectiveWorldRadius = MapWorldRadius / FMath::Max(0.2f, FullMapZoom);

	// Konverter piksel-pan til world-offset via Canvas-dimensjoner
	const float PixelToWorld = EffectiveWorldRadius / FMath::Max(1.0f, MapPixelRadius);
	const float CenterWorldX = PlayerLoc.X - FullMapPanPixels.X * PixelToWorld;
	const float CenterWorldY = PlayerLoc.Y + FullMapPanPixels.Y * PixelToWorld;
	const float Scale = MapPixelRadius / FMath::Max(1.0f, EffectiveWorldRadius);
	const float MapScreenCX = ScreenW * 0.5f;
	const float MapScreenCY = ScreenH * 0.5f;

	// 1. Halvtransparent bakgrunn
	DrawRect(FLinearColor(0.0f, 0.0f, 0.0f, 0.75f), 0.0f, 0.0f, ScreenW, ScreenH);

	// 2. Kartbakgrunn
	const float MapLeft = MapScreenCX - MapPixelRadius;
	const float MapTop = MapScreenCY - MapPixelRadius;
	const float MapSize = MapPixelRadius * 2.0f;

	const bool bUseChart = GM->bUseFjordMap && ChartTexture;
	if (bUseChart)
	{
		float LonMin, LatMin, LonMax, LatMax;
		WorldToLonLat(CenterWorldX - EffectiveWorldRadius, CenterWorldY - EffectiveWorldRadius, LonMin, LatMin);
		WorldToLonLat(CenterWorldX + EffectiveWorldRadius, CenterWorldY + EffectiveWorldRadius, LonMax, LatMax);
		const float ChartMinLon = ChartLonLatBox.X;
		const float ChartMinLat = ChartLonLatBox.Y;
		const float ChartMaxLon = ChartLonLatBox.Z;
		const float ChartMaxLat = ChartLonLatBox.W;
		const float ChartLonSpan = FMath::Max(ChartMaxLon - ChartMinLon, 0.0001f);
		const float ChartLatSpan = FMath::Max(ChartMaxLat - ChartMinLat, 0.0001f);
		const float U0 = FMath::Clamp((LonMin - ChartMinLon) / ChartLonSpan, 0.0f, 1.0f);
		const float V0 = FMath::Clamp((ChartMaxLat - LatMax) / ChartLatSpan, 0.0f, 1.0f);
		const float U1 = FMath::Clamp((LonMax - ChartMinLon) / ChartLonSpan, 0.0f, 1.0f);
		const float V1 = FMath::Clamp((ChartMaxLat - LatMin) / ChartLatSpan, 0.0f, 1.0f);
		DrawTexture(ChartTexture, MapLeft, MapTop, MapSize, MapSize,
			U0, V0, U1 - U0, V1 - V0, FLinearColor::White, EBlendMode::BLEND_Translucent);
	}
	else
	{
		DrawRect(MapSeaColor, MapLeft, MapTop, MapSize, MapSize);
	}

	// Museposisjon for hover (kan feile på macOS i kartmodus)
	float MouseX = 0.0f, MouseY = 0.0f;
	const bool bHasMousePos = PlayerOwner && PlayerOwner->GetMousePosition(MouseX, MouseY);

	constexpr float HoverThresholdPx = 18.0f;
	const float HoverThresholdSq = HoverThresholdPx * HoverThresholdPx;
	AIslandActor* HoveredIsland = nullptr;
	float BestHoverDistSq = HoverThresholdSq;
	float HoveredPx = 0.0f, HoveredPy = 0.0f;

	// 3. Øyer
	TArray<AActor*> AllIslands;
	UGameplayStatics::GetAllActorsOfClass(GetWorld(), AIslandActor::StaticClass(), AllIslands);
	for (AActor* Actor : AllIslands)
	{
		AIslandActor* Island = Cast<AIslandActor>(Actor);
		if (!Island) continue;
		FVector Loc = Island->GetActorLocation();
		float Dx = Loc.X - CenterWorldX;
		float Dy = Loc.Y - CenterWorldY;
		float Px = MapScreenCX + Dx * Scale;
		float Py = MapScreenCY - Dy * Scale;
		// Klipp til kartområde
		if (Px < MapLeft || Px > MapLeft + MapSize || Py < MapTop || Py > MapTop + MapSize) continue;

		// Hover-hit: øy nærmest cursor innenfor terskel
		if (bHasMousePos)
		{
			float DistSq = (Px - MouseX) * (Px - MouseX) + (Py - MouseY) * (Py - MouseY);
			if (DistSq < BestHoverDistSq)
			{
				BestHoverDistSq = DistSq;
				HoveredIsland = Island;
				HoveredPx = Px;
				HoveredPy = Py;
			}
		}

		const float DotSize = 7.0f;
		FLinearColor Color = Island->bDiscovered ? MapIslandColor : MapUndiscoveredIslandColor;
		DrawRect(Color, Px - DotSize * 0.5f, Py - DotSize * 0.5f, DotSize, DotSize);

		// Vis navn på oppdagede øyer (med lesbar bakgrunn)
		if (Island->bDiscovered)
		{
			const float TextX = Px + DotSize;
			const float TextY = Py - 6.0f;
			const float EstCharW = 9.0f;
			const float LabelW = FMath::Max(40.0f, Island->IslandName.Len() * EstCharW + 8.0f);
			const float LabelH = 14.0f;
			DrawRect(FLinearColor(0.0f, 0.0f, 0.0f, 0.75f), TextX, TextY - 2.0f, LabelW, LabelH);
			DrawText(Island->IslandName, FLinearColor::White,
				TextX + 4.0f, TextY, nullptr, 1.0f);
		}
	}

	// Tooltip for hovered øy (alle øyer, også uoppdagede)
	if (HoveredIsland)
	{
		const float TooltipPad = 6.0f;
		const float EstCharW = 9.0f;
		const float Tw = FMath::Max(60.0f, HoveredIsland->IslandName.Len() * EstCharW + TooltipPad * 2.0f);
		const float Th = 18.0f;
		const float Tx = HoveredPx - Tw * 0.5f;
		const float Ty = HoveredPy - Th - 8.0f;
		DrawRect(FLinearColor(0.05f, 0.1f, 0.2f, 0.9f), Tx, Ty, Tw, Th);
		DrawText(HoveredIsland->IslandName, FLinearColor::White,
			Tx + TooltipPad, Ty + 2.0f, nullptr, 1.1f);
	}

	// 4. Spiller-trekant
	{
		float Dx = PlayerLoc.X - CenterWorldX;
		float Dy = PlayerLoc.Y - CenterWorldY;
		float PlayerPx = MapScreenCX + Dx * Scale;
		float PlayerPy = MapScreenCY - Dy * Scale;

		FVector Forward = Pawn->GetActorForwardVector();
		float Angle = FMath::Atan2(Forward.X, Forward.Y);
		const float TriR = 10.0f;
		const float TriW = 5.0f;
		float TipX = PlayerPx + FMath::Sin(Angle) * TriR;
		float TipY = PlayerPy - FMath::Cos(Angle) * TriR;
		float B1X = PlayerPx - FMath::Sin(Angle) * TriR + FMath::Cos(Angle) * TriW;
		float B1Y = PlayerPy + FMath::Cos(Angle) * TriR + FMath::Sin(Angle) * TriW;
		float B2X = PlayerPx - FMath::Sin(Angle) * TriR - FMath::Cos(Angle) * TriW;
		float B2Y = PlayerPy + FMath::Cos(Angle) * TriR - FMath::Sin(Angle) * TriW;
		DrawLine(TipX, TipY, B1X, B1Y, MapPlayerColor, 2.5f);
		DrawLine(TipX, TipY, B2X, B2Y, MapPlayerColor, 2.5f);
		DrawLine(B1X, B1Y, B2X, B2Y, MapPlayerColor, 2.5f);
	}

	// 5. Ramme
	DrawRect(FLinearColor(0.5f, 0.7f, 0.9f, 0.8f), MapLeft - 2, MapTop - 2, MapSize + 4, 2);
	DrawRect(FLinearColor(0.5f, 0.7f, 0.9f, 0.8f), MapLeft - 2, MapTop + MapSize, MapSize + 4, 2);
	DrawRect(FLinearColor(0.5f, 0.7f, 0.9f, 0.8f), MapLeft - 2, MapTop, 2, MapSize);
	DrawRect(FLinearColor(0.5f, 0.7f, 0.9f, 0.8f), MapLeft + MapSize, MapTop, 2, MapSize);

	// 6. Hint-tekst
	FString HintText = TEXT("M / Esc: Lukk  |  Dra: Panorer  |  Scroll: Zoom");
	DrawText(HintText, FLinearColor(0.7f, 0.8f, 0.9f, 0.8f),
		MapScreenCX - 200.0f, MapTop + MapSize + 12.0f, nullptr, 1.2f);

	// Zoom-indikator
	FString ZoomText = FString::Printf(TEXT("Zoom: %.1fx"), FullMapZoom);
	DrawText(ZoomText, FLinearColor(0.7f, 0.8f, 0.9f, 0.7f),
		MapLeft + 8.0f, MapTop + 8.0f, nullptr, 1.1f);
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
