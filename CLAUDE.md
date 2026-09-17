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

**ASailboatPawn** — Player-controlled sailboat. CapsuleComp (root) is fully physics-simulated (SetSimulatePhysics). Oppdrift/krengning bruker EGEN fjær/demper-pongtongmodell (ApplyPontoonBuoyancy, kalt hver Tick) som sampler Water System sin bølgehøyde direkte via `UWaterBodyComponent::TryQueryWaterInfoClosestToWorldLocation` med `IncludeWaves` (NB: bekvemmelighetsfunksjonen `GetWaterSurfaceInfoAtLocation` setter ALDRI IncludeWaves og returnerer det flate vannplanet) — IKKE UBuoyancyComponent. Pongtong-dempingen begrenses per pongtong til `0.6 * m_eff / dt` (m_eff = effektiv masse sett fra punktet): kreftene påføres eksplisitt én gang per frame, og ubegrenset demping (900 mot m_eff≈7 kg ved baug/akter) var numerisk ustabil og pisket opp ±25° stamping, verre ved lav fps. Fortegn: positiv Pitch/Roll i UE er rotasjon om −Right/−Forward. Årsak (verifisert i PIE, se `scripts/` for research-notater): Chaos-simulerte kropper genererer ikke pålitelige overlap-events mot Water-pluginets QUERY_ONLY-havkollisjon, så UBuoyancyComponents interne "er jeg i vann"-sporing (CurrentWaterBodyComponents, populert via AWaterBody::NotifyActorBeginOverlap) forble tom uansett kollisjonsoppsett. Pongtong-fjærkraften er klampet (±2x båtvekt per pongtong) for å unngå at et ubegrenset dempingsledd kan gi en eksplosiv "trampoline"-effekt ved høy inngangsfart. Seilkraft fra en polar-kurve-vindmodell påføres som massefri AddForce (fremdrift) + separat AddTorque (krengning); sving styres via direkte vinkelhastighet. Grunnstøting håndteres via OnComponentHit (filtrert på UProceduralMeshComponent-land) i stedet for manuell sweep. Uses Enhanced Input for turning and camera control.

**ASailingPlayerController** — Creates and binds Enhanced Input actions and mapping context programmatically.

**AWindActor** — Global vindmodell (Perlin-basert retning/styrke/kast). Feeds sailboat sail force calculation AND (fjordmodus) AOceanWaterSetupActors bølgeretning/-styrke.

**AOceanWaterSetupActor** (fjordmodus) — Setter opp UE5 Water System for havet: spawner AWaterZone + en AFjordOceanBodyActor (se under) dekkende hele Oslofjordens kjente utstrekning, konstruerer en Gerstner-bølgegenerator (UGerstnerWaterWaveGeneratorSimple) og synker bølgeretning/-styrke mot AWindActor. Bruker Water-pluginets ferdige Water_Material_Ocean-material; fargen settes på komponentens WaterMID (`WaterAbsorption`/`WaterScattering` — Absorption er en DISTANSE per kanal, høyere = klarere; default gir tropisk turkis). Water-pluginets editor-only bekvemmelighetsfunksjoner (SetOceanExtent, FillWaterZoneWithOcean) er ikke tilgjengelige i spillkode — havets VISUELLE utstrekning settes via `UFjordOceanBodyComponent::SetRuntimeOceanExtents` (= ZoneExtent) og kollisjonsboksen via `SetRuntimeCollisionExtents` (se AFjordOceanBodyActor). Aktørskalering virker IKKE (motoren deler OceanExtents på komponentskalaen for å holde verdensstørrelsen fast). NB: hav-splinen beskriver en ØY — vannet genereres UTENFOR splinen ut til OceanExtents. Splinen er derfor en bitteliten (2x2 m) pliktøy i sonens hjørne; en spline rundt hele sonen gir et hav uten vann (båten «svever» — verifisert i PIE).

**AFjordOceanBodyActor / UFjordOceanBodyComponent** (`FjordOceanBodyActor.h/.cpp`) — Underklasser av AWaterBodyOcean/UWaterBodyOceanComponent, nødvendig fordi `CollisionExtents` (den faktiske, ALLTID verdensromsstore havkollisjonsboksen — uavhengig av spline/aktørskalering) og `OceanExtents` (visuell utstrekning) er `protected` med en C++ `friend`-erklæring som kun gjelder AWaterBodyOcean selv, og settefunksjonen er editor-only. Protected-arv via en komponent-underklasse omgår dette. `UFjordOceanBodyComponent`s konstruktør setter også `bAffectsLandscape=false` (MÅ skje i konstruktøren, ikke post-spawn — WaterEditor-modulens `OnLevelActorAdded`-lytter kjører synkront under selve SpawnActor()-kallet og kan ellers henge en automatisert PIE-økt via en modal "Insert New Landscape Edit Layer"-dialog, siden nivået har et — irrelevant, dekorativt — Landscape).

**AOceanPlaneActor** (legacy prosedyremodus, `bUseFjordMap=false`) — Procedural ocean mesh (128x128 grid, 200k unit extent) som følger spilleren. Four stacked layers (Deep/Mid/Shallow/Surface) using the opaque M_OceanVC material; the surface layer animates via vertex displacement.

**ALightingSetupActor** — Spawner et globalt PostProcessVolume (eksponering/bloom/vignette/saturation) og justerer eksisterende DirectionalLight/SkyAtmosphere-aktører i MainOcean.umap for en fotorealistisk "gyllen time"-sjøfølelse. VolumetricCloud/ExponentialHeightFog sitt UTSEENDE røres bevisst ikke herfra — for scene-avhengig til å tunes blindt i kode. Unntak (ren ytelse): skyenes ray-march-samples skaleres ned (`CloudViewSampleCountScale=0.25`, `CloudShadowTracingDistanceKm=5`) — målt med `ProfileGPU` i PIE: CloudView 20 ms → 2–3 ms, 27 → ~49 fps. NB ved FPS-måling fra logg: editoren struper til ~3 fps når den ikke er i forgrunnen (`bThrottleCPUWhenNotForeground`).

**AChunkManager** — Chunk-based procedural island streaming (legacy prosedyremodus). Loads islands within 3 chunks, unloads beyond 5. Max 3 islands per chunk. Deterministic placement via seeded generation. Integrates with save system to restore discovery state.

**AIslandActor** — Én øy med discovery-trigger (USphereComponent). I fjordmodus bruker den **offline-bakt Nanite-terreng** (`FFjordIslandDef.BakedMesh`, satt på `IslandMesh`) når det finnes, ellers prosedural polygon (`LandMesh`, `FjordGeometry`) som fallback. Bakt mesh: pivot = øyas `Position`, z=0 = middelvannstand (aktøren står på WaterZ=100), terrenget fortsetter som sjøbunn under vann — ingen skjørt/Z-hack. Materiale: MID av `/Game/Fjord/M_Land` med `Discovered`-parameter (M_Land MÅ ha Nanite-bruksflagget, ellers «missing usage flag Nanite» + standardmateriale). Legacy chunk-modus: identifiseres med ChunkCoord + IslandIndex, M_Island → M_IslandDiscovered.

**Land-kollisjon (`ECC_FjordLand`)** — Øyer og kystlinje ligger på egen objektkanal (`ECC_GameTraceChannel2`, profil `FjordLand`, definert i `Sailing.h`/`DefaultEngine.ini`). `ASailboatPawn::HandleCapsuleHit` og `IsOverLand` filtrerer på kanalen (ikke komponenttype), så nivåets dekorative Landscape (WorldStatic) aldri teller som land. `IsOverLand` krever i tillegg terreng over `OverLandMinZ` (170): bakt sjøbunn er ikke «land».

**UFjordBenchmarkComponent** — Måleverktøy, kun aktivt med kommandolinjeflagg (via `scripts/run_fullscreen.sh`): `--shots` (faste kamerastasjoner → `renders/landscape/<label>/`), `--bench` (`[FPSBENCH]`-linjer, snitt + p1 per stasjon), `--ground-test` (skyver båten mot Hovedøya; forvent `[GRUNNSTOT]`, null `[REDNING]`), `--label <navn>`, `--spike-mesh <asset>`. Målekjøringer lagrer aldri spillet og avslutter seg selv. Baseline 1600x900 Epic: ~61 fps ved indre øyer, 55 i oversikt.

**ASailingHUD** — Renders compass with wind indicator, speed info, discovery popup (4s duration), and discovery counter.

**USaveGameSailing** — Persists discovered islands (TMap with FIslandData), total count, and player location. Save slot: "SailingSave".

**UIslandNameGenerator** — Deterministic Nordic-style names from chunk coordinates and island index.

### Event Flow
Wind → Sailboat (sail force) → ChunkManager (position-based loading) → IslandActor (discovery trigger) → HUD (popup) + SaveGame (persistence)

## Materials

Hav (fjordmodus): `Content/Materials/Water/` inneholder en prosjektkopi av Water-pluginets havmateriale (`M_FjordWater` ← `MI_FjordWaterInst` ← `MI_FjordOcean`) med én tilføyelse: en boksmaske på Opacity Mask (`HullMaskPos/Fwd/HalfExtent`, satt hver frame av `ASailboatPawn::UpdateHullWaterMask`) som klipper bort vannflaten innenfor skroget — Single Layer Water vet ikke at båten fortrenger vann, så uten masken ser cockpiten vannfylt ut. Grafen ble bygget med Python (Break/MakeMaterialAttributes; NB: `SetMaterialAttributes`-noden og `get_material_expression_input_names()` på Make-noden KRASJER editoren ved skripting).

Five materials in Content/Materials/: M_Ocean (translucent), M_OceanVC (vertex-color opaque), M_Boat (brown), M_Island (green), M_IslandDiscovered (bright green). Python scripts at repo root create these inside Unreal's Python environment.

## Wind Model

Sail force uses a polar curve model matching real Optimist dinghy physics. Tuning parameters exposed under "Sailing|WindModel" in the editor:
- NoGoZoneAngle (default 45°): angle from wind where sail can't generate force
- CloseHauledForce (0.3): force at edge of no-go zone
- BeamReachForce (1.0): peak force at 90° — fastest point of sail
- BroadReachForce (0.5): force at ~135°
- RunningForce (0.35): force at 180° (dead downwind, drag only)

HUD shows Norwegian point-of-sail names: I JERN, BIDEVIND, SLOR, HALV VIND, ROMSKJØTS, LENS.

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

### Bakt øy-terreng (Nanite)
Pipeline: `scripts/fetch_island_dtm.py` (Kartverket WCS, 1 m DTM per øy → `scripts/cache/dtm/`) → `scripts/bake/bake_land.py` (høydefelt + syntetisk sjøbunn → `.glb` render + kollisjon) → `scripts/bake/import_baked_land.py` (headless UE-import, Nanite, kompleks kollisjon, kobler `BakedMesh` i `/Game/Fjord/OslofjordMapData`). Alt i ett: `scripts/bake/rebuild_land.sh` (editoren LUKKET, ~5 min). **`Content/Fjord/Land/` (~300 MB) og `scripts/cache/` ligger utenfor git** — kjør rebuild etter fersk klone; uten assetene brukes prosedural fallback.
- Spillets projeksjon er ekvirektangulær fra lon/lat, så WCS i EPSG:4326 gir rutenett direkte på spillets gitter (ingen reprojeksjon). Hav kommer som ~0 m, ikke nodata.
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
