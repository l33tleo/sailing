import unreal

mel = unreal.MaterialEditingLibrary

# Check parameters on MaterialInstances and Materials
mats_to_check = [
    "/Engine/BasicShapes/BasicShapeMaterial",
    "/Engine/BasicShapes/BasicShapeMaterial_Inst",
    "/Game/Materials/M_Ocean",
    "/Game/Materials/M_Boat",
    "/Game/Materials/M_Island",
]

for mat_path in mats_to_check:
    mat = unreal.EditorAssetLibrary.load_asset(mat_path)
    if mat is None:
        unreal.log(f"NOT FOUND: {mat_path}")
        continue

    unreal.log(f"\n=== {mat_path} (type: {type(mat).__name__}) ===")

    # For any MaterialInterface, get parameter names via the instance methods
    try:
        scalar_names = mel.get_scalar_parameter_names(mat)
        unreal.log(f"  Scalar params: {scalar_names}")
    except:
        unreal.log(f"  Scalar params: (error getting)")

    try:
        vector_names = mel.get_vector_parameter_names(mat)
        unreal.log(f"  Vector params: {vector_names}")
    except:
        unreal.log(f"  Vector params: (error getting)")

    try:
        texture_names = mel.get_texture_parameter_names(mat)
        unreal.log(f"  Texture params: {texture_names}")
    except:
        unreal.log(f"  Texture params: (error getting)")

unreal.log("\n=== Done ===")
