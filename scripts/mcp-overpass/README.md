# OpenStreetMap Overpass MCP Server

MCP-server som snakker med Overpass API (OpenStreetMap). Brukes for å hente øyer, steder og andre objekter i et bounding box – f.eks. for å berike FjordMapData med OSM-navn og koordinater.

## Verktøy

- **overpass_query_bbox(south, west, north, east, tags?)** – Hent features i bbox. Valgfritt filter `tags` (f.eks. `natural=island`).
- **overpass_get_islands(south, west, north, east)** – Hent øyer i bbox (natural=island og place=island).
- **overpass_run_query(query)** – Kjør rå Overpass QL; returnerer normalisert feature-liste.

Alle returnerer liste av `{ name, lat, lon, osm_type, osm_id, tags }`. Koordinater er WGS84 (lat/lon).

## Kjøring

```bash
cd scripts/mcp-overpass
uv sync
uv run server.py
```

Cursor: serveren er konfigurert i `.cursor/mcp.json` under navnet `overpass`.

## API

Bruker `https://overpass-api.de/api/interpreter` med 60 s timeout. Unngå mange raske kall (rate limiting).
