"""
Create a Laser dinghy in Blender and export to FBX.

How to run:
  1. Open Blender, switch to Scripting workspace.
  2. Open this file (Text → Open → create_laser_boat.py).
  3. Run Script (or Alt+P). FBX is written to this script’s folder as LaserBoat.fbx.

Then in Unreal: import LaserBoat.fbx to /Game/Models with "Combine Meshes" OFF
so you get separate meshes (LaserHull, LaserMast, LaserBoom, LaserSail, LaserRudder, LaserTiller).

Laser reference: plumb bow, flat transom, single triangular mainsail,
mast + boom, rudder with tiller. Approximate scale 1 Blender unit = 1 m.
"""
import bpy
import mathutils
import os

# Clear existing mesh objects
bpy.ops.object.select_all(action='SELECT')
bpy.ops.object.delete(use_global=False)

# === Hull (Laser hull: ~4.23 m LOA, beam ~1.42 m) ===
# Simple hull: box with pointed bow (taper front vertices to a point)
bpy.ops.mesh.primitive_cube_add(size=1)
hull = bpy.context.active_object
hull.name = "LaserHull"
hull.scale = (4.23, 1.42, 0.45)  # length, beam, depth
bpy.ops.object.transform_apply(scale=True)

# Taper front to pointed bow: pinch front vertices in Y and Z
bpy.ops.object.mode_set(mode='EDIT')
bpy.ops.mesh.select_all(action='DESELECT')
for v in hull.data.vertices:
    if v.co.x > 1.0:  # front half in local space
        v.select = True
bpy.ops.transform.resize(value=(1, 0.25, 0.3), constraint_axis=(False, True, True), orient_type='GLOBAL')
bpy.ops.mesh.select_all(action='DESELECT')
bpy.ops.object.mode_set(mode='OBJECT')

# Slight deck crown (optional): leave hull as is for simplicity
# Add smooth shading
for poly in hull.data.polygons:
    poly.use_smooth = True

# === Mast (cylinder, ~6.5 m, at ~1.2 m from bow) ===
bpy.ops.mesh.primitive_cylinder_add(radius=0.04, depth=6.5, vertices=16)
mast = bpy.context.active_object
mast.name = "LaserMast"
mast.location = (1.2, 0, 0.25)
mast.rotation_euler = (0.5 * 3.14159, 0, 0)  # lean back slightly
bpy.ops.object.transform_apply(rotation=True)

# === Boom (cylinder along Y, under sail) ===
bpy.ops.mesh.primitive_cylinder_add(radius=0.03, depth=2.2, vertices=12)
boom = bpy.context.active_object
boom.name = "LaserBoom"
boom.location = (1.2, 0, 2.8)
boom.rotation_euler = (0.5 * 3.14159, 0, 0.5 * 3.14159)
bpy.ops.object.transform_apply(rotation=True)

# === Sail (single triangular plane – Laser mainsail) ===
# Triangle: head at mast top, tack at mast base, clew at boom end
verts_sail = [
    (1.2, 0, 6.2),   # head
    (1.2, 0, 0.3),   # tack
    (1.2, 1.1, 2.9), # clew
]
faces_sail = [(0, 1, 2)]
mesh_sail = bpy.data.meshes.new("LaserSailMesh")
mesh_sail.from_pydata(verts_sail, [], faces_sail)
mesh_sail.update()
sail = bpy.data.objects.new("LaserSail", mesh_sail)
bpy.context.collection.objects.link(sail)
# Give sail a little thickness (solidify) or keep single-sided
for poly in sail.data.polygons:
    poly.use_smooth = True

# === Rudder (flat vertical blade behind transom) ===
bpy.ops.mesh.primitive_cube_add(size=1)
rudder = bpy.context.active_object
rudder.name = "LaserRudder"
rudder.scale = (0.04, 0.5, 0.35)  # thin, tall, some width
rudder.location = (-2.0, 0, -0.2)
bpy.ops.object.transform_apply(scale=True)

# === Tiller (rod from rudder to cockpit) ===
bpy.ops.mesh.primitive_cylinder_add(radius=0.02, depth=0.9, vertices=8)
tiller = bpy.context.active_object
tiller.name = "LaserTiller"
tiller.location = (-1.6, 0.35, 0.15)
tiller.rotation_euler = (0, 0.5 * 3.14159, 0.3)
bpy.ops.object.transform_apply(rotation=True)

# Hull center is already at world origin; no shift needed for Unreal placement

# === Export FBX (same folder as this script, or blend file, or home) ===
try:
    script_dir = os.path.dirname(os.path.abspath(__file__))
except NameError:
    script_dir = os.path.dirname(bpy.data.filepath) if bpy.data.filepath else os.path.expanduser("~")
fbx_path = os.path.join(script_dir, "LaserBoat.fbx")

bpy.ops.object.select_all(action='SELECT')
bpy.ops.export_scene.fbx(
    filepath=fbx_path,
    use_selection=True,
    object_types={'MESH'},
    mesh_smooth_type='FACE',
    add_leaf_bones=False,
    path_mode='AUTO',
    embed_textures=False,
)
print(f"Exported: {fbx_path}")
