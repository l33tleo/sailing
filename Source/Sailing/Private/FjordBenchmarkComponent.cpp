#include "FjordBenchmarkComponent.h"
#include "Camera/CameraActor.h"
#include "Camera/CameraComponent.h"
#include "Engine/World.h"
#include "GameFramework/HUD.h"
#include "GameFramework/Pawn.h"
#include "Components/PrimitiveComponent.h"
#include "GameFramework/PlayerController.h"
#include "HAL/FileManager.h"
#include "Kismet/KismetMathLibrary.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "Engine/StaticMesh.h"
#include "Engine/StaticMeshActor.h"
#include "Components/StaticMeshComponent.h"
#include "RenderUtils.h"

UFjordBenchmarkComponent::UFjordBenchmarkComponent()
{
	PrimaryComponentTick.bCanEverTick = true;

	// Posisjoner i verdens-uu (meter fra Oslo-origo × 100). Kamerapunktene er valgt i åpent vann
	// 300–400 m fra øya (punkt-i-polygon-sjekket mot scripts/fjord_data.json). Øyehøyde for en
	// seiler: ~2,5 m over vannflaten (WaterZ=100). «Oversikt» er et høyt skråbilde som viser relieff.
	const float EyeZ = 350.0f;
	Stations = {
		{ TEXT("hovedoya"),    FVector(-189800.0, -156900.0, EyeZ),  FVector(-102570.0, -156873.0, 1500.0) },
		// Nærbilde ~60 m fra Hovedøyas vestside: for å vurdere materiale, strandsone og vegetasjon.
		{ TEXT("hovedoya_naer"), FVector(-156000.0, -160100.0, EyeZ),  FVector(-140000.0, -157000.0, 1200.0) },
		{ TEXT("gressholmen"), FVector(-134400.0, -229300.0, EyeZ),  FVector(-177868.0, -304643.0, 1000.0) },
		{ TEXT("lindoya"),     FVector(-270900.0, -291700.0, EyeZ),  FVector(-205019.0, -225800.0, 1000.0) },
		{ TEXT("haaoya"),      FVector(-834300.0, -2650900.0, EyeZ), FVector(-1011285.0, -2344466.0, 8000.0) },
		{ TEXT("oversikt"),    FVector(-320000.0, -420000.0, 60000.0), FVector(-150000.0, -220000.0, 0.0) },
	};
}

bool UFjordBenchmarkComponent::IsRequestedOnCommandLine()
{
	return FParse::Param(FCommandLine::Get(), TEXT("FjordShots"))
		|| FParse::Param(FCommandLine::Get(), TEXT("FjordBench"))
		|| FParse::Param(FCommandLine::Get(), TEXT("FjordGroundTest"));
}

void UFjordBenchmarkComponent::BeginPlay()
{
	Super::BeginPlay();

	bShots = FParse::Param(FCommandLine::Get(), TEXT("FjordShots"));
	bBench = FParse::Param(FCommandLine::Get(), TEXT("FjordBench"));
	FParse::Value(FCommandLine::Get(), TEXT("FjordLabel="), Label);

	bGroundTest = FParse::Param(FCommandLine::Get(), TEXT("FjordGroundTest"));
	if (bGroundTest)
	{
		// Egen modus: spillerkameraet beholdes, ingen stasjoner.
		UE_LOG(LogTemp, Log, TEXT("[GROUNDTEST] start"));
		return;
	}

	if (!bShots && !bBench)
	{
		SetComponentTickEnabled(false);
		return;
	}

	Camera = GetWorld()->SpawnActor<ACameraActor>(ACameraActor::StaticClass(), FTransform::Identity);
	if (Camera)
	{
		// Ingen letterboxing: skjermbildet skal fylle hele oppløsningen.
		Camera->GetCameraComponent()->bConstrainAspectRatio = false;
		// Smalere enn spillkameraet: øyene fyller mer av bildet, så før/etter blir lettere å lese.
		Camera->GetCameraComponent()->SetFieldOfView(60.0f);
	}

	// Nanite-status for denne maskinen/RHI-en (fase 0-spike; nyttig i hver målelogg).
	UE_LOG(LogTemp, Log, TEXT("[FPSBENCH] nanite plattformstotte=%d ibruk=%d"),
		DoesPlatformSupportNanite(GMaxRHIShaderPlatform) ? 1 : 0,
		UseNanite(GMaxRHIShaderPlatform) ? 1 : 0);

	// -FjordSpikeMesh=/Game/...: plasser en testmesh rett foran første stasjon (Nanite-/ytelsesspike).
	FString SpikePath;
	if (FParse::Value(FCommandLine::Get(), TEXT("FjordSpikeMesh="), SpikePath))
	{
		if (UStaticMesh* SpikeMesh = LoadObject<UStaticMesh>(nullptr, *SpikePath))
		{
			const FVector Loc = Stations[0].CameraLocation + FVector(40000.0, 0.0, -250.0);
			AStaticMeshActor* A = GetWorld()->SpawnActor<AStaticMeshActor>(Loc, FRotator::ZeroRotator);
			A->SetMobility(EComponentMobility::Movable);
			A->GetStaticMeshComponent()->SetStaticMesh(SpikeMesh);
			UE_LOG(LogTemp, Log, TEXT("[FPSBENCH] spikemesh=%s nanitedata=%d"), *SpikePath,
				SpikeMesh->HasValidNaniteData() ? 1 : 0);
		}
		else
		{
			UE_LOG(LogTemp, Warning, TEXT("[FPSBENCH] fant ikke spikemesh %s"), *SpikePath);
		}
	}

	UE_LOG(LogTemp, Log, TEXT("[FPSBENCH] start label=%s bilder=%d benk=%d stasjoner=%d"),
		*Label, bShots ? 1 : 0, bBench ? 1 : 0, Stations.Num());

	EnterStation(0);
}

void UFjordBenchmarkComponent::EnterStation(int32 Index)
{
	StationIndex = Index;
	Phase = EPhase::Warmup;
	PhaseTime = 0.0f;

	if (!Stations.IsValidIndex(Index))
	{
		Phase = EPhase::Done;
		UE_LOG(LogTemp, Log, TEXT("[FPSBENCH] ferdig label=%s"), *Label);
		FPlatformMisc::RequestExit(false);
		return;
	}

	const FStation& S = Stations[Index];
	if (Camera)
	{
		Camera->SetActorLocationAndRotation(S.CameraLocation,
			UKismetMathLibrary::FindLookAtRotation(S.CameraLocation, S.LookAt));
	}
}

void UFjordBenchmarkComponent::TickComponent(float DeltaTime, ELevelTick TickType,
	FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	if (bGroundTest)
	{
		TickGroundTest(DeltaTime);
		return;
	}

	if (Phase == EPhase::Done || !Camera)
	{
		return;
	}

	// Spillerkontrolleren finnes ikke nødvendigvis i GameMode::BeginPlay; hold visningen låst her.
	if (APlayerController* PC = GetWorld()->GetFirstPlayerController())
	{
		if (PC->GetViewTarget() != Camera)
		{
			PC->SetViewTarget(Camera);
		}
		if (AHUD* HUD = PC->GetHUD())
		{
			HUD->bShowHUD = false;
		}
	}

	PhaseTime += DeltaTime;

	switch (Phase)
	{
	case EPhase::Warmup:
		if (PhaseTime >= (StationIndex == 0 ? FirstStationWarmupSeconds : StationWarmupSeconds))
		{
			AdvancePhase();
		}
		break;

	case EPhase::ShotSettle:
		if (PhaseTime >= ShotSettleSeconds)
		{
			AdvancePhase();
		}
		break;

	case EPhase::Bench:
		FrameTimes.Add(DeltaTime);
		if (PhaseTime >= BenchSeconds)
		{
			FinishBench();
			AdvancePhase();
		}
		break;

	default:
		break;
	}
}

void UFjordBenchmarkComponent::AdvancePhase()
{
	const EPhase Previous = Phase;
	PhaseTime = 0.0f;

	if (Previous == EPhase::Warmup && bShots)
	{
		const FString Dir = FPaths::ConvertRelativePathToFull(
			FPaths::ProjectDir() / TEXT("renders/landscape") / Label);
		IFileManager::Get().MakeDirectory(*Dir, true);
		const FString File = Dir / FString::Printf(TEXT("%s.png"), Stations[StationIndex].Name);

		if (APlayerController* PC = GetWorld()->GetFirstPlayerController())
		{
			PC->ConsoleCommand(FString::Printf(TEXT("HighResShot 1600x900 filename=\"%s\""), *File));
			UE_LOG(LogTemp, Log, TEXT("[FPSBENCH] bilde stasjon=%s fil=%s"),
				Stations[StationIndex].Name, *File);
		}
		Phase = EPhase::ShotSettle;
		return;
	}

	if ((Previous == EPhase::Warmup || Previous == EPhase::ShotSettle) && bBench)
	{
		FrameTimes.Reset();
		Phase = EPhase::Bench;
		return;
	}

	EnterStation(StationIndex + 1);
}

void UFjordBenchmarkComponent::FinishBench()
{
	if (FrameTimes.Num() == 0)
	{
		return;
	}

	double Sum = 0.0;
	for (float T : FrameTimes)
	{
		Sum += T;
	}

	// p1 = fps ved 99-persentilen av bildetid (de 1 % tregeste bildene).
	FrameTimes.Sort();
	const float P99Time = FrameTimes[FMath::Clamp(
		FMath::FloorToInt(FrameTimes.Num() * 0.99f), 0, FrameTimes.Num() - 1)];

	UE_LOG(LogTemp, Log, TEXT("[FPSBENCH] stasjon=%s snitt=%.1f p1=%.1f bilder=%d sek=%.1f label=%s"),
		Stations[StationIndex].Name, FrameTimes.Num() / Sum,
		P99Time > 0.0f ? 1.0f / P99Time : 0.0f, FrameTimes.Num(), Sum, *Label);
}

void UFjordBenchmarkComponent::TickGroundTest(float DeltaTime)
{
	APlayerController* PC = GetWorld()->GetFirstPlayerController();
	APawn* Pawn = PC ? PC->GetPawn() : nullptr;
	UPrimitiveComponent* Body = Pawn ? Cast<UPrimitiveComponent>(Pawn->GetRootComponent()) : nullptr;
	if (!Body)
	{
		return;
	}

	// Start i åpent vann ved Hovedøya-stasjonen; mål = øyas sentroide (inne på land).
	const FVector Start(Stations[0].CameraLocation.X, Stations[0].CameraLocation.Y, 100.0);
	const FVector2D Target(Stations[0].LookAt.X, Stations[0].LookAt.Y);

	GroundTestTime += DeltaTime;
	if (!bGroundTestPlaced)
	{
		// Vent til GameMode har flyttet båten til lagret/startposisjon, og plasser den så selv.
		if (GroundTestTime < 3.0f)
		{
			return;
		}
		Pawn->SetActorLocationAndRotation(Start, FRotator::ZeroRotator, false, nullptr, ETeleportType::ResetPhysics);
		bGroundTestPlaced = true;
		GroundTestTime = 0.0f;
		return;
	}

	const FVector Loc = Pawn->GetActorLocation();
	const FVector2D ToTarget = Target - FVector2D(Loc.X, Loc.Y);
	const FVector2D Dir = ToTarget.GetSafeNormal();

	// Skyv med en massefri akselerasjon mot land (som seilkraften), begrenset til ~MaxBoatSpeed.
	// Hastigheten tvinges IKKE: da overstyres oppdrift/kollisjon og båten «flyr» inn over land.
	const FVector Before = Body->GetPhysicsLinearVelocity();
	if (FVector2D::DotProduct(FVector2D(Before.X, Before.Y), Dir) < 800.0f)
	{
		Body->AddForce(FVector(Dir.X, Dir.Y, 0.0f) * 300.0f, NAME_None, /*bAccelChange=*/true);
	}

	// «Står stille» = fysikken bremset oss ned siden forrige frame selv om vi skyver.
	const float ActualSpeed = FVector2D(Before.X, Before.Y).Size();
	GroundTestStillTime = (GroundTestTime > 2.0f && ActualSpeed < 150.0f) ? GroundTestStillTime + DeltaTime : 0.0f;

	if (GroundTestTime >= GroundTestNextLog)
	{
		GroundTestNextLog += 2.0f;
		UE_LOG(LogTemp, Log, TEXT("[GROUNDTEST] t=%.0f pos=(%.0f,%.0f,%.0f) fart=%.0f til_sentroide=%.0f m"),
			GroundTestTime, Loc.X, Loc.Y, Loc.Z, ActualSpeed, ToTarget.Size() / 100.0f);
	}

	if (GroundTestStillTime > 4.0f || GroundTestTime > 120.0f)
	{
		UE_LOG(LogTemp, Log, TEXT("[GROUNDTEST] slutt t=%.0f stoppet=%d pos=(%.0f,%.0f,%.0f) til_sentroide=%.0f m"),
			GroundTestTime, GroundTestStillTime > 4.0f ? 1 : 0, Loc.X, Loc.Y, Loc.Z, ToTarget.Size() / 100.0f);
		bGroundTest = false;
		SetComponentTickEnabled(false);
		FPlatformMisc::RequestExit(false);
	}
}
