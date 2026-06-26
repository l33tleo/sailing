# Kartverket Sjøkart MCP Server

MCP-server som wrapper Kartverkets sjøkart-WMS (`wms.sjokartraster2`) direkte. Ingen API-nøkkel, WMS 1.3.0.

## Tools

- **get_chart_layers()** – Henter tilgjengelige lag fra GetCapabilities (navn og tittel).
- **get_chart_image(bbox, layers, width, height)** – GetMap som PNG; returnerer base64-bilde i dict.
- **get_feature_info(lon, lat, layers, width, height, buffer_degrees)** – GetFeatureInfo for et punkt (lon/lat).

Bbox er `[min_lon, min_lat, max_lon, max_lat]` (WGS84).

## Kjøring

```bash
uv sync
uv run server.py
```

Cursor: serveren er konfigurert i `.cursor/mcp.json` under navnet `kartverket`.

## Merk

Sjøkart-tjenesten er ikke beregnet på navigasjon. Bruk i visualisering/spill er typisk OK.
