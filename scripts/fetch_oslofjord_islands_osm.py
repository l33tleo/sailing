#!/usr/bin/env python3
"""
Fetch named islands in inner Oslofjord from OpenStreetMap (Overpass), convert to
Unreal coordinates (Oslo origin, 1 unit = 1 m), and output C++ snippet for
FjordMapManager.

Usage:
  python scripts/fetch_oslofjord_islands_osm.py
  python scripts/fetch_oslofjord_islands_osm.py --output json

Output: C++ TArray<FFjordIslandDef> (and optionally fjord_data.json).
"""
from __future__ import annotations

import json
import math
import sys
from pathlib import Path

# Paths so we can import from scripts/ and scripts/mcp-overpass
REPO_ROOT = Path(__file__).resolve().parent.parent
SCRIPTS = REPO_ROOT / "scripts"
sys.path.insert(0, str(SCRIPTS))
sys.path.insert(0, str(SCRIPTS / "mcp-overpass"))

from fjord_data_from_geojson import ORIGIN_LAT, ORIGIN_LON, lat_lon_to_unreal
from overpass import get_islands

# Known scales for original 7 islands (tuned to look right in-game)
KNOWN_SCALES: dict[str, float] = {
    "Hovedøya": 3.0,
    "Lindøya": 2.5,
    "Nakholmen": 2.0,
    "Bleikøya": 2.0,
    "Gressholmen": 2.5,
    "Langøyene": 2.0,
    "Gåsøya": 1.5,
}

DEFAULT_SCALE = 2.0


def main() -> None:
    # Expanded bbox: inner Oslofjord from Drøbak to Oslo (south, west, north, east)
    south, west, north, east = 59.65, 10.5, 59.95, 10.85

    # Fetch all islands from Overpass
    features = get_islands(south, west, north, east)

    # Deduplicate by name (keep first occurrence), skip unnamed
    seen: set[str] = set()
    islands_out: list[dict] = []
    for f in features:
        name = (f.get("name") or "").strip()
        if not name or name in seen:
            continue
        seen.add(name)
        lat, lon = f["lat"], f["lon"]
        x, y = lat_lon_to_unreal(lat, lon, ORIGIN_LAT, ORIGIN_LON)
        scale = KNOWN_SCALES.get(name, DEFAULT_SCALE)
        islands_out.append({
            "name": name,
            "position": (x, y),
            "scale": scale,
        })

    # Sort by Y (south to north) for readability
    islands_out.sort(key=lambda i: i["position"][1])

    print(f"Found {len(islands_out)} named islands.", file=sys.stderr)

    # Compute bounding box for CoastlinePoints (all island positions + margin)
    if islands_out:
        xs = [i["position"][0] for i in islands_out]
        ys = [i["position"][1] for i in islands_out]
        margin = 2000.0  # 2 km margin around outermost islands
        coast_min_x = min(xs) - margin
        coast_max_x = max(xs) + margin
        coast_min_y = min(ys) - margin
        coast_max_y = max(ys) + margin
    else:
        coast_min_x, coast_max_x = -5000.0, 1000.0
        coast_min_y, coast_max_y = -30000.0, 5000.0

    coastline_points = [
        (coast_min_x, coast_min_y),
        (coast_max_x, coast_min_y),
        (coast_max_x, coast_max_y),
        (coast_min_x, coast_max_y),
    ]

    # Output C++
    def cpp_float(f: float) -> str:
        return f"{f:.1f}f"

    lines = ["\t\tTestData->CoastlinePoints = {"]
    for x, y in coastline_points:
        lines.append(f"\t\t\tFVector2D({cpp_float(x)}, {cpp_float(y)}),")
    if lines[-1].endswith(","):
        lines[-1] = lines[-1][:-1]
    lines.append("\t\t};")
    lines.append("\t\tTestData->Islands = {")
    for i in islands_out:
        x, y = i["position"]
        name = i["name"].replace('"', '\\"')
        scale = i["scale"]
        lines.append(f'\t\t\t{{ TEXT("{name}"),   FVector2D({cpp_float(x)}, {cpp_float(y)}), {scale}f }},')
    if lines[-1].endswith(","):
        lines[-1] = lines[-1][:-1]
    lines.append("\t\t};")
    cpp = "\n".join(lines)

    print("// Paste into FjordMapManager.cpp (replace TestData->CoastlinePoints and TestData->Islands)")
    print(cpp)

    # Optional: write JSON
    if "--output" in sys.argv and "json" in sys.argv:
        out = {
            "CoastlinePoints": [{"X": x, "Y": y} for x, y in coastline_points],
            "Islands": [
                {
                    "Name": i["name"],
                    "Position": {"X": i["position"][0], "Y": i["position"][1]},
                    "Scale": i["scale"],
                }
                for i in islands_out
            ],
            "WorldOrigin": {"X": 0.0, "Y": 0.0},
            "MetersPerUnit": 1.0,
        }
        out_path = SCRIPTS / "fjord_data.json"
        with open(out_path, "w", encoding="utf-8") as f:
            json.dump(out, f, indent=2, ensure_ascii=False)
        print("Wrote", out_path, file=sys.stderr)


if __name__ == "__main__":
    main()
