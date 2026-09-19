# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project Overview

Sailing is a single-player sailing exploration game built in Unreal Engine 5.8 (macOS only). The player navigates a sailboat through procedurally generated ocean with discoverable Nordic-named islands. Written in C++ with Norwegian documentation.

## Build Commands

Build the project (editor target, Development config):
```bash
"/Users/Shared/Epic Games/UE_5.8/Engine/Build/BatchFiles/Mac/Build.sh" SailingEditor Mac Development -Project="/Users/leovonschwind/sailing/Sailing.uproject" -WaitMutex
```
Append `2>&1 | tail -10` for abbreviated output.

Kjør spillet frittstående i fullskjerm (bygger først; krever at editoren er lukket):
```bash
scripts/run_fullscreen.sh            # --no-build | --res 1920x1080 | --hitches | --force
```
Bruk dette for å vurdere spillfølelse: på macOS 27 + UE 5.8 gir vindusmodus/PIE periodiske ~1 s-stopp (Metal `nextDrawable`-timeout i motoren, ikke prosjektkode) som ikke opptrer i fullskjerm. Kjør aldri to Unreal-instanser samtidig (memory pressure).

Generate Xcode project files:
```bash
"/Users/Shared/Epic Games/UE_5.8/Engine/Build/BatchFiles/Mac/GenerateProjectFiles.sh" -project="/Users/leovonschwind/sailing/Sailing.uproject" -game -engine
```

## Architecture

### Module & Dependencies
Single module "Sailing" depending on: Core, CoreUObject, Engine, InputCore, EnhancedInput, ProceduralMeshComponent, UMG, Slate, SlateCore, Json, Water (+ private: RenderCore, RHI).

### Key Classes and Data Flow

**ASailingGameMode** — Central coordinator. Spawns AWindActor, ALightingSetupActor, and either (fjordmodus, default) AFjordMapManager+AFjordCoastlineActor+AOceanWaterSetupActor or (legacy prosedyremodus) AChunkManager+AOceanPlaneActor on BeginPlay. Manages save/load via USaveGameSailing. Sets default pawn (ASailboatPawn), controller, and HUD classes.

**ASailboatPawn** — Player-controlled sailboat. CapsuleComp (root) is fully physics-simulated (SetSimulatePhysics). Oppdrift/krengning bruker EGEN fjær/demper-pongtongmodell (ApplyPontoonBuoyancy, kalt hver Tick) som sampler Water System sin bølgehøyde direkte via `UWaterBodyComponent::TryQueryWaterInfoClosestToWorldLocation` med `IncludeWaves` (NB: bekvemmelighetsfunksjonen `GetWaterSurfaceInfoAtLocation` setter ALDRI IncludeWaves og returnerer det flate vannplanet) — IKKE UBuoyancyComponent. Pongtong-dempingen begrenses per pongtong til `0.6 * m_eff / dt` (m_eff = effektiv masse sett fra punktet): kreftene påføres eksplisitt én gang per frame, og ubegrenset demping (900 mot m_eff≈7 kg ved baug/akter) var numerisk ustabil og pisket opp ±25° stamping, verre ved lav fps. Fortegn: positiv Pitch/Roll i UE er rotasjon om −Right/−Forward. Årsak (verifisert i PIE): Chaos-simulerte kropper genererer ikke pålitelige overlap-events mot Water-pluginets QUERY_ONLY-havkollisjon, så UBuoyancyComponents interne "er jeg i vann"-sporing (CurrentWaterBodyComponents, populert via AWaterBody::NotifyActorBeginOverlap) forble tom uansett kollisjonsoppsett. Pongtong-fjærkraften er klampet (±2x båtvekt per pongtong) for å unngå at et ubegrenset dempingsledd kan gi en eksplosiv "trampoline"-effekt ved høy inngangsfart. Seilkraft fra en polar-kurve-vindmodell × riggens `TrimEfficiency` påføres som massefri AddForce (fremdrift) + separat AddTorque (krengning mot le for den TILSYNELATENDE vinden); sving styres via en rorvinkel-tilstand (`RudderAngleDeg`, rate + retur mot midt, svakere virkning i stillstand) som settes som vinkelhastighet. Grunnstøting håndteres via OnComponentHit (filtrert på UProceduralMeshComponent-land) i stedet for manuell sweep. Input: A/D ror, W/S skjøt (hal inn/slakk, slår auto-trim av), T auto-trim på, mus kamera. Tilsynelatende vind beregnes ÉN gang per Tick og lagres som medlemmer (`TrueWindVec`, `ApparentWindVec/Str/AngleDeg/SideSign`) som HUD/rigg leser. **NB vindkonvensjon:** `AWindActor` sine vektorer peker MOT vindkilden (kast advekteres og sprut driver langs `-WindDir`), så tilsynelatende vind er `TrueWindVec + Forward*CurrentSpeed` (pluss, ikke minus — den dreier forover og øker mot vinden). `ApparentWindAngleDeg` er signert, positiv = vind fra styrbord.

**Båtmeshene** (`Content/ModelsV2/`): tre static meshes med pivot bakt inn i geometrien — `SM_Boat_Hull` (skrog+mast+sverd+tofte, i `BoatMesh`), `SM_Boat_Rig` (bom+sprit+seil, i `RigMesh` under `SailRig`-pivoten på mastaksen `(75,0,66.5)`), `SM_Boat_Rudder` (ror+rorhode+rorkult, i `RudderMesh` på rorakselen `(-119,0,0)`). Mangler noen av dem, brukes det gamle kombinerte `Optimist3735` med stillestående rigg/ror. Alle med `SetCastShadow(false)` (skygge på Single Layer Water ser ut som mudder/bunn). Pipeline: `Blender -b Optimist3735.blend -P scripts/blender/export_optimist_parts.py` → `scripts/cache/boat/*.fbx` + manifest → headless `scripts/import_boat_parts.py` (verifiserer trekanttall; FBX-flaggene fra `scripts/blender/CLAUDE.md`). Blender +Y → UE −Y (speilkonsistent). Seilet bygges flatt og enkeltsidig med UV0 av eksportscriptet (masterens seil har innbakt bukt, Solidify og ingen UV — ubrukelig for shader-bukt); spilene utelates (stive bokser over hele korden). **Riggen følger IODA-klassereglene, ikke masteren**: bommen sitter 1680 mm under mastetoppen (bompinnen, CR 3.5.2.13 — masteren hadde bommen i ripehøyde og spriten på den pinnen), og peaken er målt fra klassetegningen (`reference/optimist_main.jpg`): seil 3,2 m², topplinje ~37°, rett akterlik, sprit 2,25 m. Eksportscriptet løfter bommen og bygger spriten på nytt; `MAST_PIVOT` der MÅ stemme med `USailRigComponent::MastPivotLocal`. Skroget er fortsatt ~12 cm grunnere enn klassens stasjonstabell (0,28 mot 0,40 m) — ikke endret.

**USailRigComponent** (`SailRigComponent.h/.cpp`) — Mastepivoten; relativ yaw = bomvinkel (`SetRelativeRotation(0, -BoomAngleDeg, 0)`, verifisert med `--boat-shot`). Modell: skjøten begrenser bare hvor langt UT bommen kan gå: `BoomMag = min(|AWA|, SheetLimitDeg)`, `AoA = |AWA| - BoomMag`; auto-trim setter `SheetLimitDeg = clamp(|AWA| - OptimalAoADeg(20), 5, 85)`. `TrimEfficiency` (0..1, ganges inn i seilkraften) er 1 under auto-trim, faller ved flagring (AoA < LuffAoADeg) og overhaling. Jibb svinger bommen over med `JibeRateDegPerS`. Setter `SailFill/SailSide/Flutter` på seilets MID hver frame. Testkroker: `sailing.BoomTestDeg`, `sailing.FlutterTest`, `sailing.RudderTestDeg`, `sailing.CamYawTest` (kamera-yaw rundt båten), `sailing.SprayTest`, `sailing.Wake 0` (A/B). Logg `[RIGG]` 4/s.

**UWakeRibbonComponent** (`WakeRibbonComponent.h/.cpp`, UProceduralMeshComponent) — Kjølvann: ringbuffer av prøver bak akterspeilet, én strimmel med fast topologi oppdatert per frame med `UpdateMeshSection_LinearColor` (aldri `MarkRenderStateDirty`). Absolutt transform flyttet til akterpunktet, vertekser relative (float-presisjon). Z = Water Systems bølgehøyde + `WakeZOffset` (22 cm: CPU-spørringen gir Gerstner-høyden uten horisontal forskyvning, så den tegnede flaten ligger stedvis 10–30 cm høyere og skjuler strimmelen ved 6 cm). Målt: ingen fps-kostnad. Sprut (`UpdateSpray`): ISM-pool av kameravendte Engine-plan med `M_SprayQuad`, levetid i per-instans custom data 0 (`SetCustomDataValue(..., false)` + én batch-transformoppdatering); spawner ved vannlinjen (kapselsenteret ligger under WaterZ, og partikler under vann resirkuleres straks).

**ASailingPlayerController** — Creates and binds Enhanced Input actions and mapping context programmatically.

**AWindActor** — Global vindmodell (Perlin-basert retning/styrke/kast). Feeds sailboat sail force calculation AND (fjordmodus) AOceanWaterSetupActors bølgeretning/-styrke.

**AOceanWaterSetupActor** (fjordmodus) — Setter opp UE5 Water System for havet: spawner AWaterZone + en AFjordOceanBodyActor (se under) dekkende hele Oslofjordens kjente utstrekning, konstruerer en Gerstner-bølgegenerator og synker bølgeretning/-styrke mot AWindActor. **Bølgene (`UFjordWaveGenerator`, `FjordWaveGenerator.h/.cpp`):** vindsjø i fjordskala (12 bølger, 4–40 m, 4–15 cm, ±30° spredning) som går MED middelvinden — retningen er −`GetMeanWindDirection()` (vindvektorene peker mot kilden; tidligere gikk sjøen mot vinden). Pluginets `UGerstnerWaterWaveGeneratorSimple` har `DirectionAngularSpreadDeg=1325°` som standard (bølger i alle retninger). Retningsbytte uten hopp: Gerstner-fasen er forankret i origo, så et sett kan aldri dreies — i stedet tones et NYTT sett i den nye retningen inn over `WaveCrossfadeSeconds` (15 s, oppdatert hver 0,1 s) når middelvinden har dreid `WaveDirectionChangeDeg` (20°). Vekten skalerer BÅDE amplitude (√w, energibevarende) og krapphet: den horisontale Gerstner-forskyvningen er Steepness/k, uavhengig av amplituden. `PhaseOffset` i `FGerstnerWave` brukes IKKE av motoren. Logg: `[BOLGER] … Største sprang i vannflaten ved båten per steg: N cm` (målt 1–3 cm; gammel retningsendring ga 20–65 cm). Testkrok: `sailing.WaveTestTurnDeg <grader>` (dreier målet etter 10 s) + `--arg -FjordBoatShotDelay=40`. Under overtoning er det 24 bølger (~2 fps). **«Spiraler» i vannet** kom fra materialets tekstur-detaljnormaler (`WaterTextureWaves`: fire panner-noder i fire motsatte diagonaler, ingen netto retning) — dempet med `NearDetailNormalStrength`/`DistantDetailNormalStrength` (0.12/0.1, standard 0.25/0.3) i `ApplyWaterLook`. Bruker Water-pluginets ferdige Water_Material_Ocean-material; fargen settes på komponentens WaterMID (`WaterAbsorption`/`WaterScattering` — Absorption er en DISTANSE per kanal, høyere = klarere; default gir tropisk turkis). Water-pluginets editor-only bekvemmelighetsfunksjoner (SetOceanExtent, FillWaterZoneWithOcean) er ikke tilgjengelige i spillkode — havets VISUELLE utstrekning settes via `UFjordOceanBodyComponent::SetRuntimeOceanExtents` (= ZoneExtent) og kollisjonsboksen via `SetRuntimeCollisionExtents` (se AFjordOceanBodyActor). Aktørskalering virker IKKE (motoren deler OceanExtents på komponentskalaen for å holde verdensstørrelsen fast). NB: hav-splinen beskriver en ØY — vannet genereres UTENFOR splinen ut til OceanExtents. Splinen er derfor en bitteliten (2x2 m) pliktøy i sonens hjørne; en spline rundt hele sonen gir et hav uten vann (båten «svever» — verifisert i PIE). **Vannflaten tegnes bare innenfor havets `Bounds`**: CPU-vannmeshen (`r.Water.WaterMesh.GPUQuadTree`=0) bygger quadtree-fliser (96 m, fordi `MaxDimensionInTiles`=256 tvinger TileSize 2400×4) kun der, og bygges ikke på nytt i økten. Etter oppsettet logges `[VANN] havbounds … | sone … | dekker sonen` — står det «DEKKER IKKE», mangler vannflaten i deler av fjorden (man ser sjøbunnen). Water info-teksturen er 512² som standard; den heves med `WaterInfoResolution` (2048) via `AWaterZone::SetRenderTargetResolution`. `r.Water.WaterInfo.RenderTargetResolutionMax` i DefaultEngine.ini er bare et tak og hever ingenting. NB pakket build: havets info-mesh bygges bare av `WITH_EDITOR`-kode, så et runtime-spawnet hav blir usynlig i en cooket build — krever en lagret havaktør i nivået (ikke gjort).

**AFjordOceanBodyActor / UFjordOceanBodyComponent** (`FjordOceanBodyActor.h/.cpp`) — Underklasser av AWaterBodyOcean/UWaterBodyOceanComponent, nødvendig fordi `CollisionExtents` (den faktiske, ALLTID verdensromsstore havkollisjonsboksen — uavhengig av spline/aktørskalering) og `OceanExtents` (visuell utstrekning) er `protected` med en C++ `friend`-erklæring som kun gjelder AWaterBodyOcean selv, og settefunksjonen er editor-only. Protected-arv via en komponent-underklasse omgår dette. `UFjordOceanBodyComponent`s konstruktør setter også `bAffectsLandscape=false` (MÅ skje i konstruktøren, ikke post-spawn — WaterEditor-modulens `OnLevelActorAdded`-lytter kjører synkront under selve SpawnActor()-kallet og kan ellers henge en automatisert PIE-økt via en modal "Insert New Landscape Edit Layer"-dialog, siden nivået har et — irrelevant, dekorativt — Landscape). Konstruktøren setter også `bCenterOnWaterZone=false`: ellers sentreres havets `CalcBounds` på `SavedZoneLocation`, som for et hav spawnet i spillet er (0,0) når vannmeshen bygges (skrives bare av editor-kode etterpå). Da dekket flisene bare ±halv sone rundt verdens ORIGO (X ≥ −12 km, Y ≥ −20 km): ved Nesøya og i hele søndre fjord manglet vannflaten (verifisert 2026-09-19). Havaktøren står på sonens senter, så bounds rundt aktøren er riktige.

**AOceanPlaneActor** (legacy prosedyremodus, `bUseFjordMap=false`) — Procedural ocean mesh (128x128 grid, 200k unit extent) som følger spilleren. Four stacked layers (Deep/Mid/Shallow/Surface) using the opaque M_OceanVC material; the surface layer animates via vertex displacement.

**ALightingSetupActor** — Spawner et globalt PostProcessVolume (eksponering/bloom/vignette/saturation) og justerer eksisterende DirectionalLight/SkyAtmosphere-aktører i MainOcean.umap for en fotorealistisk "gyllen time"-sjøfølelse. VolumetricCloud/ExponentialHeightFog sitt UTSEENDE røres bevisst ikke herfra — for scene-avhengig til å tunes blindt i kode. Unntak (ren ytelse): skyenes ray-march-samples skaleres ned (`CloudViewSampleCountScale=0.25`, `CloudShadowTracingDistanceKm=5`) — målt med `ProfileGPU` i PIE: CloudView 20 ms → 2–3 ms, 27 → ~49 fps. NB ved FPS-måling fra logg: editoren struper til ~3 fps når den ikke er i forgrunnen (`bThrottleCPUWhenNotForeground`).

**AChunkManager** — Chunk-based procedural island streaming (legacy prosedyremodus). Loads islands within 3 chunks, unloads beyond 5. Max 3 islands per chunk. Deterministic placement via seeded generation. Integrates with save system to restore discovery state.

**AIslandActor** — Én øy med discovery-trigger (USphereComponent). I fjordmodus bruker den **offline-bakt Nanite-terreng** (`FFjordIslandDef.BakedMesh`, satt på `IslandMesh`) når det finnes, ellers prosedural polygon (`LandMesh`, `FjordGeometry`) som fallback. Bakt mesh: pivot = øyas `Position`, z=0 = middelvannstand (aktøren står på WaterZ=100), terrenget fortsetter som sjøbunn under vann — ingen skjørt/Z-hack. Materiale: MID av `/Game/Fjord/M_Land` med `Discovered`-parameter (M_Land MÅ ha Nanite-bruksflagget, ellers «missing usage flag Nanite» + standardmateriale). Legacy chunk-modus: identifiseres med ChunkCoord + IslandIndex, M_Island → M_IslandDiscovered.

**Land-kollisjon (`ECC_FjordLand`)** — Øyer og kystlinje ligger på egen objektkanal (`ECC_GameTraceChannel2`, profil `FjordLand`, definert i `Sailing.h`/`DefaultEngine.ini`). `ASailboatPawn::HandleCapsuleHit` og `IsOverLand` filtrerer på kanalen (ikke komponenttype), så nivåets dekorative Landscape (WorldStatic) aldri teller som land. `IsOverLand` krever i tillegg terreng over `OverLandMinZ` (170): bakt sjøbunn er ikke «land».

**Start/lagring: aldri på land** — `ASailboatPawn::IsOpenWater(Loc, Klaring)` (17 nedstråler mot `ECC_FjordLand`: senter + ringer på 4 m og `Klaring`) krever minst `OpenWaterMinDepth` (90 cm) vann. `IsOverLand` holder IKKE til dette: den godtar strender/grunner under `OverLandMinZ` som «ikke land», men der står båten fast (verifisert: lagret posisjon på Nesøya-stranda gav pitch −16°, 1240 `[GRUNNSTOT]`, null `[REDNING]`). `PlaceAtStart` (kalt av GameMode for lagret posisjon og fjordstart) flytter til nærmeste åpne vann med dobbel klaring (30 m) og vender baugen bort fra land; logg `[START]`. NB: på frame 0 finnes landkollisjonen IKKE ennå (første sjekk svarer alltid «åpent vann»), så pawnen etterprøver fotavtrykket de første 3 s. `GetSaveLocation()` lagrer siste ÅPNE vann (oppdatert 2 Hz), ikke en grunnstøtt posisjon. Test: `scripts/run_fullscreen.sh --no-build --boat-shot --label starttest` (bruker lagret posisjon, lagrer aldri).

**UFjordBenchmarkComponent** — Måleverktøy, kun aktivt med kommandolinjeflagg (via `scripts/run_fullscreen.sh`): `--shots` (faste kamerastasjoner → `renders/landscape/<label>/`), `--bench` (`[FPSBENCH]`-linjer, snitt + p1 per stasjon), `--ground-test` (skyver båten mot Hovedøya; forvent `[GRUNNSTOT]`, null `[REDNING]`), `--water-check` (`[VANNSJEKK]`: 250 m-rutenett over DTM-en; hvert robust sjøpunkt testes for FjordLand-treff over vannet (`LAND_OVER_SJO`) og for å ligge utenfor havets bounds (`UTENFOR_HAVMESH`), pluss havbounds ved oppsett; oppsummering `feil=N` + bilder `vann_*` fra sonens ytterkanter), `--label <navn>`, `--spike-mesh <asset>`. Målekjøringer lagrer aldri spillet og avslutter seg selv. Er skjermen LÅST, henger fullskjermstart for alltid i `FMacWindow::UpdateFullScreenState`; bilder (ikke gyldige fps) kan da tas med `UnrealEditor … -game -windowed -FjordShots`. Baseline 1600x900 Epic: ~61 fps ved indre øyer, 55 i oversikt. Etter vannfiksen 2026-09-19 (`etter-vannfiks`): 50–53 fps ved indre øyer, 50 ved Håøya, 48 i oversikt. Eldre Håøya-tall (~59) er IKKE sammenlignbare: der ble vannflaten ikke tegnet i det hele tatt.

**ASailingHUD** — Renders compass with wind indicator, speed info, discovery popup (4s duration), and discovery counter. Enheter: spillet er i cm (`CurrentSpeed` i cm/s, vind i cm/s); `SpeedToKnots = 1/51.444`, `WindStrengthToMs = 0.01`. NB: `SpeedToKnots` var 0.00625 fram til 2026-09-19 og viste ~3,1× for lav fart — sammenlign alltid spillerens HUD-tall med `fart=` i `[BAATPOS]` (cm/s) før fysikken tunes.

**USaveGameSailing** — Persists discovered islands (TMap with FIslandData), total count, and player location. Save slot: "SailingSave".

**UIslandNameGenerator** — Deterministic Nordic-style names from chunk coordinates and island index.

### Event Flow
Wind → Sailboat (sail force) → ChunkManager (position-based loading) → IslandActor (discovery trigger) → HUD (popup) + SaveGame (persistence)

## Materials

Hav (fjordmodus): `Content/Materials/Water/` inneholder en prosjektkopi av Water-pluginets havmateriale (`M_FjordWater` ← `MI_FjordWaterInst` ← `MI_FjordOcean`) med én tilføyelse: en boksmaske på Opacity Mask (`HullMaskPos/Fwd/HalfExtent`, satt hver frame av `ASailboatPawn::UpdateHullWaterMask`) som klipper bort vannflaten innenfor skroget — Single Layer Water vet ikke at båten fortrenger vann, så uten masken ser cockpiten vannfylt ut. Grafen ble bygget med Python (Break/MakeMaterialAttributes; NB: `SetMaterialAttributes`-noden og `get_material_expression_input_names()` på Make-noden KRASJER editoren ved skripting). Samme script (`scripts/add_shore_foam.py`) legger også inn SKROGSKUM: et bånd rett utenfor skrogmasken (gjenbruker `HullMaskPos/Fwd/HalfExtent`; maskeboksen ligger innenfor skrogets fotavtrykk, så båndet starter ved `HullFoamInner=1.1`), skalert med `HullFoamStrength` ∝ fart fra `UpdateHullWaterMask`.

Båt (headless Python-scripts, alle idempotente): `scripts/create_sail_material_v2.py` → `/Game/ModelsV2/M_SailV2` (seilduk: TwoSided + TwoSidedFoliage for gjennomskinn, WorldPositionOffset-bukt `SailBellyMaxCm·SailFill·SailSide·sin(π·u^0.85)·sin(π·v)` langs riggens lokale +Y + flagring, prosedyrale paneler/sømmer/spilelommer fra UV0; setter seg selv på `SM_Boat_Rig`). `scripts/create_wake_material.py` → `M_Wake` (translucent kjølvannskum). `scripts/create_spray_material.py` → `M_SprayQuad` (unlit sprutdråpe; MÅ ha bruksflagget `InstancedStaticMeshes`, ellers standardmaterialet). Fallgruver i materialskripting: `VertexColor`-nodens alfa er en egen utgang (`out="A"`), ikke ComponentMask A på RGB-utgangen; `Sine`-noden er `sin(2π·x/period)`; `PerInstanceCustomData` bruker `const_default_value`; `TransformVector(Local→World)` ser ikke ISM-instansens transform.

Five materials in Content/Materials/: M_Ocean (translucent), M_OceanVC (vertex-color opaque), M_Boat (brown), M_Island (green), M_IslandDiscovered (bright green). Python scripts at repo root create these inside Unreal's Python environment.

## Wind Model

Sail force uses a polar curve model matching real Optimist dinghy physics. Tuning parameters exposed under "Sailing|WindModel" in the editor:
- NoGoZoneAngle (default 25°): TILSYNELATENDE vinkel der kraften er 0; stiger lineært til CloseHauledForce ved CloseHauledAngle. NB: må være et AWA-tall — på kryss (sann vind ~45°) er AWA ~30–36°, så den gamle verdien 50° (et tall for sann vind) ga `kraft=0` på hele krysset.
- CloseHauledAngle (50°) / CloseHauledForce (0.75): kurven herfra til 90° er lineær mot BeamReachForce.
- BeamReachForce (1.0): kraft ved 90° AWA
- BroadReachForce (1.0): kraft ved ~135° AWA
- RunningForce (0.85): kraft ved 180°
- PolarSharpness (1.0): potens på multiplikatoren

**Fartskalibrering:** polaren er bevisst FLAT — tilsynelatende vind (styrke + dreining forover med farten) former allerede det meste av fartspolaren; de gamle verdiene (0.18/1.0/0.5/0.22 med `DragCoefficient` 0.0012) ga 9–12 kn på slør og 12 kn halv vind i 11 m/s. Polaren, `DragCoefficient` (0.0043) og `SailForceAccelScale` (0.3, gir ~0,9 s fartsrespons) er tilpasset SAMLET mot reell Optimist-fart med `scripts/tuning/optimist_polar_fit.py` (simulerer nøyaktig Tick-modellen, inkl. kapselens `LinearDamping` 0.5 som er en vesentlig del av motstanden; `--fit` tilpasser på nytt). Referanse fra seiler: lens i 9 m/s = 3,5–5,5 kn (skrogfart ~3,5, surf 5–6+). Modellen gir i 9 m/s: kryss 3,3 kn, halv vind 5,6, slør 5,1, lens 4,3 (5,7 i 13 m/s-kast). Endrer du én av verdiene, kjør skriptet. **Feilsøking «båten går for sakte»:** sjekk `eff` og A/M i `[RIGG]`-linjene FØR du rører fysikken — verifisert 2026-09-18 at spillet følger modellen innen ~5 %, og at treg slør skyldtes manuell skjøt som ble stående inne etter kryss (`eff=0.40` → 3,1 kn mot 5,4 kn med auto-trim). HUD-en varsler derfor «OVERHALT, NN% kraft - slakk (S) / auto (T)» (`USailRigComponent::OverSheetAmount` > 0.35 i manuell modus) ved siden av «FLAGRER - hal inn (W)». Fartsterskler følger med: `MaxBoatSpeed` 450, `SpraySpeedThreshold` 230, `RudderFullEffectSpeed` 200, HUD grønn fart > 260.

Polaren evalueres på den TILSYNELATENDE vindvinkelen (samme `ApparentWindAngleDeg` som riggen og HUD-en). HUD shows Norwegian point-of-sail names: I JERN, BIDEVIND, SLØR, HALV VIND, ROMSKJØTS, LENS, pluss «SKJØT: 45° AUTO/MAN», «FLAGRER», og i kompasset bommen (beige strek) og tilsynelatende vind (cyan pil) ved siden av sann vind (gul).

## Blender → Unreal Asset Pipeline

**Blender-konvensjoner:** Les `scripts/blender/CLAUDE.md` før du rører en Blender-scene eller FBX-eksport. Gjenbrukbare helpere: `scripts/blender/sailing_blender_utils.py` (hull fra stasjoner, FBX-eksport, ortho-rendering, collection-oppsett). Master-scene: `Optimist3735.blend` på repo-rot.

### MCP Servers
MCP servers configured in `.cursor/mcp.json`:
- **unrealMCP**: Programmatic Unreal Editor control (`~/.claude/mcp-servers/unreal-mcp/Python/unreal_mcp_server.py`, timeout increased to 60s for import operations)
- **blender**: Blender control via `uvx blender-mcp` (requires Blender addon running)
- **kartverket**: Kartverket sjøkart WMS – `scripts/mcp-kartverket`, tools: get_chart_layers, get_chart_image, get_feature_info
- **overpass**: OpenStreetMap Overpass API – `scripts/mcp-overpass`, tools: overpass_query_bbox, overpass_get_islands, overpass_run_query (for øyer/features i bbox, f.eks. FjordMapData)

### Fjord map data (OSM)
Fallback for 7 Oslofjord-øyene er hardkodet i `FjordMapManager.cpp`. For å synkronisere med OSM: kjør `scripts/fetch_oslofjord_islands_osm.py` (med `uv run --directory scripts/mcp-overpass -- python scripts/fetch_oslofjord_islands_osm.py`) og lim den utskrevne C++-blokken inn i `FjordMapManager.cpp`. Valgfritt: `scripts/fjord_data.json` inneholder samme data; `scripts/create_fjord_map_data_asset.py` kan kjøres i Unreal Editor (Python Console) for å opprette/oppdatere en UFjordMapData-asset på `/Game/Fjord/OslofjordMapData`. Sett FjordMapDataPath på FjordMapManager i nivået til den asseten for å bruke den i stedet for fallback.
- **Fastland (Landmasses):** kystlinje-ways hentes for et område som dekker hele vannsonen (`CLIP_RECT_M` + 2 km), klippes mot sonens rektangel og lukkes langs rektangelkanten etter OSM-regelen «land til VENSTRE» (`assemble_land`). Den gamle metoden (lukk mot punktenes bbox på siden uten øyenes midtpunkt) hadde et 1,9 km hull og en `perim_t`-feil, og ga en selvkryssende ring som la 34 km² sjø (Nesøya, Sandvika, ni øyer) under en flat landflate på z=190. Skriptet validerer nå mot DTM-en og NEKTER å skrive `fjord_data.json` hvis en ring krysser seg selv, inneholder en øyposisjon eller dekker > 0,5 km² sjø mer enn 150 m innenfor kysten (korrekt fastland: 0,23 km² — elvemunninger som DTM-en har på ~0 m). Oppdater bare fastlandet (øyene styrer bakte assets): `uv run --directory scripts/mcp-overpass --with numpy --with shapely -- python ../fetch_oslofjord_islands_osm.py --output json --landmasses-only`. Overpass-svaret mellomlagres i `scripts/cache/osm/` (`--refresh` henter på nytt; Overpass gir ofte 504). Asseten oppdateres headless med `UnrealEditor-Cmd … -run=pythonscript -script=<abs>/scripts/create_fjord_map_data_asset.py`, som nå BEVARER øyenes `BakedMesh`/`BakeData` (logg `[FJORDDATA] … med bakt terreng=N`). NB: `bake_land.py` bruker også Landmasses til å avgjøre øytilhørighet — ny bake kan gi litt andre kystkanter.

### Bakt øy-terreng (Nanite)
Pipeline: `scripts/fetch_island_dtm.py` (Kartverket WCS, 1 m DTM per øy → `scripts/cache/dtm/`) → `scripts/bake/bake_land.py` (høydefelt + syntetisk sjøbunn → `.glb` render + kollisjon) → `scripts/bake/import_baked_land.py` (headless UE-import, Nanite, kompleks kollisjon, kobler `BakedMesh` i `/Game/Fjord/OslofjordMapData`). Alt i ett: `scripts/bake/rebuild_land.sh` (editoren LUKKET, ~5 min). **`Content/Fjord/Land/` (~300 MB), `Content/Fjord/Surfaces/` og `scripts/cache/` ligger utenfor git** — kjør rebuild etter fersk klone; uten assetene brukes prosedural fallback.
- DTM hentes i tjenestens eget CRS (EPSG:25833, `RESX/RESY=1`) og resamples til spillets ekvirektangulære gitter. **Ikke** be WCS-en om EPSG:4326: det ser ut til å virke, men gir et grovere oversiktsnivå med ~16 m flissømmer. Hav kommer som ~0 m, ikke nodata.
- I stup har lidar få bakkepunkter, så DTM-en interpolerer bratte flater som store plane trekanter. Baken glatter disse og legger på fraktal bergdetalj (`add_cliff_detail`), og eksporterer egne verteksnormaler (uten dem blir shadingen hard/fasettert etter import).
- Landmateriale `M_LandV2` (`scripts/create_land_material_v2.py`, bygges headless): klippe (triplanar, fra helning) / strand + våtbånd (fra høyde over vann) / gress / skogbunn (fra flyfotoets lyshet), flyfoto som fargetone nært og hovedbilde på avstand. Alle terskler er skalarparametre. CC0-teksturer fra Poly Haven via `scripts/fetch_surface_textures.py` → `Content/Fjord/Surfaces/` (utenfor git). Fastlandet (prosedural, flat på z=190) bruker fortsatt `M_Land`.
- Vegetasjon: `scripts/bake/make_trees.py` (furu/gran/bjørk/einer som ugjennomsiktig Nanite-geometri, farget via palett-tekstur + UV0 — verteksfarger overlevde IKKE glTF-importen) → `bake_vegetation.py` (plassering fra OSM-skogflater, helning, høyde o.h., avstand til bygg/kyst; ~263 000 instanser) → `import_vegetation.py` (`UFjordIslandBakeData` per øy med pakkede instanser, koblet via `FFjordIslandDef.BakeData`). `AIslandActor::BuildBakedInstances` lager én `UInstancedStaticMeshComponent` per mesh. NB: komponenter laget under kjøring må ha SAMME mobilitet som roten, ellers avvises festingen («is not static, cannot attach») og de blir liggende i verdens origo.
- Strandskum: `scripts/bake/bake_shore_field.py` baker avstand-til-kyst (4 m/px, 0–40 m) til `T_ShoreDistance`; `scripts/add_shore_foam.py` kobler Break.BaseColor/Roughness i `M_FjordWater` om via Lerp mot skum (bånd × bølging × støy), idempotent via desc-tag «SHOREFOAM». Parametre (`FoamWidth/Strength/Speed/Phase`, `FoamColor`) kan tunes i MI. Water-pluginets WaterInfo er for grov (24×40 km på 2048 px) til skum, og **DepthFade gir 0 overalt i Single Layer Water** (hele havet blir skum) — ikke bruk den der. `get_material_expression_input_names()` på Make-noden krasjer kun interaktivt; i commandlet går det fint.
- `scripts/fetch_oslofjord_features_osm.py` → `scripts/fjord_features.json`: OSM-skog, bygg, brygger, strender, fyr og sjømerker MED geometri og tags (grunnlag for vegetasjon/bygg/sjømerker).
- Kysten følger DTM-en; OSM-ringene avgjør bare hvilken øy en landcelle tilhører (nærmeste ring), så land aldri tegnes to ganger.
- glTF-akser: spill (X,Y,Z-opp, meter) → glTF (X,Z,Y) + invertert vinding. Filnavn = asset-navn (Interchange navngir etter fil).
- Headless Python: `UnrealEditor-Cmd Sailing.uproject -run=pythonscript -script=<abs sti> -unattended -nosplash -nullrhi`. Editor-subsystemer er `None` i commandlet — bruk `set_editor_property` direkte (f.eks. `nanite_settings`). `UnrealEditor-Cmd` uten prosjekt henger.
- Nanite er verifisert på denne Mac-en (M3 Pro, SM6): `[FPSBENCH] nanite plattformstotte=1 ibruk=1`.

### Start Blender MCP
Blender MCP has two parts: (1) an addon inside Blender that runs a socket server, and (2) the MCP server in Cursor (`uvx blender-mcp`). Cursor starts the MCP server automatically; you only need to make Blender listen.

**First time — install the addon:**
1. Download **addon.py** from [blender-mcp GitHub](https://github.com/ahujasid/blender-mcp) (addon / Release or repo).
2. Open **Blender**.
3. **Edit → Preferences → Add-ons**.
4. Click **Install…** and select `addon.py`.
5. Enable the addon: check **Interface: Blender MCP**.

**Each time you want to use MCP:**
1. Open **Blender** (the project/scene you want Cursor to work with).
2. Open the **3D View sidebar** (press **N** if hidden).
3. Find the **BlenderMCP** tab in the sidebar.
4. Click **Connect to Claude** (this starts the socket server in Blender on port 9876).
5. Leave Blender open. Cursor runs `uvx blender-mcp` when you use Blender tools; it connects to Blender when the addon is in “Connect” state.

**Troubleshooting:** “Could not connect to Blender” means the addon is not installed or “Connect to Claude” was not clicked; only one MCP client (Cursor or Claude Desktop) should be used. Timeouts: break large operations into smaller steps; the first command may occasionally fail—try again. Default port is 9876; `BLENDER_HOST` and `BLENDER_PORT` can override.

### FBX Import via MCP
Use `execute_python` to import FBX files programmatically:
```python
import unreal
task = unreal.AssetImportTask()
task.filename = '/path/to/model.fbx'
task.destination_path = '/Game/Models'
task.destination_name = 'AssetName'
task.automated = True
task.save = True
task.replace_existing = True
options = unreal.FbxImportUI()
options.import_mesh = True
options.import_materials = True
options.import_as_skeletal = False
options.static_mesh_import_data.combine_meshes = True
task.options = options
unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])
```

A helper script `import_fbx.py` at repo root wraps this with `import_fbx(path, dest, name)`.

### Island Model
Island mesh created in Blender (`Island.fbx`, exported at repo root). IslandActor loads `/Game/Models/Island` with fallback to basic Cube. The Blender model includes 5 material slots: Island_Grass, Island_Trunk, Island_Canopy, Island_Rock, Island_Bush_Mat.

## MCP Plugin Internals (UnrealMCP)

### Architecture
The plugin (`Plugins/UnrealMCP/`) runs a TCP server on port 55557. Commands arrive on a background thread and are dispatched to the game thread. Command handlers are split into classes:
- **FUnrealMCPEditorCommands** — Actor manipulation, viewport, screenshots, Python execution
- **FUnrealMCPBlueprintCommands** — Blueprint creation, components, properties
- **FUnrealMCPBlueprintNodeCommands** — Event graph nodes, connections, variables
- **FUnrealMCPProjectCommands** — Input mappings
- **FUnrealMCPUMGCommands** — UMG widget blueprints
- **FUnrealMCPMaterialCommands** — Material creation, expressions, compilation (currently broken — commands sent to this handler return no response; root cause unknown)

### execute_python: Ticker-Based Execution (Critical)
`execute_python` uses `FTSTicker::GetCoreTicker().AddTicker()` instead of `AsyncTask(ENamedThreads::GameThread, ...)`. This is essential because Python code that calls `ImportAssetTasks` triggers `WaitUntilTasksComplete`, which pumps the game thread task queue. Running inside an `AsyncTask` (task graph task) causes recursion in `FNamedTaskThread::ProcessTasksUntilIdle` → assertion failure and crash. The ticker runs during normal engine tick, outside the task graph, avoiding this recursion.

### MCP Python Server
Located at `~/.claude/mcp-servers/unreal-mcp/Python/unreal_mcp_server.py`. Key details:
- `get_unreal_connection()` returns a reusable `UnrealConnection` object; `send_command()` always reconnects per command (Unreal closes connections after each response)
- The ping test (`socket.sendall(b'\x00')`) was removed from `get_unreal_connection()` since it interfered with the reconnect logic
- Socket timeouts set to 60s to accommodate long operations like FBX import
- MaterialCommands on the C++ side is broken, so `execute_python` was moved to EditorCommands routing in `UnrealMCPBridge.cpp`

### Troubleshooting
- **"Failed to connect to Unreal Engine"**: Ensure Unreal Editor GUI is running (not `UnrealEditor-Cmd`). Check `lsof -i :55557` to verify the TCP server.
- **Task graph recursion crash**: If adding new Python commands that call blocking UE APIs (import, compile, etc.), use `FTSTicker` instead of `AsyncTask`.
- **MaterialCommands not responding**: All commands routed to `FUnrealMCPMaterialCommands::HandleCommand` silently fail (no TCP response). Use `execute_python` as a workaround for material operations.
