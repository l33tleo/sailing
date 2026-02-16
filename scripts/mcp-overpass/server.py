"""
MCP server for OpenStreetMap Overpass API.
Exposes query_bbox, get_islands, run_query.
"""
from fastmcp import FastMCP
from overpass import (
    get_islands,
    parse_elements_to_features,
    query_bbox,
    run_query,
)

mcp = FastMCP(name="OpenStreetMap Overpass")


@mcp.tool
def overpass_query_bbox(
    south: float,
    west: float,
    north: float,
    east: float,
    tags: str | None = None,
) -> list[dict]:
    """
    Query OpenStreetMap (Overpass) for features in a bounding box.
    south, west, north, east: decimal degrees (WGS84).
    tags: optional filter, e.g. "natural=island" or "place=island" (single key=value).
    Returns list of { name, lat, lon, osm_type, osm_id, tags }.
    """
    tag_dict = None
    if tags and "=" in tags:
        k, _, v = tags.partition("=")
        tag_dict = {k.strip(): v.strip()}
    return query_bbox(south, west, north, east, tags=tag_dict)


@mcp.tool
def overpass_get_islands(
    south: float,
    west: float,
    north: float,
    east: float,
) -> list[dict]:
    """
    Get islands in a bounding box (natural=island and place=island).
    south, west, north, east: decimal degrees (WGS84).
    Returns list of { name, lat, lon, osm_type, osm_id, tags }.
    """
    return get_islands(south, west, north, east)


@mcp.tool
def overpass_run_query(query: str) -> list[dict]:
    """
    Run raw Overpass QL. [out:json]; and out statement are added if missing.
    Returns normalized list of features { name, lat, lon, osm_type, osm_id, tags }.
    """
    raw = run_query(query)
    return parse_elements_to_features(raw)


def main() -> None:
    mcp.run()


if __name__ == "__main__":
    main()
