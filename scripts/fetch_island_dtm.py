"""Hent høyoppløst Kartverket-DTM (NHM, 1 m kildedata) per øy til scripts/cache/dtm/.

Data hentes i tjenestens eget koordinatsystem (EPSG:25833, 1 m piksler, flislagt) og resamples
bilineært til spillets gitter (ekvirektangulært fra lon/lat, samme system som
scripts/fjord_data.json). NB: å be WCS-en om EPSG:4326 direkte ser ut til å virke, men gir et
grovere oversiktsnivå med synlige ~16 m flissømmer i relieffet.

Kjør:
  uv run --directory scripts/mcp-kartverket --with pyproj --with scipy -- python ../fetch_island_dtm.py
  … --only Hovedøya --res 1   (én øy)

Utdata per øy: <slug>.npy (float32, meter over havet, rad 0 = NORD, hav ≈ 0)
og <slug>.json (bbox i spillmeter, oppløsning, form).
"""
from __future__ import annotations

import argparse
import io
import json
import math
import sys
import time
from pathlib import Path

import numpy as np
import tifffile
from pyproj import Transformer
from scipy import ndimage

SCRIPTS = Path(__file__).resolve().parent
sys.path.insert(0, str(SCRIPTS))
sys.path.insert(0, str(SCRIPTS / "mcp-kartverket"))

from fjord_data_from_geojson import ORIGIN_LAT, ORIGIN_LON  # noqa: E402
import wcs  # noqa: E402
from fjord_slug import slugify  # noqa: E402

FJORD_JSON = SCRIPTS / "fjord_data.json"
OUT_DIR = SCRIPTS / "cache" / "dtm"
TILE_PX = 2000
MARGIN_M = 200.0          # dekker undervannsbeltet baken legger rundt øya
M_PER_DEG_LAT = 111320.0
M_PER_DEG_LON = 111320.0 * math.cos(math.radians(ORIGIN_LAT))
TO_UTM33 = Transformer.from_crs(4326, 25833, always_xy=True)
SOURCE = "wcs-25833-1m"   # endres når henteren endres, slik at gammel cache hentes på nytt


def m_to_lonlat(x_m: float, y_m: float) -> tuple[float, float]:
    return ORIGIN_LON + x_m / M_PER_DEG_LON, ORIGIN_LAT + y_m / M_PER_DEG_LAT


def fetch_native(e0: int, n0: int, e1: int, n1: int) -> np.ndarray:
    """Hent [n0..n1] x [e0..e1] i EPSG:25833 med 1 m piksler (tjenestens egen oppløsning), rad 0 = nord."""
    w, h = e1 - e0, n1 - n0
    out = np.zeros((h, w), dtype=np.float32)
    for ty in range(0, h, TILE_PX):
        for tx in range(0, w, TILE_PX):
            tw, th = min(TILE_PX, w - tx), min(TILE_PX, h - ty)
            bbox = (e0 + tx, n1 - ty - th, e0 + tx + tw, n1 - ty)
            for attempt in range(3):
                try:
                    tif = wcs.get_coverage_native(bbox, 1.0)
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
    # Nodata/hav kan komme som store negative eller absurde verdier → havnivå.
    out[(out < -50.0) | (out > 2500.0)] = 0.0
    return out


def fetch_grid(x0: float, y0: float, x1: float, y1: float, res: float) -> np.ndarray:
    """Høyder på spillets gitter [y1..y0] x [x0..x1] (spillmeter), rad 0 = nord.

    Henter 1 m-data i EPSG:25833 og resampler bilineært til spillgitteret. (Å be WCS-en om
    EPSG:4326 direkte gir et grovere oversiktsnivå med synlige ~16 m flissømmer.)
    """
    w = int(round((x1 - x0) / res))
    h = int(round((y1 - y0) / res))
    gx = x0 + (np.arange(w) + 0.5) * res
    gy = y1 - (np.arange(h) + 0.5) * res
    lon = ORIGIN_LON + gx / M_PER_DEG_LON
    lat = ORIGIN_LAT + gy / M_PER_DEG_LAT
    lon2, lat2 = np.meshgrid(lon, lat)
    east, north = TO_UTM33.transform(lon2, lat2)

    pad = 3
    e0, e1 = int(np.floor(east.min())) - pad, int(np.ceil(east.max())) + pad
    n0, n1 = int(np.floor(north.min())) - pad, int(np.ceil(north.max())) + pad
    native = fetch_native(e0, n0, e1, n1)

    # Pikselsentre i native-rasteret: kolonne c ↔ e0 + c + 0.5, rad r ↔ n1 - r - 0.5.
    cols = east - e0 - 0.5
    rows = n1 - north - 0.5
    return ndimage.map_coordinates(native, [rows, cols], order=1, mode="nearest").astype(np.float32)


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
            old = json.loads(meta.read_text())
            if old["res_m"] == args.res and old.get("source") == SOURCE:
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
            "name": name, "slug": slug, "res_m": args.res, "source": SOURCE,
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
