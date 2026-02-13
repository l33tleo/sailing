"""
Import LaserBoat.fbx into Unreal (separate meshes: LaserHull, LaserMast, etc.).
Run in Unreal: Window → Python Console, then:
    exec(open("/Users/leovonschwind/sailing/import_laser_boat.py").read())
Or: File → Run Python Script → select this file.
"""
import unreal

task = unreal.AssetImportTask()
task.filename = "/Users/leovonschwind/sailing/LaserBoat.fbx"
task.destination_path = "/Game/Models"
task.destination_name = "LaserBoat"
task.automated = True
task.save = True
task.replace_existing = True
task.replace_existing_settings = True

options = unreal.FbxImportUI()
options.import_mesh = True
options.import_materials = True
options.import_textures = False
options.import_animations = False
options.import_as_skeletal = False
options.override_full_name = True
options.static_mesh_import_data.combine_meshes = False

task.options = options
unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])
print("LaserBoat.fbx import finished. Check Content/Models for LaserBoat_* meshes.")
