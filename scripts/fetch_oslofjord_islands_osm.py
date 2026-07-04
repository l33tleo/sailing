#!/usr/bin/env python3
"""
Fetch inner-Oslofjord geometry from OpenStreetMap (Overpass) and convert to Unreal
coordinates (Oslo origin, 1 unit = 1 m).

Unlike the earlier version, this keeps the *full polygon geometry* that Overpass
already returns via `out geom;`:
  - each named island gets an Outline (closed ring) + centroid Position
  - the mainland is fetched as natural=coastline ways, stitched into polylines and
    closed against the query bbox on the land side -> filled Landmasses

Usage:
  uv run --directory scripts/mcp-overpass -- python scripts/fetch_oslofjord_islands_osm.py
  uv run --directory scripts/mcp-overpass -- python scripts/fetch_oslofjord_islands_osm.py --output json

Output:
  - C++ TArray<FFjordIslandDef> snippet (centroid-only) for the compact FjordMapManager fallback.
  - --output json: scripts/fjord_data.json with Outline + Landmasses (feeds the FjordMapData asset).
"""
from __future__ import annotations

import json
import sys
from pathlib import Path

# Paths so we can import from scripts/ and scripts/mcp-overpass
REPO_ROOT = Path(__file__).resolve().parent.parent
SCRIPTS = REPO_ROOT / "scripts"
sys.path.insert(0, str(SCRIPTS))
sys.path.insert(0, str(SCRIPTS / "mcp-overpass"))

from fjord_data_from_geojson import ORIGIN_LAT, ORIGIN_LON, lat_lon_to_unreal
from overpass import get_islands_ql, run_query

# Known scales for original 7 islands (used only as a fallback when an island has no outline)
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

# Douglas-Peucker tolerance in meters (== Unreal units before DistanceScale)
SIMPLIFY_TOLERANCE_M = 8.0


# ---------------------------------------------------------------------------
# Geometry helpers
# ---------------------------------------------------------------------------

def _geom_to_xy(geom: list[dict]) -> list[tuple[float, float]]:
    """Overpass way geometry ([{lat,lon}, ...]) -> Unreal (x, y) list."""
    out = []
    for n in geom:
        if "lat" in n and "lon" in n:
            out.append(lat_lon_to_unreal(float(n["lat"]), float(n["lon"]), ORIGIN_LAT, ORIGIN_LON))
    return out


def _outline_from_element(el: dict) -> list[tuple[float, float]]:
    """Extract an island outline ring from a way or a multipolygon relation."""
    if el.get("type") == "way" and el.get("geometry"):
        return _geom_to_xy(el["geometry"])
    if el.get("type") == "relation" and el.get("members"):
        # Stitch the 'outer' member ways into rings; return the longest.
        outer = [_geom_to_xy(m["geometry"]) for m in el["members"]
                 if m.get("role") == "outer" and m.get("geometry")]
        rings = stitch_ways([w for w in outer if len(w) >= 2])
        if rings:
            return max(rings, key=len)
    return []


def _centroid(points: list[tuple[float, float]]) -> tuple[float, float]:
    if not points:
        return (0.0, 0.0)
    return (sum(p[0] for p in points) / len(points), sum(p[1] for p in points) / len(points))


def _perp_dist(pt, a, b) -> float:
    (px, py), (ax, ay), (bx, by) = pt, a, b
    dx, dy = bx - ax, by - ay
    seg = dx * dx + dy * dy
    if seg == 0:
        return ((px - ax) ** 2 + (py - ay) ** 2) ** 0.5
    t = ((px - ax) * dx + (py - ay) * dy) / seg
    t = max(0.0, min(1.0, t))
    cx, cy = ax + t * dx, ay + t * dy
    return ((px - cx) ** 2 + (py - cy) ** 2) ** 0.5


def simplify(points: list[tuple[float, float]], tol: float) -> list[tuple[float, float]]:
    """Douglas-Peucker. Returns simplified polyline; keeps endpoints."""
    if len(points) < 3:
        return points
    dmax, idx = 0.0, 0
    for i in range(1, len(points) - 1):
        d = _perp_dist(points[i], points[0], points[-1])
        if d > dmax:
            dmax, idx = d, i
    if dmax > tol:
        left = simplify(points[: idx + 1], tol)
        right = simplify(points[idx:], tol)
        return left[:-1] + right
    return [points[0], points[-1]]


def _key(p, prec=0):
    return (round(p[0], prec), round(p[1], prec))


def stitch_ways(ways: list[list[tuple[float, float]]]) -> list[list[tuple[float, float]]]:
    """Join way polylines that share endpoints into longer polylines."""
    remaining = [w[:] for w in ways if len(w) >= 2]
    result: list[list[tuple[float, float]]] = []
    while remaining:
        cur = remaining.pop(0)
        changed = True
        while changed:
            changed = False
            for i, w in enumerate(remaining):
                if _key(w[0]) == _key(cur[-1]):
                    cur = cur + w[1:]
                elif _key(w[-1]) == _key(cur[-1]):
                    cur = cur + w[-2::-1]
                elif _key(w[-1]) == _key(cur[0]):
                    cur = w[:-1] + cur
                elif _key(w[0]) == _key(cur[0]):
                    cur = w[::-1][:-1] + cur
                else:
                    continue
                remaining.pop(i)
                changed = True
                break
        result.append(cur)
    return result


def close_to_bbox(polyline, bmin, bmax, water_center) -> list[tuple[float, float]]:
    """Close an open coastline polyline into a land polygon by walking the bbox
    perimeter. Picks the closure side that does NOT contain the fjord water center
    (land is the outer region)."""
    if _key(polyline[0]) == _key(polyline[-1]):
        return polyline
    (minx, miny), (maxx, maxy) = bmin, bmax
    corners = [(minx, miny), (maxx, miny), (maxx, maxy), (minx, maxy)]

    def perim_t(p):
        x, y = p
        if abs(y - miny) <= abs(y - maxy) and abs(y - miny) <= min(abs(x - minx), abs(x - maxx)):
            return (x - minx) / (maxx - minx)              # bottom edge: 0..1
        if abs(x - maxx) <= abs(x - minx):
            return 1 + (y - miny) / (maxy - miny)          # right edge: 1..2
        if abs(y - maxy) <= abs(y - miny):
            return 3 - (x - minx) / (maxx - minx)          # top edge: 2..3
        return 4 - (y - miny) / (maxy - miny)              # left edge: 3..4

    t_end, t_start = perim_t(polyline[-1]), perim_t(polyline[0])

    def corners_between(forward: bool):
        # Walk the perimeter from t_end to t_start in the given direction, returning
        # the bbox corners passed along the way, in order.
        span = (t_start - t_end) % 4 if forward else (t_end - t_start) % 4
        passed = []
        for c in range(4):
            d = (c - t_end) % 4 if forward else (t_end - c) % 4
            if 0 < d < span:
                passed.append((d, corners[c]))
        passed.sort()
        return [pt for _, pt in passed]

    ring_fwd = polyline + corners_between(True)
    ring_bwd = polyline + corners_between(False)

    def contains(ring, pt) -> bool:
        x, y = pt
        inside = False
        n = len(ring)
        for i in range(n):
            x1, y1 = ring[i]
            x2, y2 = ring[(i + 1) % n]
            if (y1 > y) != (y2 > y):
                xin = x1 + (y - y1) / (y2 - y1) * (x2 - x1)
                if x < xin:
                    inside = not inside
        return inside

    return ring_bwd if contains(ring_fwd, water_center) else ring_fwd


# ---------------------------------------------------------------------------
# Fetch
# ---------------------------------------------------------------------------

def fetch_islands(south, west, north, east):
    raw = run_query(get_islands_ql(south, west, north, east))
    seen: set[str] = set()
    islands = []
    for el in raw.get("elements", []):
        tags = el.get("tags") or {}
        name = (tags.get("name") or tags.get("name:no") or "").strip()
        if not name or name in seen:
            continue
        raw_outline = _outline_from_element(el)
        outline = simplify(raw_outline, SIMPLIFY_TOLERANCE_M) if len(raw_outline) >= 3 else []
        if outline:
            pos = _centroid(outline)
        elif "center" in el:
            pos = lat_lon_to_unreal(float(el["center"]["lat"]), float(el["center"]["lon"]), ORIGIN_LAT, ORIGIN_LON)
        elif el.get("lat") is not None:
            pos = lat_lon_to_unreal(float(el["lat"]), float(el["lon"]), ORIGIN_LAT, ORIGIN_LON)
        else:
            continue
        seen.add(name)
        islands.append({
            "name": name,
            "position": pos,
            "scale": KNOWN_SCALES.get(name, DEFAULT_SCALE),
            "outline": outline,
        })
    islands.sort(key=lambda i: i["position"][1])
    return islands


def fetch_landmasses(south, west, north, east, water_center):
    bbox = f"({south},{west},{north},{east})"
    raw = run_query(f'[out:json];way["natural"="coastline"]{bbox};out geom;')
    ways = [_geom_to_xy(el["geometry"]) for el in raw.get("elements", []) if el.get("geometry")]
    if not ways:
        return []
    xs = [p[0] for w in ways for p in w]
    ys = [p[1] for w in ways for p in w]
    margin = 500.0
    bmin = (min(xs) - margin, min(ys) - margin)
    bmax = (max(xs) + margin, max(ys) + margin)
    rings = []
    for pl in stitch_ways(ways):
        ring = close_to_bbox(pl, bmin, bmax, water_center)
        ring = simplify(ring, SIMPLIFY_TOLERANCE_M)
        if len(ring) >= 3:
            rings.append(ring)
    return rings


def main() -> None:
    # Inner Oslofjord: Drøbak to Oslo (south, west, north, east)
    south, west, north, east = 59.65, 10.5, 59.95, 10.85

    islands = fetch_islands(south, west, north, east)
    water_center = _centroid([i["position"] for i in islands]) if islands else (-5000.0, -12000.0)
    landmasses = fetch_landmasses(south, west, north, east, water_center)

    n_isl_pts = sum(len(i["outline"]) for i in islands)
    n_land_pts = sum(len(r) for r in landmasses)
    print(f"Islands: {len(islands)} ({n_isl_pts} outline pts). "
          f"Landmasses: {len(landmasses)} ({n_land_pts} pts). "
          f"Simplify tol={SIMPLIFY_TOLERANCE_M} m.", file=sys.stderr)

    # C++ snippet: centroid-only, for the compact hardcoded fallback in FjordMapManager
    def cpp_float(f: float) -> str:
        return f"{f:.1f}f"

    lines = ["\t\tTestData->Islands = {"]
    for i in islands:
        x, y = i["position"]
        name = i["name"].replace('"', '\\"')
        lines.append(f'\t\t\t{{ TEXT("{name}"),   FVector2D({cpp_float(x)}, {cpp_float(y)}), {i["scale"]}f }},')
    if lines[-1].endswith(","):
        lines[-1] = lines[-1][:-1]
    lines.append("\t\t};")
    print("// Paste into FjordMapManager.cpp fallback (centroid-only; polygons live in the asset)")
    print("\n".join(lines))

    if "--output" in sys.argv and "json" in sys.argv:
        out = {
            "Landmasses": [[{"X": x, "Y": y} for x, y in r] for r in landmasses],
            "Islands": [
                {
                    "Name": i["name"],
                    "Position": {"X": i["position"][0], "Y": i["position"][1]},
                    "Scale": i["scale"],
                    "Outline": [{"X": x, "Y": y} for x, y in i["outline"]],
                }
                for i in islands
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
