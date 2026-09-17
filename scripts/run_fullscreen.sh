#!/bin/bash
# Bygger og starter Sailing frittstående i EKTE fullskjerm.
#
# Hvorfor: på macOS 27 + UE 5.8 gir vindusmodus (inkl. PIE i editoren) periodiske stopp på ~1 s
# (CAMetalLayer nextDrawable-timeout i MetalRHI). I fullskjerm slår motoren av display-sync og
# stoppene forsvinner (målt 2026-09-17: 0 stopp på 311 s, mot 3–5 forventet). Bruk dette skriptet
# når du vil vurdere hvordan spillet faktisk føles; PIE er fint for funksjonstesting.
#
# Bruk:
#   scripts/run_fullscreen.sh              bygg + start i fullskjerm
#   scripts/run_fullscreen.sh --no-build   hopp over bygging
#   scripts/run_fullscreen.sh --res 1920x1080
#   scripts/run_fullscreen.sh --hitches    logg hakk >60 ms til Saved/Logs/Fullscreen.log
#   scripts/run_fullscreen.sh --quality 2  grafikkvalitet 0–3 (lav/medium/høy/epic; standard er 3)
#   scripts/run_fullscreen.sh --fps 30     lås bildefrekvensen (jevnere enn ulåst ~25–35 fps)
#   scripts/run_fullscreen.sh --force      start selv om editoren er åpen (frarådes, se under)
# Avslutt spillet med Cmd+Q.

set -euo pipefail

UE_ROOT="/Users/Shared/Epic Games/UE_5.8"
PROJECT="$(cd "$(dirname "$0")/.." && pwd)/Sailing.uproject"
EDITOR_BIN="$UE_ROOT/Engine/Binaries/Mac/UnrealEditor.app/Contents/MacOS/UnrealEditor"
LOG="$(dirname "$PROJECT")/Saved/Logs/Fullscreen.log"

BUILD=1
FORCE=0
RES=""
EXEC_CMDS=""

add_cmd() { EXEC_CMDS="${EXEC_CMDS:+$EXEC_CMDS,}$1"; }

while [[ $# -gt 0 ]]; do
	case "$1" in
		--no-build) BUILD=0 ;;
		--force)    FORCE=1 ;;
		--hitches)  add_cmd "t.HitchFrameTimeThreshold 60,stat dumphitches" ;;
		--quality)
			Q="${2:-}"; shift
			[[ "$Q" =~ ^[0-3]$ ]] || { echo "--quality må være 0–3" >&2; exit 2; }
			# ResolutionQuality røres ikke (0 = auto); oppløsning styres med --res.
			for G in ViewDistance AntiAliasing Shadow GlobalIllumination Reflection PostProcess \
				Texture Effects Foliage Shading Landscape; do
				add_cmd "sg.${G}Quality $Q"
			done ;;
		--fps)
			FPS="${2:-}"; shift
			[[ "$FPS" =~ ^[0-9]+$ ]] || { echo "--fps må være et heltall" >&2; exit 2; }
			add_cmd "t.MaxFPS $FPS" ;;
		--res)      RES="${2:-}"; shift ;;
		-h|--help)  sed -n '2,18p' "$0" | sed 's/^# \{0,1\}//'; exit 0 ;;
		*) echo "Ukjent valg: $1 (se --help)" >&2; exit 2 ;;
	esac
	shift
done

# To Unreal-instanser samtidig gir memory pressure-varsel på denne maskinen, og et bygg mens
# editoren er åpen lager bare en Live Coding-patch (spillet ville startet med gammel kode).
if pgrep -x UnrealEditor >/dev/null; then
	if [[ $FORCE -eq 0 ]]; then
		echo "Unreal Editor kjører. Lukk den først (Cmd+Q), eller bruk --force." >&2
		exit 1
	fi
	echo "ADVARSEL: editoren er åpen — hopper over bygging og starter en ekstra instans." >&2
	BUILD=0
fi

if [[ $BUILD -eq 1 ]]; then
	echo "Bygger SailingEditor ..."
	"$UE_ROOT/Engine/Build/BatchFiles/Mac/Build.sh" SailingEditor Mac Development \
		-Project="$PROJECT" -WaitMutex 2>&1 | grep -E "error|warning: |Result:|Total execution" || true
	# grep svelger exit-koden; sjekk resultatet eksplisitt via UBT-loggen.
	if ! grep -q "Result: Succeeded" "$HOME/Library/Application Support/Epic/UnrealBuildTool/Log.txt"; then
		echo "Bygget feilet — starter ikke spillet." >&2
		exit 1
	fi
fi

ARGS=("$PROJECT" -game -fullscreen -log -abslog="$LOG")
if [[ -n "$RES" ]]; then
	if [[ ! "$RES" =~ ^[0-9]+x[0-9]+$ ]]; then
		echo "--res må være på formen BREDDExHØYDE, f.eks. 1920x1080" >&2
		exit 2
	fi
	ARGS+=(-ResX="${RES%x*}" -ResY="${RES#*x}")
fi
if [[ -n "$EXEC_CMDS" ]]; then
	ARGS+=(-ExecCmds="$EXEC_CMDS")
fi

echo "Starter Sailing i fullskjerm (logg: $LOG). Avslutt med Cmd+Q."
exec "$EDITOR_BIN" "${ARGS[@]}"
