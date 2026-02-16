"""
MCP server for Kartverket nautical chart WMS (sjøkart).
Exposes get_chart_layers, get_chart_image, get_feature_info.
"""
import base64
from fastmcp import FastMCP
from wms import (
    get_capabilities_xml,
    parse_layers_from_capabilities,
    get_map,
    lon_lat_to_pixel,
    get_feature_info as wms_get_feature_info,
)

mcp = FastMCP(name="Kartverket Sjøkart")


@mcp.tool
def get_chart_layers() -> list[dict]:
    """
    Hent tilgjengelige lag fra Kartverkets sjøkart-WMS (GetCapabilities).
    Returnerer liste med name og title for hvert lag.
    """
    xml_text = get_capabilities_xml(use_cache=True)
    layers = parse_layers_from_capabilities(xml_text)
    return layers


@mcp.tool
def get_chart_image(
    bbox: list[float],
    layers: str,
    width: int = 800,
    height: int = 600,
) -> dict:
    """
    Hent kartbilde som PNG fra Kartverkets sjøkart-WMS (GetMap).
    bbox: [min_lon, min_lat, max_lon, max_lat] i WGS84.
    layers: kommaseparert lag-navn (f.eks. fra get_chart_layers).
    Returnerer dict med image_base64 (PNG), format, width, height.
    """
    if len(bbox) != 4:
        raise ValueError("bbox must have 4 elements: min_lon, min_lat, max_lon, max_lat")
    png_bytes = get_map(tuple(bbox), layers, width, height)
    b64 = base64.standard_b64encode(png_bytes).decode("ascii")
    return {
        "image_base64": b64,
        "format": "png",
        "width": width,
        "height": height,
    }


@mcp.tool
def get_feature_info(
    lon: float,
    lat: float,
    layers: str,
    width: int = 256,
    height: int = 256,
    buffer_degrees: float = 0.01,
) -> str:
    """
    Hent feature-info for et punkt (lon, lat) fra Kartverkets sjøkart-WMS (GetFeatureInfo).
    Bruker et lite viewport rundt punktet (buffer_degrees) for å beregne pixel og kalle GetFeatureInfo.
    Returnerer respons som ren tekst.
    """
    min_lon = lon - buffer_degrees
    max_lon = lon + buffer_degrees
    min_lat = lat - buffer_degrees
    max_lat = lat + buffer_degrees
    bbox = (min_lon, min_lat, max_lon, max_lat)
    i, j = lon_lat_to_pixel(lon, lat, bbox, width, height)
    return wms_get_feature_info(bbox, layers, width, height, i, j, info_format="text/plain")


def main() -> None:
    mcp.run()


if __name__ == "__main__":
    main()
