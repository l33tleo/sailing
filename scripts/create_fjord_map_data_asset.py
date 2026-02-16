#!/usr/bin/env python3
"""
Create or update UFjordMapData asset from scripts/fjord_data.json.
Run from Unreal Editor (Window -> Python Console, or Execute Python Script),
or via MCP execute_python with Unreal Editor open.

Expects fjord_data.json next to this script (scripts/fjord_data.json).
Writes asset to /Game/Fjord/OslofjordMapData.
"""
from __future__ import annotations

import json
import os

# When run inside Unreal, unreal is available
try:
    import unreal
except ImportError:
    print("Run this script from Unreal Editor (Python Console or Execute Python Script).")
    raise

def get_json_path():
    # When executed via Editor, __file__ may not be set; use project dir
    if "__file__" in dir():
        return os.path.join(os.path.dirname(os.path.abspath(__file__)), "fjord_data.json")
    try:
        project_dir = unreal.SystemLibrary.get_project_directory()
    except Exception:
        project_dir = os.path.dirname(os.path.dirname(os.path.abspath(os.getcwd())))
    return os.path.join(project_dir, "scripts", "fjord_data.json")

def main():
    json_path = get_json_path()
    if not os.path.isfile(json_path):
        print("Not found:", json_path)
        return False
    with open(json_path, encoding="utf-8") as f:
        data_json = json.load(f)

    # Resolve FjordMapData class (Sailing module)
    fjord_class = unreal.load_class(None, "/Script/Sailing.FjordMapData")
    if not fjord_class:
        print("FjordMapData class not found. Is Sailing module loaded?")
        return False

    asset_tools = unreal.AssetToolsHelpers.get_asset_tools()
    package_path = "/Game/Fjord"
    asset_name = "OslofjordMapData"
    full_path = f"{package_path}/{asset_name}"

    # Load existing or create new
    existing = unreal.load_asset(full_path)
    if existing and existing.get_class() == fjord_class:
        data = existing
        print("Updating existing asset:", full_path)
    else:
        data = asset_tools.create_asset(asset_name, package_path, fjord_class, None)
        if not data:
            print("Failed to create asset at", full_path)
            return False
        print("Created asset:", full_path)

    # CoastlinePoints: array of Vector2D
    coast = []
    for p in data_json.get("CoastlinePoints", []):
        coast.append(unreal.Vector2D(float(p["X"]), float(p["Y"])))
    data.set_editor_property("coastline_points", coast)

    # Islands: TArray<FFjordIslandDef>
    islands_raw = data_json.get("Islands", [])
    # In Unreal Python we build the array; struct type may be FjordIslandDef (no F prefix)
    islands_array = data.get_editor_property("islands")
    islands_array.clear()
    for i in islands_raw:
        # Append new struct element and set fields (API may vary by UE version)
        islands_array.append(unreal.FjordIslandDef())
        entry = islands_array[-1]
        entry.set_editor_property("name", i["Name"])
        entry.set_editor_property("position", unreal.Vector2D(float(i["Position"]["X"]), float(i["Position"]["Y"])))
        entry.set_editor_property("scale", float(i["Scale"]))
    data.set_editor_property("islands", islands_array)

    # WorldOrigin, MetersPerUnit
    wo = data_json.get("WorldOrigin", {})
    data.set_editor_property("world_origin", unreal.Vector2D(float(wo.get("X", 0)), float(wo.get("Y", 0))))
    data.set_editor_property("meters_per_unit", float(data_json.get("MetersPerUnit", 1.0)))

    unreal.EditorAssetLibrary.save_asset(full_path)
    print("Saved.", full_path)
    return True

if __name__ == "__main__":
    main()
