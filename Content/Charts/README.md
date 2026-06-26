# Kartverket-kart for oversiktskart

Oversiktskartet i HUD kan vise Kartverket sjøkart som bakgrunn i fjordmodus.

## Oppsett

### 1. Hent kart

**Via script (samme WMS som Kartverket MCP):**
```bash
PYTHONPATH=scripts/mcp-kartverket scripts/mcp-kartverket/.venv/bin/python scripts/fetch_oslofjord_chart.py -o Content/Charts/OslofjordChart.png
```

**Via Kartverket MCP** (i Cursor): Kall `get_chart_image` med `bbox=[10.4, 59.7, 11.2, 60.0]`, `layers="kart549"`, `width=1024`, `height=1024`. Dekod `image_base64` fra responsen og skriv til `Content/Charts/OslofjordChart.png`.

Du kan endre `--bbox`, `--layer` (f.eks. `kart549`, `overseiling`) og `--width`/`--height`.

### 2. Importer i Unreal

- **Manuelt:** Dra `OslofjordChart.png` inn i Content Browser (Content/Charts).
- **Via Unreal MCP:** Åpne Sailing-prosjektet i Unreal Editor, deretter kjør i Cursor (execute_python):
  ```python
  import unreal
  from pathlib import Path
  png_path = str(Path("/path/to/sailing") / "Content" / "Charts" / "OslofjordChart.png")
  task = unreal.AssetImportTask()
  task.filename = png_path
  task.destination_path = "/Game/Charts"
  task.destination_name = "OslofjordChart"
  task.automated = True
  task.save = True
  task.replace_existing = True
  unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])
  ```

### 3. Bruk kartet i Blender (valgfritt)

Via **Blender MCP** kan du laste inn kartbildet som referanse:
```python
# execute_blender_code:
bpy.data.images.load("/full/path/to/sailing/Content/Charts/OslofjordChart.png")
```
Bildet ligger da i Blender under Images og kan brukes som tekstur eller referanse.

### 4. I spillet

- Sett **GameMode** til å bruke fjordkart (`bUseFjordMap = true`).
- På **Sailing HUD** (eller nivå-Blueprint): tilordne **Chart Texture** til texture-assetet `OslofjordChart`.
- **ChartLonLatBox** skal matche bbox brukt ved henting (standard: X=10.4, Y=59.7, Z=11.2, W=60.0).

Oversiktskartet viser da Kartverket-kart som bakgrunn med øyer og spiller over.
