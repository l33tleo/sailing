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
#include "EngineUtils.h"
#include "WaterBodyActor.h"
#include "Sailing.h"
#include "Engine/StaticMesh.h"
#include "Engine/StaticMeshActor.h"
#include "Components/StaticMeshComponent.h"
#include "RenderUtils.h"
#include "FjordGeometry.h"
#include "OceanWaterSetupActor.h"
#include "WaterBodyComponent.h"
#include "WaterZoneActor.h"

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
		|| FParse::Param(FCommandLine::Get(), TEXT("FjordGroundTest"))
		|| FParse::Param(FCommandLine::Get(), TEXT("FjordBoatShot"))
		|| FParse::Param(FCommandLine::Get(), TEXT("FjordWaterCheck"));
}

void UFjordBenchmarkComponent::BeginPlay()
{
	Super::BeginPlay();

	bShots = FParse::Param(FCommandLine::Get(), TEXT("FjordShots"));
	bBench = FParse::Param(FCommandLine::Get(), TEXT("FjordBench"));
	FParse::Value(FCommandLine::Get(), TEXT("FjordLabel="), Label);

	bBoatShot = FParse::Param(FCommandLine::Get(), TEXT("FjordBoatShot"));
	if (bBoatShot)
	{
		// -FjordBoatShotDelay=<s>: vent lenger før bildet (f.eks. for å se en bølgeovertoning).
		FParse::Value(FCommandLine::Get(), TEXT("FjordBoatShotDelay="), FirstStationWarmupSeconds);
		return;
	}

	bWaterCheck = FParse::Param(FCommandLine::Get(), TEXT("FjordWaterCheck"));
	if (bWaterCheck)
	{
		// Bildene tas fra vannsonens ytterkanter i stedet for referanseøyene: der manglet vannflaten
		// (havets bounds var sentrert på origo, se UFjordOceanBodyComponent). Punktene er åpent vann
		// i DTM-en (≥400 m til land, unntatt Nesøya som er selve feilstedet). 25 m over vannet,
		// blikk ~600 m frem, så bildet viser mest vannflate.
		bShots = true;
		const float CheckEyeZ = 2500.0f;
		Stations = {
			{ TEXT("vann_nesoya"),  FVector(-1209000.0, -389100.0, CheckEyeZ),  FVector(-1150000.0, -360000.0, 100.0) },
			{ TEXT("vann_vest"),    FVector(-1354000.0, -640200.0, CheckEyeZ),  FVector(-1300000.0, -600000.0, 100.0) },
			{ TEXT("vann_sor"),     FVector(-700000.0, -3100000.0, CheckEyeZ),  FVector(-700000.0, -3040000.0, 100.0) },
			{ TEXT("vann_midt"),    FVector(-910100.0, -1387100.0, CheckEyeZ),  FVector(-850000.0, -1330000.0, 100.0) },
			{ TEXT("vann_nordost"), FVector(-5000.0, -318500.0, CheckEyeZ),     FVector(50000.0, -280000.0, 100.0) },
		};
	}

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
	if (bBoatShot)
	{
		TickBoatShot(DeltaTime);
		return;
	}

	if (Phase == EPhase::Done || !Camera)
	{
		return;
	}

	// Vannsjekken kjøres én gang etter oppvarmingen på første stasjon: da er landkollisjonen bygd
	// (den finnes ikke på frame 0) og vannmeshen har hatt tid til å bygges.
	if (bWaterCheck && !bWaterCheckDone && StationIndex == 0 && PhaseTime > FirstStationWarmupSeconds * 0.5f)
	{
		bWaterCheckDone = true;
		RunWaterCheck();
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

void UFjordBenchmarkComponent::TickBoatShot(float DeltaTime)
{
	BoatShotTime += DeltaTime;
	if (BoatShotTime < FirstStationWarmupSeconds)
	{
		return;
	}
	if (BoatShotTime < FirstStationWarmupSeconds + ShotSettleSeconds)
	{
		if (!bGroundTestPlaced)   // gjenbrukt som «bilde tatt»-flagg
		{
			bGroundTestPlaced = true;
			const FString Dir = FPaths::ConvertRelativePathToFull(FPaths::ProjectDir() / TEXT("renders/landscape") / Label);
			IFileManager::Get().MakeDirectory(*Dir, true);
			if (APlayerController* PC = GetWorld()->GetFirstPlayerController())
			{
				PC->ConsoleCommand(FString::Printf(TEXT("HighResShot 1600x900 filename=\"%s\""), *(Dir / TEXT("boat.png"))));

				// Diagnose: hva ligger i/over vannflaten rundt båten? (stråler ned i et rutenett)
				if (APawn* Pawn = PC->GetPawn())
				{
					const FVector P = Pawn->GetActorLocation();
					for (int32 dy = -4; dy <= 4; ++dy)
					{
						for (int32 dx = -4; dx <= 4; ++dx)
						{
							const FVector S(P.X + dx * 500.0, P.Y + dy * 500.0, 5000.0);
							TArray<FHitResult> Hits;
							FCollisionQueryParams Q(SCENE_QUERY_STAT(BoatShotDiag), true, Pawn);
							for (TActorIterator<AWaterBody> It(GetWorld()); It; ++It)
							{
								Q.AddIgnoredActor(*It);
							}
							FCollisionObjectQueryParams Obj;
							Obj.AddObjectTypesToQuery(ECC_WorldStatic);
							Obj.AddObjectTypesToQuery(ECC_FjordLand);
							GetWorld()->LineTraceMultiByObjectType(Hits, S, FVector(S.X, S.Y, -5000.0), Obj, Q);
							for (const FHitResult& Hit : Hits)
							{
								if (Hit.ImpactPoint.Z > 60.0 && Hit.bBlockingHit)
								{
									UE_LOG(LogTemp, Log, TEXT("[BOATDIAG] (%+d,%+d) z=%.0f aktor=%s komp=%s"), dx, dy, Hit.ImpactPoint.Z,
										*GetNameSafe(Hit.GetActor()), *GetNameSafe(Hit.GetComponent()));
								}
							}
						}
					}
				}
			}
		}
		return;
	}
	FPlatformMisc::RequestExit(false);
}

void UFjordBenchmarkComponent::RunWaterCheck()
{
	UWorld* World = GetWorld();
	const TSharedPtr<FjordGeometry::FHeightGrid> Grid = FjordGeometry::FHeightGrid::Load();
	if (!World || !Grid.IsValid() || !Grid->IsValid())
	{
		UE_LOG(LogTemp, Error, TEXT("[VANNSJEKK] mangler DTM (Content/Fjord/Terrain/oslofjord_dtm) — ingen sjekk."));
		return;
	}

	// Havets render-bounds: vannmeshen bygges bare der. Setup-aktøren logger også om bounds dekket
	// sonen i det øyeblikket meshen ble bygd (det var der feilen satt).
	FBox2D OceanBox(ForceInit);
	float WaterZ = 100.0f;
	for (TActorIterator<AWaterBody> It(World); It; ++It)
	{
		if (const UWaterBodyComponent* Body = It->GetWaterBodyComponent())
		{
			if (Body->GetWaterBodyType() == EWaterBodyType::Ocean)
			{
				OceanBox += FBox2D(FVector2D(Body->Bounds.Origin - Body->Bounds.BoxExtent),
					FVector2D(Body->Bounds.Origin + Body->Bounds.BoxExtent));
				WaterZ = Body->GetComponentLocation().Z;
			}
		}
	}
	bool bSetupCovered = false;
	for (TActorIterator<AOceanWaterSetupActor> It(World); It; ++It)
	{
		bSetupCovered = It->bOceanBoundsCoveredZoneAtSetup;
	}

	// Sjø = DTM ≤ 0,5 m i punktet OG 100 m ut i fire retninger, så avvik mellom OSM-kysten og den
	// grove DTM-en (~34 m/px) langs strendene ikke gir falske treff.
	constexpr float SeaMaxM = 0.5f;
	constexpr double StepUU = 25000.0;     // 250 m
	constexpr double ProbeUU = 10000.0;    // 100 m
	auto IsSea = [&](double X, double Y)
	{
		return Grid->SampleMeters(X, Y) <= SeaMaxM
			&& Grid->SampleMeters(X + ProbeUU, Y) <= SeaMaxM && Grid->SampleMeters(X - ProbeUU, Y) <= SeaMaxM
			&& Grid->SampleMeters(X, Y + ProbeUU) <= SeaMaxM && Grid->SampleMeters(X, Y - ProbeUU) <= SeaMaxM;
	};

	FCollisionQueryParams Params(SCENE_QUERY_STAT(FjordWaterCheck), /*bTraceComplex=*/true);
	const FCollisionObjectQueryParams LandOnly(ECC_FjordLand);
	int32 NumSea = 0, NumLandOverSea = 0, NumOutsideOcean = 0, NumLogged = 0;
	constexpr int32 MaxLogged = 40;
	for (double Y = Grid->MinY + StepUU * 0.5; Y < Grid->MaxY; Y += StepUU)
	{
		for (double X = Grid->MinX + StepUU * 0.5; X < Grid->MaxX; X += StepUU)
		{
			if (!IsSea(X, Y))
			{
				continue;
			}
			++NumSea;
			if (!OceanBox.IsInside(FVector2D(X, Y)))
			{
				++NumOutsideOcean;
				if (NumLogged++ < MaxLogged)
				{
					UE_LOG(LogTemp, Warning, TEXT("[VANNSJEKK] UTENFOR_HAVMESH (%.0f,%.0f)"), X, Y);
				}
				continue;
			}
			FHitResult Hit;
			if (World->LineTraceSingleByObjectType(Hit, FVector(X, Y, 100000.0), FVector(X, Y, -100000.0), LandOnly, Params)
				&& Hit.ImpactPoint.Z > WaterZ)
			{
				++NumLandOverSea;
				if (NumLogged++ < MaxLogged)
				{
					UE_LOG(LogTemp, Warning, TEXT("[VANNSJEKK] LAND_OVER_SJO (%.0f,%.0f) z=%.0f aktor=%s"),
						X, Y, Hit.ImpactPoint.Z, *GetNameSafe(Hit.GetActor()));
				}
			}
		}
	}
	const int32 NumErrors = NumLandOverSea + NumOutsideOcean + (bSetupCovered ? 0 : 1);
	UE_LOG(LogTemp, Log, TEXT("[VANNSJEKK] sjopunkter=%d land_over_sjo=%d (%.2f km2) utenfor_havmesh=%d havbounds_ved_oppsett=%s feil=%d"),
		NumSea, NumLandOverSea, NumLandOverSea * StepUU * StepUU / 1e10, NumOutsideOcean,
		bSetupCovered ? TEXT("dekket") : TEXT("DEKKET_IKKE"), NumErrors);
}
