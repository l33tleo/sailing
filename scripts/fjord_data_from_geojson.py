#!/usr/bin/env python3
"""
Convert GeoJSON (coastline) and island list to Unreal-friendly data for UFjordMapData.

Usage:
  python fjord_data_from_geojson.py --coastline coast.geojson --islands islands.csv
  python fjord_data_from_geojson.py --coastline coast.geojson --islands islands.csv --output json
  python fjord_data_from_geojson.py --coastline coast.geojson --output cpp

Output:
  - Default: prints C++ TArray snippets (CoastlinePoints, Islands) for copy-paste.
  - --output json: writes fjord_data.json (CoastlinePoints + Islands) for use in Unreal or other tools.
  - --output csv: writes coastline.csv and islands.csv.

Coordinate system: 1 Unreal unit = 1 m. Origin at Oslo (default 59.91°N, 10.75°E).
  X = East (positive east), Y = North (positive north).
"""
from __future__ import annotations

import argparse
import csv
import json
import math
import sys
from pathlib import Path

# Oslo harbour reference (WGS84)
ORIGIN_LAT = 59.91
ORIGIN_LON = 10.75


def lat_lon_to_unreal(
    lat: float, lon: float,
    origin_lat: float = ORIGIN_LAT, origin_lon: float = ORIGIN_LON,
) -> tuple[float, float]:
    """Convert WGS84 (lat, lon) to Unreal X,Y in meters (origin at Oslo)."""
    m_per_deg_lat = 111320.0
    m_per_deg_lon = 111320.0 * math.cos(math.radians(origin_lat))
    x = (lon - origin_lon) * m_per_deg_lon
    y = (lat - origin_lat) * m_per_deg_lat
    return (x, y)


def load_coastline_geojson(path: Path, origin_lat: float = ORIGIN_LAT, origin_lon: float = ORIGIN_LON) -> list[tuple[float, float]]:
    """
    Load first polygon or line from GeoJSON.
    Coordinates in GeoJSON are [lon, lat]. Returns list of (x, y) in Unreal units.
    """
    with open(path, encoding="utf-8") as f:
        data = json.load(f)

    coords = []
    if data.get("type") == "FeatureCollection":
        for feature in data.get("features", []):
            geom = feature.get("geometry")
            if not geom:
                continue
            coords = _coords_from_geometry(geom, origin_lat, origin_lon)
            if coords:
                break
    elif data.get("type") == "Feature":
        coords = _coords_from_geometry(data.get("geometry", {}), origin_lat, origin_lon)
    elif data.get("type") in ("Polygon", "LineString"):
        coords = _coords_from_geometry(data, origin_lat, origin_lon)

    return coords


def _coords_from_geometry(
    geom: dict,
    origin_lat: float = ORIGIN_LAT,
    origin_lon: float = ORIGIN_LON,
) -> list[tuple[float, float]]:
    if not geom:
        return []
    gtype = geom.get("type")
    coords_raw = geom.get("coordinates")
    if not coords_raw:
        return []

    def to_xy(lon: float, lat: float) -> tuple[float, float]:
        return lat_lon_to_unreal(lat, lon, origin_lat, origin_lon)

    if gtype == "Polygon":
        ring = coords_raw[0]
        return [to_xy(lon, lat) for lon, lat in ring]
    if gtype == "LineString":
        return [to_xy(lon, lat) for lon, lat in coords_raw]
    if gtype == "MultiPolygon":
        ring = coords_raw[0][0]
        return [to_xy(lon, lat) for lon, lat in ring]
    return []


def load_islands_csv(
    path: Path,
    origin_lat: float = ORIGIN_LAT,
    origin_lon: float = ORIGIN_LON,
) -> list[dict]:
    """
    CSV columns: name, lat, lon [, scale].
    scale defaults to 3.0.
    """
    islands = []
    with open(path, encoding="utf-8", newline="") as f:
        reader = csv.DictReader(f)
        for row in reader:
            name = (row.get("name") or row.get("Name") or "").strip()
            try:
                lat = float(row.get("lat", row.get("Lat", 0)))
                lon = float(row.get("lon", row.get("Lon", row.get("long", 0))))
            except (TypeError, ValueError):
                continue
            scale = 3.0
            if "scale" in row:
                try:
                    scale = float(row["scale"])
                except (TypeError, ValueError):
                    pass
            x, y = lat_lon_to_unreal(lat, lon, origin_lat, origin_lon)
            islands.append({"name": name, "position": (x, y), "scale": scale})
    return islands


def coastline_to_cpp(points: list[tuple[float, float]]) -> str:
    lines = ["TArray<FVector2D> CoastlinePoints = {"]
    for x, y in points:
        lines.append(f"    FVector2D({x:.1f}, {y:.1f}),")
    if lines[-1].endswith(","):
        lines[-1] = lines[-1][:-1]
    lines.append("};")
    return "\n".join(lines)


def islands_to_cpp(islands: list[dict]) -> str:
    lines = ["TArray<FFjordIslandDef> Islands = {"]
    for i in islands:
        x, y = i["position"]
        name = i["name"].replace('"', '\\"')
        scale = i["scale"]
        lines.append(f'    {{ TEXT("{name}"), FVector2D({x:.1f}, {y:.1f}), {scale}f }},')
    if lines[-1].endswith(","):
        lines[-1] = lines[-1][:-1]
    lines.append("};")
    return "\n".join(lines)


def main() -> None:
    ap = argparse.ArgumentParser(description="Convert GeoJSON/CSV to fjord map data for Unreal")
    ap.add_argument("--coastline", type=Path, help="GeoJSON file with coastline polygon/line")
    ap.add_argument("--islands", type=Path, help="CSV: name, lat, lon [, scale]")
    ap.add_argument("--origin-lat", type=float, default=ORIGIN_LAT, help="Origin latitude")
    ap.add_argument("--origin-lon", type=float, default=ORIGIN_LON, help="Origin longitude")
    ap.add_argument("--output", choices=["cpp", "json", "csv"], default="cpp",
                    help="Output format: cpp (default), json, or csv")
    ap.add_argument("--out-dir", type=Path, default=Path("."),
                    help="Output directory for json/csv files")
    args = ap.parse_args()

    origin_lat = args.origin_lat
    origin_lon = args.origin_lon

    coastline_points: list[tuple[float, float]] = []
    if args.coastline and args.coastline.exists():
        coastline_points = load_coastline_geojson(args.coastline, origin_lat, origin_lon)
        if not coastline_points:
            print("Warning: no coastline coordinates from", args.coastline, file=sys.stderr)
    else:
        if args.coastline:
            print("Warning: coastline file not found:", args.coastline, file=sys.stderr)

    islands: list[dict] = []
    if args.islands and args.islands.exists():
        islands = load_islands_csv(args.islands, origin_lat, origin_lon)
    else:
        if args.islands:
            print("Warning: islands file not found:", args.islands, file=sys.stderr)

    if args.output == "cpp":
        print("// CoastlinePoints (Unreal units, 1 = 1 m, origin Oslo)")
        print(coastline_to_cpp(coastline_points))
        print()
        print("// Islands (FFjordIslandDef)")
        print(islands_to_cpp(islands))
        return

    if args.output == "json":
        out = {
            "CoastlinePoints": [{"X": x, "Y": y} for x, y in coastline_points],
            "Islands": [
                {"Name": i["name"], "Position": {"X": i["position"][0], "Y": i["position"][1]}, "Scale": i["scale"]}
                for i in islands
            ],
            "WorldOrigin": {"X": 0.0, "Y": 0.0},
            "MetersPerUnit": 1.0,
        }
        out_path = args.out_dir / "fjord_data.json"
        out_path.parent.mkdir(parents=True, exist_ok=True)
        with open(out_path, "w", encoding="utf-8") as f:
            json.dump(out, f, indent=2)
        print("Wrote", out_path)
        return

    if args.output == "csv":
        args.out_dir.mkdir(parents=True, exist_ok=True)
        coast_path = args.out_dir / "coastline.csv"
        with open(coast_path, "w", encoding="utf-8", newline="") as f:
            w = csv.writer(f)
            w.writerow(["X", "Y"])
            w.writerows(coastline_points)
        print("Wrote", coast_path)
        isles_path = args.out_dir / "islands.csv"
        with open(isles_path, "w", encoding="utf-8", newline="") as f:
            w = csv.writer(f)
            w.writerow(["name", "X", "Y", "scale"])
            for i in islands:
                w.writerow([i["name"], f"{i['position'][0]:.1f}", f"{i['position'][1]:.1f}", i["scale"]])
        print("Wrote", isles_path)
        return


if __name__ == "__main__":
    main()
