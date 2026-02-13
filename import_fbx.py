"""
Import FBX files into Unreal Engine.
Run this inside Unreal's Python environment (via MCP or Python console).

Usage:
    import_fbx("/path/to/model.fbx", "/Game/Models", "ModelName")
"""
import unreal


def import_fbx(fbx_path, destination="/Game/Models", asset_name=None, combine_meshes=True):
    """
    Import an FBX file into Unreal Engine.
    
    Args:
        fbx_path: Absolute path to the .fbx file
        destination: Content Browser destination (e.g. "/Game/Models")
        asset_name: Name for the imported asset (defaults to filename)
        combine_meshes: Whether to combine all meshes into one static mesh
    """
    task = unreal.AssetImportTask()
    task.filename = fbx_path
    task.destination_path = destination
    task.automated = True
    task.save = True
    task.replace_existing = True
    task.replace_existing_settings = True

    if asset_name:
        task.destination_name = asset_name

    options = unreal.FbxImportUI()
    options.import_mesh = True
    options.import_materials = True
    options.import_textures = False
    options.import_animations = False
    options.import_as_skeletal = False
    options.override_full_name = bool(asset_name)
    options.static_mesh_import_data.combine_meshes = combine_meshes

    task.options = options

    unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])

    # Verify
    full_path = f"{destination}/{asset_name}" if asset_name else destination
    if unreal.EditorAssetLibrary.does_asset_exist(full_path):
        unreal.log(f"Successfully imported: {full_path}")
        return True
    else:
        unreal.log_warning(f"Import may have completed but asset not found at {full_path}")
        return False


# Direct execution: import Island.fbx
if __name__ == "__main__":
    import_fbx(
        "/Users/leovonschwind/sailing/Island.fbx",
        "/Game/Models",
        "Island",
        combine_meshes=True
    )
