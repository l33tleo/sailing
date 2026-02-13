import unreal

def create_colored_material(name, path, r, g, b, two_sided=False, translucent=False):
    """Create a simple colored material with a VectorParameter named 'BaseColor'"""

    asset_tools = unreal.AssetToolsHelpers.get_asset_tools()
    mat_factory = unreal.MaterialFactoryNew()

    # Create the material asset
    material = asset_tools.create_asset(name, path, unreal.Material, mat_factory)
    if material is None:
        # Already exists, load it
        material = unreal.EditorAssetLibrary.load_asset(f"{path}/{name}")

    if material is None:
        unreal.log_error(f"Failed to create or load material {name}")
        return

    # We need to use the material editing utilities
    # Create a constant vector expression for the base color
    mel = unreal.MaterialEditingLibrary

    # Create a VectorParameter node
    param_node = mel.create_material_expression(material, unreal.MaterialExpressionVectorParameter, -300, 0)
    param_node.set_editor_property('parameter_name', 'BaseColor')
    param_node.set_editor_property('default_value', unreal.LinearColor(r, g, b, 1.0))

    # Connect to base color
    mel.connect_material_property(param_node, '', unreal.MaterialProperty.MP_BASE_COLOR)

    # Create a scalar parameter for roughness
    roughness_node = mel.create_material_expression(material, unreal.MaterialExpressionScalarParameter, -300, 200)
    roughness_node.set_editor_property('parameter_name', 'Roughness')
    roughness_node.set_editor_property('default_value', 0.5)
    mel.connect_material_property(roughness_node, '', unreal.MaterialProperty.MP_ROUGHNESS)

    if two_sided:
        material.set_editor_property('two_sided', True)

    if translucent:
        material.set_editor_property('blend_mode', unreal.BlendMode.BLEND_TRANSLUCENT)
        # Add opacity parameter
        opacity_node = mel.create_material_expression(material, unreal.MaterialExpressionScalarParameter, -300, 400)
        opacity_node.set_editor_property('parameter_name', 'Opacity')
        opacity_node.set_editor_property('default_value', 0.85)
        mel.connect_material_property(opacity_node, '', unreal.MaterialProperty.MP_OPACITY)

    # Recompile the material
    mel.recompile_material(material)
    unreal.EditorAssetLibrary.save_asset(f"{path}/{name}")
    unreal.log(f"Created material: {path}/{name}")

# Create materials directory
unreal.EditorAssetLibrary.make_directory("/Game/Materials")

# Ocean material - blue, two-sided so it renders from above and below
create_colored_material("M_Ocean", "/Game/Materials", 0.02, 0.15, 0.45, two_sided=True)

# Boat material - brown wood color
create_colored_material("M_Boat", "/Game/Materials", 0.35, 0.2, 0.08)

# Island material - green earthy
create_colored_material("M_Island", "/Game/Materials", 0.15, 0.35, 0.1)

# Island discovered material - bright green
create_colored_material("M_IslandDiscovered", "/Game/Materials", 0.2, 0.8, 0.2)

unreal.log("=== All sailing materials created successfully! ===")
