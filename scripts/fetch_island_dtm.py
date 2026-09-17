"""Hent høyoppløst Kartverket-DTM (NHM, 1 m kildedata) per øy til scripts/cache/dtm/.

Spillets projeksjon er ekvirektangulær (lon/lat → meter fra Oslo-origo), så et regulært
lon/lat-rutenett ER et regulært rutenett i spillmeter. Vi ber derfor WCS-en om EPSG:4326 med
en pikselstørrelse som tilsvarer --res meter, flislagt i biter på maks TILE_PX piksler.
Ingen reprojeksjon trengs, og resultatet ligger eksakt i samme koordinatsystem som
scripts/fjord_data.json.

Kjør:
  uv run --directory scripts/mcp-kartverket -- python ../fetch_island_dtm.py            # alle øyer, 1 m
  uv run --directory scripts/mcp-kartverket -- python ../fetch_island_dtm.py --only Hovedøya --res 1

Utdata per øy: <slug>.npy (float32, meter over havet, rad 0 = NORD, NaN = hav/nodata)
og <slug>.json (bbox i spillmeter, oppløsning, form).
"""
from __future__ import annotations

import argparse
import io
import json
import math
import re
import sys
import time
import unicodedata
from pathlib import Path

import numpy as np
import tifffile

SCRIPTS = Path(__file__).resolve().parent
sys.path.insert(0, str(SCRIPTS))
sys.path.insert(0, str(SCRIPTS / "mcp-kartverket"))

from fjord_data_from_geojson import ORIGIN_LAT, ORIGIN_LON  # noqa: E402
import wcs  # noqa: E402

FJORD_JSON = SCRIPTS / "fjord_data.json"
OUT_DIR = SCRIPTS / "cache" / "dtm"
TILE_PX = 2000
MARGIN_M = 200.0          # dekker undervannsbeltet baken legger rundt øya
M_PER_DEG_LAT = 111320.0
M_PER_DEG_LON = 111320.0 * math.cos(math.radians(ORIGIN_LAT))


def slugify(name: str) -> str:
    """«Håøya» → «haaoya»: stabile ASCII-filnavn/asset-navn."""
    s = name.lower().replace("å", "aa").replace("ø", "o").replace("æ", "ae")
    s = unicodedata.normalize("NFKD", s).encode("ascii", "ignore").decode()
    return re.sub(r"[^a-z0-9]+", "_", s).strip("_")


def m_to_lonlat(x_m: float, y_m: float) -> tuple[float, float]:
    return ORIGIN_LON + x_m / M_PER_DEG_LON, ORIGIN_LAT + y_m / M_PER_DEG_LAT


def fetch_grid(x0: float, y0: float, x1: float, y1: float, res: float) -> np.ndarray:
    """Hent [y1..y0] x [x0..x1] (spillmeter) som float32-rutenett, rad 0 = nord."""
    w = int(round((x1 - x0) / res))
    h = int(round((y1 - y0) / res))
    out = np.full((h, w), np.nan, dtype=np.float32)
    for ty in range(0, h, TILE_PX):
        for tx in range(0, w, TILE_PX):
            tw, th = min(TILE_PX, w - tx), min(TILE_PX, h - ty)
            # Flisens utstrekning i spillmeter (rad 0 = nord → y teller nedover fra y1).
            fx0, fx1 = x0 + tx * res, x0 + (tx + tw) * res
            fy1, fy0 = y1 - ty * res, y1 - (ty + th) * res
            lon0, lat0 = m_to_lonlat(fx0, fy0)
            lon1, lat1 = m_to_lonlat(fx1, fy1)
            for attempt in range(3):
                try:
                    tif = wcs.get_coverage((lon0, lat0, lon1, lat1), tw, th)
                    break
                except Exception as ex:  # noqa: BLE001 - nettverksfeil: prøv igjen
                    if attempt == 2:
                        raise
                    print(f"    flis ({tx},{ty}) feilet ({ex}); prøver igjen", file=sys.stderr)
                    time.sleep(3)
            arr = tifffile.imread(io.BytesIO(tif)).astype(np.float32)
            if arr.shape != (th, tw):
                raise RuntimeError(f"uventet flisform {arr.shape}, forventet {(th, tw)}")
            out[ty:ty + th, tx:tx + tw] = arr
    # Nodata/hav kommer som store negative eller absurde verdier.
    out[(out < -50.0) | (out > 2500.0)] = np.nan
    return out


def main() -> None:
    ap = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    ap.add_argument("--res", type=float, default=1.0, help="meter per piksel (standard 1)")
    ap.add_argument("--only", action="append", help="øynavn (kan gjentas); standard alle")
    ap.add_argument("--force", action="store_true", help="hent selv om cache finnes")
    args = ap.parse_args()

    data = json.loads(FJORD_JSON.read_text(encoding="utf-8"))
    OUT_DIR.mkdir(parents=True, exist_ok=True)

    for isl in data["Islands"]:
        name = isl["Name"]
        if args.only and name not in args.only:
            continue
        slug = slugify(name)
        npy, meta = OUT_DIR / f"{slug}.npy", OUT_DIR / f"{slug}.json"
        if npy.exists() and meta.exists() and not args.force:
            if json.loads(meta.read_text())["res_m"] == args.res:
                print(f"{name}: cache finnes, hopper over")
                continue

        xs = [p["X"] for p in isl["Outline"]]
        ys = [p["Y"] for p in isl["Outline"]]
        # Snap til hele --res slik at naboøyers rutenett ligger på samme gitter.
        x0 = math.floor((min(xs) - MARGIN_M) / args.res) * args.res
        y0 = math.floor((min(ys) - MARGIN_M) / args.res) * args.res
        x1 = math.ceil((max(xs) + MARGIN_M) / args.res) * args.res
        y1 = math.ceil((max(ys) + MARGIN_M) / args.res) * args.res

        print(f"{name}: {x1 - x0:.0f} x {y1 - y0:.0f} m @ {args.res} m ...", flush=True)
        grid = fetch_grid(x0, y0, x1, y1, args.res)
        np.save(npy, grid)
        meta.write_text(json.dumps({
            "name": name, "slug": slug, "res_m": args.res,
            "min_x_m": x0, "min_y_m": y0, "max_x_m": x1, "max_y_m": y1,
            "width": int(grid.shape[1]), "height": int(grid.shape[0]),
            "row0_is_north": True,
            "min_m": float(np.nanmin(grid)) if np.isfinite(grid).any() else None,
            "max_m": float(np.nanmax(grid)) if np.isfinite(grid).any() else None,
            "land_fraction": float(np.isfinite(grid).mean()),
        }, indent=2, ensure_ascii=False), encoding="utf-8")
        print(f"  → {npy.name}  {grid.shape[1]}x{grid.shape[0]}  "
              f"høyde {np.nanmin(grid):.1f}..{np.nanmax(grid):.1f} m  "
              f"land {np.isfinite(grid).mean() * 100:.0f} %")


if __name__ == "__main__":
    main()
