#!/usr/bin/env python3
"""
Import scripts/terrain/ortho.png as UTexture2D /Game/Fjord/T_OslofjordOrtho.
Run from Unreal Editor Python Console, or via MCP execute_python.
"""
import os
import unreal


def main() -> bool:
    project_dir = unreal.SystemLibrary.get_project_directory()
    png = os.path.join(project_dir, "scripts", "terrain", "ortho.png")
    if not os.path.isfile(png):
        unreal.log_error(f"Ortho not found: {png} (run fetch_oslofjord_terrain.py first)")
        return False

    task = unreal.AssetImportTask()
    task.filename = png
    task.destination_path = "/Game/Fjord"
    task.destination_name = "T_OslofjordOrtho"
    task.automated = True
    task.save = True
    task.replace_existing = True

    unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])

    tex = unreal.load_asset("/Game/Fjord/T_OslofjordOrtho")
    if not tex:
        unreal.log_error("Import failed: T_OslofjordOrtho not created")
        return False
    # Aerial photo is albedo -> sRGB; no tiling.
    tex.set_editor_property("srgb", True)
    tex.set_editor_property("address_x", unreal.TextureAddress.TA_CLAMP)
    tex.set_editor_property("address_y", unreal.TextureAddress.TA_CLAMP)
    unreal.EditorAssetLibrary.save_asset("/Game/Fjord/T_OslofjordOrtho")
    unreal.log("Imported /Game/Fjord/T_OslofjordOrtho")
    return True


if __name__ == "__main__":
    main()
