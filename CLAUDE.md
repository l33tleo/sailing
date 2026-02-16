# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project Overview

Sailing is a single-player sailing exploration game built in Unreal Engine 5.7 (macOS only). The player navigates a sailboat through procedurally generated ocean with discoverable Nordic-named islands. Written in C++ with Norwegian documentation.

## Build Commands

Build the project (editor target, Development config):
```bash
"/Users/Shared/Epic Games/UE_5.7/Engine/Build/BatchFiles/Mac/Build.sh" SailingEditor Mac Development -Project="/Users/leovonschwind/sailing/Sailing.uproject" -WaitMutex
```
Append `2>&1 | tail -10` for abbreviated output.

Generate Xcode project files:
```bash
"/Users/Shared/Epic Games/UE_5.7/Engine/Build/BatchFiles/Mac/GenerateProjectFiles.sh" -project="/Users/leovonschwind/sailing/Sailing.uproject" -game -engine
```

## Architecture

### Module & Dependencies
Single module "Sailing" depending on: Core, CoreUObject, Engine, InputCore, EnhancedInput, ProceduralMeshComponent.

### Key Classes and Data Flow

**ASailingGameMode** — Central coordinator. Spawns AWindActor, AChunkManager, and AOceanPlaneActor on BeginPlay. Manages save/load via USaveGameSailing. Sets default pawn (ASailboatPawn), controller, and HUD classes.

**ASailboatPawn** — Player-controlled sailboat with capsule collision, static mesh visual, spring arm + camera. Calculates sail force from wind direction (cosine-based). Simulates wave bobbing. Uses Enhanced Input for turning and camera control.

**ASailingPlayerController** — Creates and binds Enhanced Input actions and mapping context programmatically.

**AWindActor** — Global wind with direction, strength, and slow rotation rate. Feeds into sailboat sail force calculation.

**AChunkManager** — Chunk-based procedural island streaming. Loads islands within 3 chunks, unloads beyond 5. Max 3 islands per chunk. Deterministic placement via seeded generation. Integrates with save system to restore discovery state.

**AIslandActor** — Individual island with discovery trigger (USphereComponent). Broadcasts discovery event, changes material from M_Island to M_IslandDiscovered. Identified by ChunkCoord + IslandIndex.

**AOceanPlaneActor** — Procedural ocean mesh (128x128 grid, 200k unit extent). Animated waves via vertex displacement. Has warmup system (bHideDuringWarmup) to avoid shader compilation artifacts.

**ASailingHUD** — Renders compass with wind indicator, speed info, discovery popup (4s duration), and discovery counter.

**USaveGameSailing** — Persists discovered islands (TMap with FIslandData), total count, and player location. Save slot: "SailingSave".

**UIslandNameGenerator** — Deterministic Nordic-style names from chunk coordinates and island index.

### Event Flow
Wind → Sailboat (sail force) → ChunkManager (position-based loading) → IslandActor (discovery trigger) → HUD (popup) + SaveGame (persistence)

## Materials

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

### MCP Servers
MCP servers configured in `.cursor/mcp.json`:
- **unrealMCP**: Programmatic Unreal Editor control (`~/.claude/mcp-servers/unreal-mcp/Python/unreal_mcp_server.py`, timeout increased to 60s for import operations)
- **blender**: Blender control via `uvx blender-mcp` (requires Blender addon running)
- **kartverket**: Kartverket sjøkart WMS – `scripts/mcp-kartverket`, tools: get_chart_layers, get_chart_image, get_feature_info
- **overpass**: OpenStreetMap Overpass API – `scripts/mcp-overpass`, tools: overpass_query_bbox, overpass_get_islands, overpass_run_query (for øyer/features i bbox, f.eks. FjordMapData)

### Fjord map data (OSM)
Fallback for 7 Oslofjord-øyene er hardkodet i `FjordMapManager.cpp`. For å synkronisere med OSM: kjør `scripts/fetch_oslofjord_islands_osm.py` (med `uv run --directory scripts/mcp-overpass -- python scripts/fetch_oslofjord_islands_osm.py`) og lim den utskrevne C++-blokken inn i `FjordMapManager.cpp`. Valgfritt: `scripts/fjord_data.json` inneholder samme data; `scripts/create_fjord_map_data_asset.py` kan kjøres i Unreal Editor (Python Console) for å opprette/oppdatere en UFjordMapData-asset på `/Game/Fjord/OslofjordMapData`. Sett FjordMapDataPath på FjordMapManager i nivået til den asseten for å bruke den i stedet for fallback.

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
