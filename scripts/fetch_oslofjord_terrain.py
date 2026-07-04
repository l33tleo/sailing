#!/usr/bin/env python3
"""
Fetch aerial imagery (ESRI World Imagery) and elevation (Kartverket DTM) for the
inner Oslofjord, matched to the game's georeferencing, so the fjord islands and
mainland can be textured with a real photo and given real relief.

Outputs:
  - scripts/terrain/ortho.png            : aerial photo covering the fjord bbox
  - scripts/terrain/ortho.json           : the exact bbox (WGS84 + meters + Unreal units)
  - Content/Fjord/Terrain/oslofjord_dtm.r16   : uint16 heightfield (row-major, LE)
  - Content/Fjord/Terrain/oslofjord_dtm.json  : grid dims, bbox, min/max meters

The bbox is derived from scripts/fjord_data.json (all island outlines + landmasses)
so imagery/elevation line up with the geometry already in the game.

Usage:
  uv run --directory scripts/mcp-kartverket -- python ../fetch_oslofjord_terrain.py
"""
from __future__ import annotations

import io
import json
import sys
from pathlib import Path

import numpy as np
import requests
import tifffile
from PIL import Image

REPO_ROOT = Path(__file__).resolve().parent.parent
SCRIPTS = REPO_ROOT / "scripts"
sys.path.insert(0, str(SCRIPTS))
sys.path.insert(0, str(SCRIPTS / "mcp-kartverket"))

from fjord_data_from_geojson import ORIGIN_LAT, ORIGIN_LON  # noqa: E402
import wcs  # noqa: E402

# meters × DistanceScale = Unreal units (must match FjordMapManager::DistanceScale)
DISTANCE_SCALE = 100.0
# WCS/imagery bbox margin around the geometry extent, in meters
BBOX_MARGIN_M = 800.0
# ESRI export: cap the long side; keep the fjord aspect ratio
ORTHO_LONG_SIDE = 4096
# DTM grid: cap the long side; keep aspect (square-ish cells)
DTM_LONG_SIDE = 1024

ESRI_EXPORT = (
    "https://server.arcgisonline.com/ArcGIS/rest/services/"
    "World_Imagery/MapServer/export"
)

M_PER_DEG_LAT = 111320.0


def m_per_deg_lon(lat: float) -> float:
    return 111320.0 * np.cos(np.radians(lat))


def unreal_m_to_lonlat(x_m: float, y_m: float) -> tuple[float, float]:
    """Inverse of lat_lon_to_unreal (meters, origin Oslo) -> (lon, lat)."""
    lon = ORIGIN_LON + x_m / m_per_deg_lon(ORIGIN_LAT)
    lat = ORIGIN_LAT + y_m / M_PER_DEG_LAT
    return (lon, lat)


def geometry_extent_m() -> tuple[float, float, float, float]:
    """Read fjord_data.json, return (min_x, min_y, max_x, max_y) in meters."""
    data = json.loads((SCRIPTS / "fjord_data.json").read_text(encoding="utf-8"))
    xs: list[float] = []
    ys: list[float] = []
    for isl in data.get("Islands", []):
        for p in isl.get("Outline", []):
            xs.append(p["X"]); ys.append(p["Y"])
    for ring in data.get("Landmasses", []):
        for p in ring:
            xs.append(p["X"]); ys.append(p["Y"])
    if not xs:
        raise RuntimeError("No geometry points in fjord_data.json")
    return (min(xs), min(ys), max(xs), max(ys))


def fetch_ortho(bbox_lonlat, width, height) -> bytes:
    min_lon, min_lat, max_lon, max_lat = bbox_lonlat
    params = {
        "bbox": f"{min_lon},{min_lat},{max_lon},{max_lat}",
        "bboxSR": "4326",
        "imageSR": "4326",
        "size": f"{width},{height}",
        "format": "jpg",
        "f": "image",
    }
    resp = requests.get(ESRI_EXPORT, params=params, timeout=120,
                        headers={"User-Agent": "sailing-game/1.0 (ortho client)"})
    resp.raise_for_status()
    if "image" not in resp.headers.get("Content-Type", ""):
        raise RuntimeError(f"ESRI did not return an image:\n{resp.text[:500]}")
    return resp.content


def main() -> None:
    min_x, min_y, max_x, max_y = geometry_extent_m()
    min_x -= BBOX_MARGIN_M; min_y -= BBOX_MARGIN_M
    max_x += BBOX_MARGIN_M; max_y += BBOX_MARGIN_M

    span_x = max_x - min_x
    span_y = max_y - min_y
    min_lon, min_lat = unreal_m_to_lonlat(min_x, min_y)
    max_lon, max_lat = unreal_m_to_lonlat(max_x, max_y)
    bbox_lonlat = (min_lon, min_lat, max_lon, max_lat)

    print(f"Fjord extent (m): X[{min_x:.0f},{max_x:.0f}] Y[{min_y:.0f},{max_y:.0f}] "
          f"span {span_x:.0f}×{span_y:.0f} m", file=sys.stderr)
    print(f"WGS84 bbox: {bbox_lonlat}", file=sys.stderr)

    terrain_dir = SCRIPTS / "terrain"
    terrain_dir.mkdir(parents=True, exist_ok=True)
    content_dir = REPO_ROOT / "Content" / "Fjord" / "Terrain"
    content_dir.mkdir(parents=True, exist_ok=True)

    # ---- Orthophoto (aerial) -------------------------------------------
    if span_x >= span_y:
        ow = ORTHO_LONG_SIDE
        oh = max(1, round(ORTHO_LONG_SIDE * span_y / span_x))
    else:
        oh = ORTHO_LONG_SIDE
        ow = max(1, round(ORTHO_LONG_SIDE * span_x / span_y))
    print(f"Fetching ESRI ortho {ow}×{oh} …", file=sys.stderr)
    jpg = fetch_ortho(bbox_lonlat, ow, oh)
    img = Image.open(io.BytesIO(jpg)).convert("RGB")
    ortho_png = terrain_dir / "ortho.png"
    img.save(ortho_png)
    # bbox in Unreal world units (meters × DistanceScale); world X=East, Y=North
    ortho_meta = {
        "bbox_lonlat": list(bbox_lonlat),
        "bbox_m": {"min_x": min_x, "min_y": min_y, "max_x": max_x, "max_y": max_y},
        "bbox_unreal": {
            "min_x": min_x * DISTANCE_SCALE, "min_y": min_y * DISTANCE_SCALE,
            "max_x": max_x * DISTANCE_SCALE, "max_y": max_y * DISTANCE_SCALE,
        },
        "size": [ow, oh],
        "distance_scale": DISTANCE_SCALE,
    }
    (terrain_dir / "ortho.json").write_text(json.dumps(ortho_meta, indent=2), encoding="utf-8")
    print(f"Wrote {ortho_png} ({img.size[0]}×{img.size[1]})", file=sys.stderr)

    # ---- Elevation (DTM) -----------------------------------------------
    if span_x >= span_y:
        dw = DTM_LONG_SIDE
        dh = max(1, round(DTM_LONG_SIDE * span_y / span_x))
    else:
        dh = DTM_LONG_SIDE
        dw = max(1, round(DTM_LONG_SIDE * span_x / span_y))
    print(f"Fetching Kartverket DTM {dw}×{dh} …", file=sys.stderr)
    tif = wcs.get_coverage(bbox_lonlat, dw, dh)
    arr = tifffile.imread(io.BytesIO(tif)).astype(np.float32)
    if arr.ndim == 3:
        arr = arr[..., 0]
    dh_act, dw_act = arr.shape

    # Sea / nodata / spurious values -> 0 m (sea level). Keep plausible land only.
    valid = np.isfinite(arr) & (arr > -10.0) & (arr < 3000.0)
    n_drop = int((~valid).sum())
    arr = np.where(valid, arr, 0.0)

    min_m = float(arr[valid].min()) if valid.any() else 0.0
    max_m = float(arr.max())
    if max_m <= min_m:
        max_m = min_m + 1.0
    print(f"DTM {dw_act}×{dh_act}: elevation {min_m:.1f}..{max_m:.1f} m, "
          f"{n_drop} px clamped to 0 (sea/nodata)", file=sys.stderr)

    # Normalize to uint16, row-major. Row 0 = north (max_lat), matching image top.
    norm = np.clip((arr - min_m) / (max_m - min_m), 0.0, 1.0)
    u16 = (norm * 65535.0 + 0.5).astype("<u2")
    r16_path = content_dir / "oslofjord_dtm.r16"
    r16_path.write_bytes(u16.tobytes(order="C"))

    dtm_meta = {
        "width": dw_act,
        "height": dh_act,
        "min_m": min_m,
        "max_m": max_m,
        "bbox_m": {"min_x": min_x, "min_y": min_y, "max_x": max_x, "max_y": max_y},
        "bbox_unreal": {
            "min_x": min_x * DISTANCE_SCALE, "min_y": min_y * DISTANCE_SCALE,
            "max_x": max_x * DISTANCE_SCALE, "max_y": max_y * DISTANCE_SCALE,
        },
        "distance_scale": DISTANCE_SCALE,
        "row0_is_north": True,
    }
    (content_dir / "oslofjord_dtm.json").write_text(json.dumps(dtm_meta, indent=2), encoding="utf-8")
    print(f"Wrote {r16_path} ({dw_act}×{dh_act}) + json", file=sys.stderr)


if __name__ == "__main__":
    main()
