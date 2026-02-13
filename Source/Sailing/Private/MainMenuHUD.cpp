#include "MainMenuHUD.h"
#include "SailingGameInstance.h"
#include "SaveGameSailing.h"
#include "Kismet/GameplayStatics.h"
#include "Kismet/KismetSystemLibrary.h"
#include "Engine/Canvas.h"

void AMainMenuHUD::BeginPlay()
{
	Super::BeginPlay();
	bHasSave = UGameplayStatics::DoesSaveGameExist(USaveGameSailing::SaveSlotName, 0);
}

void AMainMenuHUD::DrawHUD()
{
	Super::DrawHUD();
	if (!Canvas) return;
	DrawMenu();
}

void AMainMenuHUD::DrawMenu()
{
	CenterX = Canvas->ClipX * 0.5f;
	CenterY = Canvas->ClipY * 0.5f;

	const float TotalH = ButtonH * 3.0f + ButtonSpacing * 2.0f;
	float Y = CenterY - TotalH * 0.5f;

	NewGameY = Y;
	ContinueY = Y + ButtonH + ButtonSpacing;
	QuitY = Y + (ButtonH + ButtonSpacing) * 2.0f;

	const float Bx = CenterX - ButtonW * 0.5f;

	// Bakgrunn
	DrawRect(FLinearColor(0.0f, 0.05f, 0.15f, 0.9f), Bx - 20.0f, NewGameY - 20.0f, ButtonW + 40.0f, TotalH + 40.0f);

	// Knapper
	FLinearColor Disabled(0.3f, 0.3f, 0.3f, 0.6f);
	FLinearColor Normal(0.2f, 0.5f, 0.8f, 0.9f);

	DrawRect(bHasSave ? Normal : Disabled, Bx, NewGameY, ButtonW, ButtonH);
	DrawText(TEXT("Nytt spill"), FLinearColor::White, Bx + 20.0f, NewGameY + 14.0f, nullptr, 1.5f);

	DrawRect(bHasSave ? Normal : Disabled, Bx, ContinueY, ButtonW, ButtonH);
	DrawText(TEXT("Fortsett"), FLinearColor::White, Bx + 20.0f, ContinueY + 14.0f, nullptr, 1.5f);

	DrawRect(Normal, Bx, QuitY, ButtonW, ButtonH);
	DrawText(TEXT("Avslutt"), FLinearColor::White, Bx + 20.0f, QuitY + 14.0f, nullptr, 1.5f);
}

bool AMainMenuHUD::CheckButtonHit(float X, float Y, float Bx, float By, float Bw, float Bh)
{
	return X >= Bx && X <= Bx + Bw && Y >= By && Y <= By + Bh;
}

void AMainMenuHUD::OnMouseClick(float X, float Y)
{
	const float Bx = CenterX - ButtonW * 0.5f;

	if (CheckButtonHit(X, Y, Bx, NewGameY, ButtonW, ButtonH))
	{
		USailingGameInstance* GI = Cast<USailingGameInstance>(GetWorld()->GetGameInstance());
		if (GI)
		{
			GI->bRequestNewGame = true;
			UGameplayStatics::OpenLevel(this, FName(TEXT("MainOcean")));
		}
		return;
	}
	if (bHasSave && CheckButtonHit(X, Y, Bx, ContinueY, ButtonW, ButtonH))
	{
		UGameplayStatics::OpenLevel(this, FName(TEXT("MainOcean")));
		return;
	}
	if (CheckButtonHit(X, Y, Bx, QuitY, ButtonW, ButtonH))
	{
		UKismetSystemLibrary::QuitGame(this, GetOwningPlayerController(), EQuitPreference::Quit, false);
	}
}
