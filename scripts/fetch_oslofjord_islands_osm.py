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
  uv run --directory scripts/mcp-overpass --with numpy --with shapely -- python scripts/fetch_oslofjord_islands_osm.py
  ... --output json                      skriv hele scripts/fjord_data.json (øyer + fastland)
  ... --output json --landmasses-only    bytt BARE Landmasses i eksisterende fjord_data.json (øyene
                                         styrer bakte assets og skal ikke endres i forbifarten)

Fastlandet: kystlinje-ways hentes for et område som dekker hele vannsonen, klippes mot sonens
rektangel (CLIP_RECT_M) og lukkes langs rektangelkanten etter OSM-regelen «land til venstre». Den
gamle metoden (lukk mot punktenes bbox på siden uten øyenes midtpunkt) ga en selvkryssende ring som
la ~33 km² sjø under en landflate på z=190. Resultatet valideres mot DTM-en (validate_landmasses):
skriptet nekter å skrive en ring som krysser seg selv, dekker sjø eller en øy.

Output:
  - C++ TArray<FFjordIslandDef> snippet (centroid-only) for the compact FjordMapManager fallback.
  - --output json: scripts/fjord_data.json with Outline + Landmasses (feeds the FjordMapData asset).
"""
from __future__ import annotations

import json
import math
import sys
import time
import zlib
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


# Vannsonen i meter (= AOceanWaterSetupActor ZoneCenter ± ZoneExtent/2, delt på DistanceScale 100).
# Utenfor den finnes ikke vann, så landpolygonene trenger ikke gå lenger.
CLIP_RECT_M = ((-18840.0, -34870.0), (5160.0, 5130.0))
# Overpass-området må dekke hele klipperektangelet (med margin), ellers mangler kystlinjebiter som
# går ut og inn av området — det var det som ga 1,9 km-hullet i den gamle ringen.
QUERY_MARGIN_M = 2000.0
# Skjær mindre enn dette etter forenkling droppes (de blir ellers degenererte 2–3-punktsringer).
MIN_RING_AREA_M2 = 25.0


def _clip_segment(a, b, rect):
    """Liang–Barsky: den delen av segmentet a→b som ligger i rektangelet, eller None."""
    (minx, miny), (maxx, maxy) = rect
    x0, y0 = a
    dx, dy = b[0] - x0, b[1] - y0
    t0, t1 = 0.0, 1.0
    for p, q in ((-dx, x0 - minx), (dx, maxx - x0), (-dy, y0 - miny), (dy, maxy - y0)):
        if p == 0:
            if q < 0:
                return None
            continue
        r = q / p
        if p < 0:
            if r > t1:
                return None
            t0 = max(t0, r)
        else:
            if r < t0:
                return None
            t1 = min(t1, r)
    return (x0 + t0 * dx, y0 + t0 * dy), (x0 + t1 * dx, y0 + t1 * dy)


def clip_polyline(pl, rect):
    """Klipper en polylinje mot rektangelet. Returnerer bitene som ligger inni, i samme retning."""
    pieces, cur = [], []
    for a, b in zip(pl, pl[1:]):
        seg = _clip_segment(a, b, rect)
        if seg is None:
            if cur:
                pieces.append(cur)
                cur = []
            continue
        p0, p1 = seg
        if not cur:
            cur = [p0]
        elif _key(cur[-1], 3) != _key(p0, 3):   # gikk ut og inn igjen mellom to punkter
            pieces.append(cur)
            cur = [p0]
        cur.append(p1)
        if _key(p1, 3) != _key(b, 3):            # segmentet går ut av rektangelet her
            pieces.append(cur)
            cur = []
    if cur:
        pieces.append(cur)
    return [p for p in pieces if len(p) >= 2]


def _perim_t(p, rect) -> float:
    """Posisjon langs rektangelkanten, MOT klokka (y = nord): sør 0..1, øst 1..2, nord 2..3, vest 3..4.
    Punktet klassifiseres etter den NÆRMESTE kanten (den gamle if-kjeden kunne velge feil kant)."""
    (minx, miny), (maxx, maxy) = rect
    x, y = p
    d = {"s": abs(y - miny), "e": abs(x - maxx), "n": abs(y - maxy), "w": abs(x - minx)}
    edge = min(d, key=d.get)
    if edge == "s":
        return (x - minx) / (maxx - minx)
    if edge == "e":
        return 1 + (y - miny) / (maxy - miny)
    if edge == "n":
        return 3 - (x - minx) / (maxx - minx)
    return 4 - (y - miny) / (maxy - miny)


def assemble_land(pieces, rect):
    """Lukker åpne kystlinjebiter (endepunkter på rektangelkanten) til landpolygoner.

    OSM tegner kystlinjen med land til VENSTRE. Et landpolygon med land til venstre går mot klokka,
    så fra et utgangspunkt fortsetter vi mot klokka langs kanten til nærmeste inngangspunkt for en
    kystlinjebit, følger den, og slik videre til vi er tilbake ved første bit."""
    (minx, miny), (maxx, maxy) = rect
    corners = [(1.0, (maxx, miny)), (2.0, (maxx, maxy)), (3.0, (minx, maxy)), (4.0, (minx, miny))]
    starts = [_perim_t(pc[0], rect) for pc in pieces]
    unused = set(range(len(pieces)))
    rings = []
    while unused:
        first = min(unused)
        unused.discard(first)
        ring = list(pieces[first])
        cur = first
        for _ in range(len(pieces) + 1):
            t_end = _perim_t(pieces[cur][-1], rect)
            cands = [(( starts[k] - t_end) % 4.0 or 4.0, k) for k in unused | {first}]
            dist, nxt = min(cands)
            for ct, c in sorted(corners, key=lambda c: (c[0] - t_end) % 4.0 or 4.0):
                if 0.0 < (ct - t_end) % 4.0 < dist:
                    ring.append(c)
            if nxt == first:
                break
            unused.discard(nxt)
            ring.extend(pieces[nxt])
            cur = nxt
        rings.append(ring)
    return rings


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


def _query_bbox_latlon(rect, margin):
    """Klipperektangel (meter) + margin → Overpass-bbox (south, west, north, east)."""
    (minx, miny), (maxx, maxy) = rect
    m_lat = 111320.0
    m_lon = 111320.0 * math.cos(math.radians(ORIGIN_LAT))
    return (ORIGIN_LAT + (miny - margin) / m_lat, ORIGIN_LON + (minx - margin) / m_lon,
            ORIGIN_LAT + (maxy + margin) / m_lat, ORIGIN_LON + (maxx + margin) / m_lon)


def _stitch_directed(ways):
    """Som stitch_ways, men skjøter BARE slutt→start: kystlinjens retning (land til venstre) må
    bevares, og stitch_ways kan snu en way."""
    remaining = [w[:] for w in ways if len(w) >= 2]
    out = []
    while remaining:
        cur = remaining.pop(0)
        changed = True
        while changed:
            changed = False
            for i, w in enumerate(remaining):
                if _key(w[0]) == _key(cur[-1]):
                    cur = cur + w[1:]
                elif _key(w[-1]) == _key(cur[0]):
                    cur = w[:-1] + cur
                else:
                    continue
                remaining.pop(i)
                changed = True
                break
        out.append(cur)
    return out


def fetch_landmasses(rect=CLIP_RECT_M):
    s, w, n, e = _query_bbox_latlon(rect, QUERY_MARGIN_M)
    query = f'[out:json][timeout:180];way["natural"="coastline"]({s:.6f},{w:.6f},{n:.6f},{e:.6f});out geom;'
    # Overpass er treg/ustabil (504) — svaret mellomlagres per spørring; --refresh henter på nytt.
    cache = SCRIPTS / "cache" / "osm" / f"coastline_{zlib.crc32(query.encode()):08x}.json"
    if cache.exists() and "--refresh" not in sys.argv:
        raw = json.loads(cache.read_text(encoding="utf-8"))
        print(f"Kystlinje fra mellomlager {cache.name}", file=sys.stderr)
    else:
        for attempt in range(4):
            try:
                raw = run_query(query)
                break
            except Exception as exc:   # 429/504 fra Overpass: vent og prøv igjen
                if attempt == 3:
                    raise
                print(f"Overpass feilet ({exc}); nytt forsøk om {30 * (attempt + 1)} s", file=sys.stderr)
                time.sleep(30 * (attempt + 1))
        cache.parent.mkdir(parents=True, exist_ok=True)
        cache.write_text(json.dumps(raw), encoding="utf-8")
    ways = [_geom_to_xy(el["geometry"]) for el in raw.get("elements", []) if el.get("geometry")]
    if not ways:
        return []
    lines = _stitch_directed(ways)
    closed = [l for l in lines if len(l) >= 4 and _key(l[0]) == _key(l[-1])]
    open_lines = [l for l in lines if not (len(l) >= 4 and _key(l[0]) == _key(l[-1]))]

    (minx, miny), (maxx, maxy) = rect
    rings = []
    for ring in closed:   # øyer/holmer: beholdes hvis de ligger (delvis) i sonen
        if any(minx <= x <= maxx and miny <= y <= maxy for x, y in ring):
            rings.append(ring[:-1])
    pieces = [pc for l in open_lines for pc in clip_polyline(l, rect)]
    # Etter klipping skal alle åpne biter starte og slutte på rektangelkanten. Gjør de ikke det,
    # mangler det kystlinje i OSM-svaret, og en lukking ville gjettet — stopp heller.
    eps = 1.0
    def on_edge(p):
        return min(abs(p[0] - minx), abs(p[0] - maxx), abs(p[1] - miny), abs(p[1] - maxy)) < eps
    dangling = [pc for pc in pieces if not (on_edge(pc[0]) and on_edge(pc[-1]))]
    if dangling:
        for pc in dangling:
            print(f"  ÅPEN KYSTLINJE {pc[0]} -> {pc[-1]} ({len(pc)} pkt)", file=sys.stderr)
        sys.exit(f"{len(dangling)} kystlinjebit(er) slutter inne i sonen — OSM-svaret er ufullstendig.")
    rings += assemble_land(pieces, rect)
    print(f"Kystlinje: {len(ways)} ways -> {len(closed)} lukkede ringer + {len(pieces)} klippede biter "
          f"-> {len(rings)} landpolygoner", file=sys.stderr)

    # Douglas-Peucker kan gjøre små skjær til sløyfer/streker. Reparer (største del) eller dropp.
    from shapely.geometry import Polygon
    from shapely.validation import make_valid
    out, dropped, repaired = [], 0, 0
    for ring in rings:
        ring = simplify(ring + [ring[0]], SIMPLIFY_TOLERANCE_M)[:-1]
        poly = Polygon(ring) if len(ring) >= 3 else None
        if poly is not None and not poly.is_valid:
            geoms = [g for g in getattr(make_valid(poly), "geoms", [make_valid(poly)]) if g.geom_type == "Polygon"]
            poly = max(geoms, key=lambda g: g.area) if geoms else None
            if poly is not None:
                ring = list(poly.exterior.coords)[:-1]
                repaired += 1
        if poly is None or poly.area < MIN_RING_AREA_M2:
            dropped += 1
            continue
        out.append(ring)
    print(f"Forenkling: {repaired} ringer reparert, {dropped} for små droppet (< {MIN_RING_AREA_M2:.0f} m²)",
          file=sys.stderr)
    return out


# ---------------------------------------------------------------------------
# Validering mot DTM
# ---------------------------------------------------------------------------

DTM_JSON = REPO_ROOT / "Content" / "Fjord" / "Terrain" / "oslofjord_dtm.json"
SEA_MAX_M = 0.5          # DTM-høyde (m) som regnes som sjø (havet kommer som ~0 m)
SAMPLE_STEP_M = 100.0
# Sjø som ligger mer enn DEEP_INSIDE_M innenfor kystlinjen teller; nærmere er det bare avvik mellom
# OSM-kysten og den grove DTM-en (~34 m/px). Målt på korrekt fastland 2026-09-19: 0,23 km² (elve-
# munninger/flate havneområder som DTM-en har på ~0 m). Den gamle, feillukkede ringen: 34 km².
DEEP_INSIDE_M = 150.0
MAX_SEA_KM2 = 0.5


def validate_landmasses(landmasses, islands) -> bool:
    """Sjekker hver landring: ingen selvkryssing, ingen øyposisjon inni, og maks MAX_SEA_KM2 sjø
    (DTM ≤ SEA_MAX_M) inni. Skriver en linje per avvik; returnerer True hvis alt er OK."""
    import numpy as np
    from shapely.geometry import LineString, Point, Polygon

    meta = json.loads(DTM_JSON.read_text(encoding="utf-8"))
    raw = np.fromfile(DTM_JSON.with_suffix(".r16"), dtype="<u2").reshape(meta["height"], meta["width"])
    dtm = meta["min_m"] + raw.astype(np.float32) / 65535.0 * (meta["max_m"] - meta["min_m"])
    bb = meta["bbox_m"]
    xs = np.arange(bb["min_x"] + SAMPLE_STEP_M / 2, bb["max_x"], SAMPLE_STEP_M)
    ys = np.arange(bb["min_y"] + SAMPLE_STEP_M / 2, bb["max_y"], SAMPLE_STEP_M)
    gx, gy = np.meshgrid(xs, ys)
    cols = ((gx - bb["min_x"]) / (bb["max_x"] - bb["min_x"]) * (meta["width"] - 1)).round().astype(int)
    rows = ((bb["max_y"] - gy) / (bb["max_y"] - bb["min_y"]) * (meta["height"] - 1)).round().astype(int)
    sea = dtm[rows, cols] <= SEA_MAX_M
    cell_km2 = SAMPLE_STEP_M * SAMPLE_STEP_M / 1e6

    sea_x, sea_y = gx[sea], gy[sea]
    isl_xy = np.array([i["position"] for i in islands], dtype=np.float64).reshape(-1, 2)

    def even_odd(ring, px, py):
        """Partall/odde-test (som ear-clipperen i FjordGeometry i praksis fyller for en
        selvkryssende ring — shapely sin buffer(0) ville undervurdert den)."""
        inside = np.zeros(px.shape, dtype=bool)
        n = len(ring)
        for a in range(n):
            x1, y1 = ring[a]
            x2, y2 = ring[(a + 1) % n]
            if y1 == y2:
                continue
            crosses = (y1 > py) != (y2 > py)
            xin = x1 + (py - y1) / (y2 - y1) * (x2 - x1)
            inside ^= crosses & (px < xin)
        return inside

    ok = True
    for k, ring in enumerate(landmasses):
        if not Polygon(ring).is_valid:
            print(f"  RING {k}: ugyldig polygon (selvkryssende), {len(ring)} pkt", file=sys.stderr)
            ok = False
        rx, ry = zip(*ring)
        box = (sea_x >= min(rx)) & (sea_x <= max(rx)) & (sea_y >= min(ry)) & (sea_y <= max(ry))
        hit = even_odd(ring, sea_x[box], sea_y[box])
        n_sea = int(hit.sum())
        if n_sea:
            edge = LineString(list(ring) + [ring[0]])
            n_deep = sum(1 for x, y in zip(sea_x[box][hit], sea_y[box][hit])
                         if edge.distance(Point(x, y)) > DEEP_INSIDE_M)
            if n_deep * cell_km2 > MAX_SEA_KM2:
                print(f"  RING {k}: dekker {n_deep * cell_km2:.2f} km² sjø mer enn {DEEP_INSIDE_M:.0f} m inne "
                      f"({n_sea * cell_km2:.2f} km² totalt), {len(ring)} pkt", file=sys.stderr)
                ok = False
        in_ring = even_odd(ring, isl_xy[:, 0], isl_xy[:, 1]) if len(isl_xy) else []
        # En øys egen kystring inneholder selvsagt øya; en ANNEN ring med en øy inni er fastland over sjø.
        cx, cy = _centroid(ring)
        inside = [i["name"] for i, hit in zip(islands, in_ring)
                  if hit and math.hypot(i["position"][0] - cx, i["position"][1] - cy) > 50.0]
        if inside:
            print(f"  RING {k}: inneholder øy(er) {', '.join(inside)}", file=sys.stderr)
            ok = False
    print(f"Validering: {len(landmasses)} ringer mot {int(sea.sum())} DTM-sjøprøver ({SAMPLE_STEP_M:.0f} m) — "
          f"{'OK' if ok else 'FEIL'}", file=sys.stderr)
    return ok


def _islands_from_json(path):
    data = json.loads(path.read_text(encoding="utf-8"))
    return [{"name": i["Name"], "position": (i["Position"]["X"], i["Position"]["Y"]),
             "scale": i["Scale"], "outline": [(p["X"], p["Y"]) for p in i["Outline"]]}
            for i in data["Islands"]]


def main() -> None:
    # Inner Oslofjord: Drøbak to Oslo (south, west, north, east)
    south, west, north, east = 59.65, 10.5, 59.95, 10.85
    out_path = SCRIPTS / "fjord_data.json"
    landmasses_only = "--landmasses-only" in sys.argv

    islands = _islands_from_json(out_path) if landmasses_only else fetch_islands(south, west, north, east)
    landmasses = fetch_landmasses()

    n_isl_pts = sum(len(i["outline"]) for i in islands)
    n_land_pts = sum(len(r) for r in landmasses)
    print(f"Islands: {len(islands)} ({n_isl_pts} outline pts). "
          f"Landmasses: {len(landmasses)} ({n_land_pts} pts). "
          f"Simplify tol={SIMPLIFY_TOLERANCE_M} m.", file=sys.stderr)

    if not validate_landmasses(landmasses, islands):
        sys.exit("Landmassene besto ikke valideringen — fjord_data.json er IKKE skrevet.")

    if not landmasses_only:
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
        land_json = [[{"X": x, "Y": y} for x, y in r] for r in landmasses]
        if landmasses_only:
            out = json.loads(out_path.read_text(encoding="utf-8"))
            out["Landmasses"] = land_json
        else:
            out = {
                "Landmasses": land_json,
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
        with open(out_path, "w", encoding="utf-8") as f:
            json.dump(out, f, indent=2, ensure_ascii=False)
        print("Wrote", out_path, file=sys.stderr)


if __name__ == "__main__":
    main()
