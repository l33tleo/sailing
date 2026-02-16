#!/usr/bin/env python3
"""
Import Oslofjord chart PNG as texture in Unreal Engine.
Run from Unreal Editor Python console or via MCP execute_python.
Expects project path (e.g. /Users/.../sailing) or pass path to PNG.

Usage from Unreal:
  import import_chart_to_unreal
  import_chart_to_unreal.import_chart("/path/to/sailing/Content/Charts/OslofjordChart.png")
"""
from __future__ import annotations

import os
from pathlib import Path


def import_chart(png_path: str | None = None, project_dir: str | None = None) -> str:
    """
    Import chart PNG as texture to /Game/Charts.
    If png_path is None, derives from project_dir/Content/Charts/OslofjordChart.png.
    Returns the asset path (/Game/Charts/OslofjordChart) or error message.
    """
    try:
        import unreal
    except ImportError:
        return "Unreal module not available (run inside Unreal Editor Python or MCP execute_python)"

    if png_path is None:
        if project_dir:
            png_path = str(Path(project_dir) / "Content" / "Charts" / "OslofjordChart.png")
        else:
            # Default: assume current project is Sailing
            project = unreal.Paths.project_dir()
            if project:
                png_path = str(Path(project) / "Content" / "Charts" / "OslofjordChart.png")
            else:
                return "Could not resolve project dir; pass png_path or project_dir"

    path = Path(png_path)
    if not path.exists():
        return f"File not found: {png_path}"

    task = unreal.AssetImportTask()
    task.filename = str(path.resolve())
    task.destination_path = "/Game/Charts"
    task.destination_name = path.stem  # OslofjordChart
    task.automated = True
    task.save = True
    task.replace_existing = True

    unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])
    if task.imported_paths:
        return task.imported_paths[0]
    return "Import completed but no path returned (check Output Log)"


if __name__ == "__main__":
    # When run as script, use project dir relative to this file
    repo = Path(__file__).resolve().parent.parent
    result = import_chart(project_dir=str(repo))
    print(result)
