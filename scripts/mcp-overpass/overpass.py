"""
Overpass API client: build queries, HTTP request, parse JSON to simplified features.
"""
from __future__ import annotations

from typing import Any

import requests

DEFAULT_ENDPOINT = "https://overpass-api.de/api/interpreter"
TIMEOUT = 60


def build_bbox_query(
    south: float,
    west: float,
    north: float,
    east: float,
    tags: dict[str, str] | None = None,
) -> str:
    """
    Build Overpass QL for bbox. Optional tags e.g. {"natural": "island"}.
    Returns full query including [out:json]; and out geom;
    """
    bbox = f"({south},{west},{north},{east})"
    if tags:
        tag_filters = "".join(f'["{k}"="{v}"]' for k, v in tags.items())
        body = f'nwr{tag_filters}{bbox};'
    else:
        body = f"nwr{bbox};"
    return f"[out:json];{body}out geom;"


def run_query(query: str, endpoint: str = DEFAULT_ENDPOINT) -> dict[str, Any]:
    """
    Send Overpass QL to interpreter. Ensures [out:json]; prefix.
    Returns raw API response (dict with "elements" key).
    """
    q = query.strip()
    if not q.startswith("[out:json]"):
        q = f"[out:json];{q}"
    if ";" not in q.split("[out:json]")[-1]:
        q = f"{q.rstrip()};"
    if "out " not in q and "out;" not in q:
        q = f"{q} out geom;"
    resp = requests.get(
        endpoint,
        params={"data": q},
        timeout=TIMEOUT,
        headers={"Accept": "application/json"},
    )
    resp.raise_for_status()
    return resp.json()


def _lat_lon_from_element(el: dict[str, Any]) -> tuple[float, float] | None:
    """Extract (lat, lon) from a node, way, or relation."""
    if el.get("lat") is not None and el.get("lon") is not None:
        return (float(el["lat"]), float(el["lon"]))
    if "center" in el:
        c = el["center"]
        return (float(c["lat"]), float(c["lon"]))
    geom = el.get("geometry")
    if geom and isinstance(geom, list) and len(geom) > 0:
        # Use first point of way geometry
        n = geom[0]
        if "lat" in n and "lon" in n:
            return (float(n["lat"]), float(n["lon"]))
        # Or compute centroid
        lats = [float(p["lat"]) for p in geom if "lat" in p]
        lons = [float(p["lon"]) for p in geom if "lon" in p]
        if lats and lons:
            return (sum(lats) / len(lats), sum(lons) / len(lons))
    return None


def parse_elements_to_features(raw: dict[str, Any]) -> list[dict[str, Any]]:
    """
    Convert Overpass JSON elements to simplified list of features.
    Each feature: name, lat, lon, osm_type, osm_id, tags.
    """
    elements = raw.get("elements") or []
    out = []
    for el in elements:
        coords = _lat_lon_from_element(el)
        if coords is None:
            continue
        lat, lon = coords
        tags = el.get("tags") or {}
        name = tags.get("name") or tags.get("name:no") or ""
        out.append({
            "name": name,
            "lat": lat,
            "lon": lon,
            "osm_type": el.get("type", "node"),
            "osm_id": el.get("id"),
            "tags": tags,
        })
    return out


def query_bbox(
    south: float,
    west: float,
    north: float,
    east: float,
    tags: dict[str, str] | None = None,
    endpoint: str = DEFAULT_ENDPOINT,
) -> list[dict[str, Any]]:
    """
    Query Overpass for features in bbox with optional tag filter.
    Returns list of { name, lat, lon, osm_type, osm_id, tags }.
    """
    q = build_bbox_query(south, west, north, east, tags)
    raw = run_query(q, endpoint)
    return parse_elements_to_features(raw)


def get_islands_ql(south: float, west: float, north: float, east: float) -> str:
    """Overpass QL for islands: natural=island and place=island in bbox. out center so relations get a center point."""
    bbox = f"({south},{west},{north},{east})"
    # Union of both tag sets; out center adds center to relations (ways already get geometry)
    return f"[out:json];(nwr[\"natural\"=\"island\"]{bbox};nwr[\"place\"=\"island\"]{bbox};);out geom; out center;"


def get_islands(
    south: float,
    west: float,
    north: float,
    east: float,
    endpoint: str = DEFAULT_ENDPOINT,
) -> list[dict[str, Any]]:
    """
    Get islands in bbox (natural=island and place=island).
    Returns same format as query_bbox.
    """
    q = get_islands_ql(south, west, north, east)
    raw = run_query(q, endpoint)
    return parse_elements_to_features(raw)
