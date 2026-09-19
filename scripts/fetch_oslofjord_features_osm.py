"""Hent OSM-features MED geometri og tags for indre Oslofjord → scripts/fjord_features.json.

Grunnlag for vegetasjon (skogflater), bygg, brygger og sjømerker på øyene. Samme bbox og
projeksjon som scripts/fetch_oslofjord_islands_osm.py (meter fra Oslo-origo).

Kjør:  uv run --directory scripts/mcp-overpass -- python ../fetch_oslofjord_features_osm.py

Utdata: {"<kategori>": [{"id", "tags", "ring": [[x,y],…]} | {"id", "tags", "point": [x,y]}]}
Kategorier: forest (wood/forest/scrub), building, pier, beach, bare_rock, lighthouse, seamark.
Bare features innen KEEP_DIST_M fra en navngitt øy (eller på den) beholdes — fastlandets
bebyggelse er ikke interessant før fastlandet bakes.
"""
from __future__ import annotations

import json
import sys
import time
from pathlib import Path

SCRIPTS = Path(__file__).resolve().parent
sys.path.insert(0, str(SCRIPTS))
sys.path.insert(0, str(SCRIPTS / "mcp-overpass"))

from fjord_data_from_geojson import lat_lon_to_unreal  # noqa: E402
from overpass import run_query  # noqa: E402

BBOX = "(59.65,10.5,59.95,10.85)"   # south, west, north, east — som øy-henteren
OUT = SCRIPTS / "fjord_features.json"
KEEP_DIST_M = 120.0

QUERIES = {
    "forest": 'nwr["natural"~"^(wood|scrub)$"]{b};nwr["landuse"="forest"]{b};',
    "building": 'way["building"]{b};',
    "pier": 'way["man_made"="pier"]{b};',
    "beach": 'way["natural"="beach"]{b};',
    "bare_rock": 'way["natural"="bare_rock"]{b};',
    "lighthouse": 'nwr["man_made"="lighthouse"]{b};',
    "seamark": 'node["seamark:type"]{b};',
}


def to_xy(lat: float, lon: float) -> list[float]:
    x, y = lat_lon_to_unreal(lat, lon)
    return [round(x, 1), round(y, 1)]


def rings_of(el: dict) -> list[list[list[float]]]:
    """Ytre ring(er) for way/relation. Relasjoners inner-ringer (lysninger) ignoreres foreløpig."""
    if el["type"] == "way" and "geometry" in el:
        return [[to_xy(p["lat"], p["lon"]) for p in el["geometry"]]]
    if el["type"] == "relation":
        return [[to_xy(p["lat"], p["lon"]) for p in m["geometry"]]
                for m in el.get("members", []) if m.get("role") == "outer" and "geometry" in m]
    return []


def main() -> None:
    fjord = json.loads((SCRIPTS / "fjord_data.json").read_text(encoding="utf-8"))
    boxes = []
    for isl in fjord["Islands"]:
        xs = [p["X"] for p in isl["Outline"]]
        ys = [p["Y"] for p in isl["Outline"]]
        boxes.append((min(xs) - KEEP_DIST_M, min(ys) - KEEP_DIST_M, max(xs) + KEEP_DIST_M, max(ys) + KEEP_DIST_M))

    def near_island(x: float, y: float) -> bool:
        return any(b[0] <= x <= b[2] and b[1] <= y <= b[3] for b in boxes)

    out: dict[str, list] = {}
    for cat, ql in QUERIES.items():
        for attempt in range(3):
            try:
                raw = run_query(f"[out:json][timeout:180];({ql.format(b=BBOX)});out geom;")
                break
            except Exception as ex:  # noqa: BLE001 - Overpass er ofte opptatt; prøv igjen
                if attempt == 2:
                    raise
                print(f"  {cat}: {ex}; prøver igjen om 20 s", file=sys.stderr)
                time.sleep(20)
        items = []
        for el in raw.get("elements", []):
            tags = el.get("tags", {})
            ident = f'{el["type"]}/{el["id"]}'
            if el["type"] == "node":
                x, y = to_xy(el["lat"], el["lon"])
                # Sjømerker står i sjøen: behold alle i bbox.
                if cat in ("seamark", "lighthouse") or near_island(x, y):
                    items.append({"id": ident, "tags": tags, "point": [x, y]})
                continue
            for ring in rings_of(el):
                if len(ring) >= 3 and near_island(*ring[0]):
                    items.append({"id": ident, "tags": tags, "ring": ring})
        out[cat] = items
        print(f"{cat}: {len(items)}")
        time.sleep(2)

    OUT.write_text(json.dumps(out, ensure_ascii=False, separators=(",", ":")), encoding="utf-8")
    print(f"→ {OUT} ({OUT.stat().st_size / 1e6:.1f} MB)")


if __name__ == "__main__":
    main()
