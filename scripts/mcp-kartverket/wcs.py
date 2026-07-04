"""
Kartverket høyde-DTM WCS client (wcs.hoyde-dtm-nhm-25833).

Fetches raw elevation as a GeoTIFF over a WGS84 bbox so it lines up with the
game's georeferencing (origin Oslo, EPSG:4326). WCS 1.0.0 GetCoverage.
"""
from __future__ import annotations

import requests

BASE_URL = "https://wcs.geonorge.no/skwms1/wcs.hoyde-dtm-nhm-25833"
COVERAGE = "nhm_dtm_topo_25833"
VERSION = "1.0.0"
CRS = "EPSG:4326"
TIMEOUT = 120


def get_coverage(
    bbox: tuple[float, float, float, float],
    width: int,
    height: int,
    fmt: str = "GeoTIFF",
) -> bytes:
    """GetCoverage: bbox (min_lon, min_lat, max_lon, max_lat) in EPSG:4326.

    Returns GeoTIFF bytes (float32 elevation in meters, nodata for sea/outside).
    """
    min_lon, min_lat, max_lon, max_lat = bbox
    params = {
        "SERVICE": "WCS",
        "VERSION": VERSION,
        "REQUEST": "GetCoverage",
        "COVERAGE": COVERAGE,
        "CRS": CRS,
        "RESPONSE_CRS": CRS,
        "BBOX": f"{min_lon},{min_lat},{max_lon},{max_lat}",
        "WIDTH": str(width),
        "HEIGHT": str(height),
        "FORMAT": fmt,
    }
    resp = requests.get(BASE_URL, params=params, timeout=TIMEOUT,
                        headers={"User-Agent": "sailing-game/1.0 (wcs client)"})
    resp.raise_for_status()
    ctype = resp.headers.get("Content-Type", "")
    if "xml" in ctype.lower() or resp.content[:5] == b"<?xml":
        raise RuntimeError(f"WCS returned an error/XML, not a coverage:\n{resp.text[:1000]}")
    return resp.content
