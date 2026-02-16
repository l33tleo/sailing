"""
Kartverket nautical chart WMS (wms.sjokartraster2) client.
WMS 1.3.0, EPSG:4326. Bbox order: min_lon, min_lat, max_lon, max_lat.
"""
import xml.etree.ElementTree as ET
import requests

BASE_URL = "https://wms.geonorge.no/skwms1/wms.sjokartraster2"
VERSION = "1.3.0"
CRS = "EPSG:4326"
TIMEOUT = 30

# In-memory cache for GetCapabilities XML (avoid repeated calls)
_capabilities_xml: str | None = None


def get_capabilities_xml(use_cache: bool = True) -> str:
    global _capabilities_xml
    if use_cache and _capabilities_xml is not None:
        return _capabilities_xml
    params = {"service": "WMS", "version": VERSION, "request": "GetCapabilities"}
    resp = requests.get(BASE_URL, params=params, timeout=TIMEOUT)
    resp.raise_for_status()
    _capabilities_xml = resp.text
    return _capabilities_xml


def _local_tag(elem: ET.Element | None) -> str:
    if elem is None:
        return ""
    return elem.tag.split("}")[-1] if "}" in elem.tag else elem.tag


def _find_direct(elem: ET.Element, local_name: str) -> ET.Element | None:
    """Find direct child with given local tag name (ignore namespace)."""
    for child in elem:
        if _local_tag(child) == local_name:
            return child
    return None


def _find_in_tree(elem: ET.Element, local_name: str) -> ET.Element | None:
    """Find first descendant with given local tag name."""
    for child in elem.iter():
        if _local_tag(child) == local_name:
            return child
    return None


def parse_layers_from_capabilities(xml_text: str) -> list[dict]:
    """Parse GetCapabilities XML and return list of layers with name and title."""
    root = ET.fromstring(xml_text)
    layers: list[dict] = []

    def collect_layers(elem: ET.Element) -> None:
        for child in elem:
            if _local_tag(child) == "Layer":
                name_el = _find_direct(child, "Name")
                title_el = _find_direct(child, "Title")
                name = (name_el.text or "").strip()
                title = (title_el.text or "").strip()
                if name:
                    layers.append({"name": name, "title": title})
                collect_layers(child)

    cap = _find_in_tree(root, "Capability")
    if cap is not None:
        layer_root = _find_direct(cap, "Layer")
        if layer_root is not None:
            collect_layers(layer_root)
    return layers


def get_map(bbox: tuple[float, float, float, float], layers: str, width: int, height: int) -> bytes:
    """GetMap: bbox (min_lon, min_lat, max_lon, max_lat), returns PNG bytes."""
    min_lon, min_lat, max_lon, max_lat = bbox
    bbox_str = f"{min_lon},{min_lat},{max_lon},{max_lat}"
    params = {
        "service": "WMS",
        "version": VERSION,
        "request": "GetMap",
        "layers": layers,
        "crs": CRS,
        "bbox": bbox_str,
        "width": width,
        "height": height,
        "format": "image/png",
        "transparent": "true",
    }
    resp = requests.get(BASE_URL, params=params, timeout=TIMEOUT)
    resp.raise_for_status()
    return resp.content


def lon_lat_to_pixel(
    lon: float, lat: float,
    bbox: tuple[float, float, float, float],
    width: int, height: int,
) -> tuple[int, int]:
    """Convert lon/lat to pixel (i, j) in a viewport with given bbox and size. Origin top-left."""
    min_lon, min_lat, max_lon, max_lat = bbox
    if max_lon <= min_lon or max_lat <= min_lat:
        return (0, 0)
    x_ratio = (lon - min_lon) / (max_lon - min_lon)
    y_ratio = (lat - min_lat) / (max_lat - min_lat)
    # Y: image row 0 is top, lat increases north so top = max_lat
    j = int((1.0 - y_ratio) * height)
    i = int(x_ratio * width)
    i = max(0, min(i, width - 1))
    j = max(0, min(j, height - 1))
    return (i, j)


def get_feature_info(
    bbox: tuple[float, float, float, float],
    layers: str,
    width: int,
    height: int,
    i: int,
    j: int,
    info_format: str = "text/plain",
) -> str:
    """GetFeatureInfo for pixel (i, j). Returns response body as string."""
    min_lon, min_lat, max_lon, max_lat = bbox
    bbox_str = f"{min_lon},{min_lat},{max_lon},{max_lat}"
    params = {
        "service": "WMS",
        "version": VERSION,
        "request": "GetFeatureInfo",
        "layers": layers,
        "query_layers": layers,
        "crs": CRS,
        "bbox": bbox_str,
        "width": width,
        "height": height,
        "i": i,
        "j": j,
        "info_format": info_format,
    }
    resp = requests.get(BASE_URL, params=params, timeout=TIMEOUT)
    resp.raise_for_status()
    return resp.text
