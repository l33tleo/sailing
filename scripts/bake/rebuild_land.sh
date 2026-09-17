#!/bin/bash
# Regenererer bakt øy-terreng fra kildedata: DTM-henting → bake → headless import til Unreal.
# De ferdige assetene (Content/Fjord/Land, ~300 MB) ligger IKKE i git — kjør dette etter fersk klone.
# Uten dem faller spillet tilbake til de prosedurale polygon-øyene (BakedMesh-referansen er myk).
#
# Krever: uv, bygget SailingEditor, og at Unreal Editor er LUKKET.
#   scripts/bake/rebuild_land.sh              alt
#   scripts/bake/rebuild_land.sh --no-fetch   bruk eksisterende scripts/cache/dtm
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
UE_CMD="/Users/Shared/Epic Games/UE_5.8/Engine/Binaries/Mac/UnrealEditor-Cmd"
LOG="$ROOT/Saved/Logs/BakeImport.log"

if pgrep -x UnrealEditor >/dev/null; then
	echo "Unreal Editor kjører. Lukk den først (Cmd+Q)." >&2
	exit 1
fi

cd "$ROOT"
if [[ "${1:-}" != "--no-fetch" ]]; then
	echo "== Henter 1 m DTM per øy (hopper over det som finnes i cache) =="
	uv run --directory scripts/mcp-kartverket -- python ../fetch_island_dtm.py
fi

echo "== Baker terreng =="
uv run --with numpy --with scipy --with pillow --with trimesh --with fast-simplification \
	python scripts/bake/bake_land.py

echo "== Importerer til Unreal (headless, ~4 min) =="
"$UE_CMD" "$ROOT/Sailing.uproject" -run=pythonscript \
	-script="$ROOT/scripts/bake/import_baked_land.py" \
	-unattended -nosplash -nullrhi -abslog="$LOG" >/dev/null 2>&1 || true
grep -a "BAKEIMPORT\|LogPython: Error" "$LOG" | grep -v "Display" | sed 's/^.*\[BAKEIMPORT\]/[BAKEIMPORT]/' | tail -5
grep -aq "koblet .* øyer til bakt mesh" "$LOG" || { echo "Importen feilet — se $LOG" >&2; exit 1; }
