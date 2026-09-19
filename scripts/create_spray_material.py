"""Bygger /Game/Materials/Water/M_SprayQuad: skumdråpe-materialet på sprut-poolen (ASailboatPawn::UpdateSpray).

Kjør headless (editoren LUKKET):
  UnrealEditor-Cmd Sailing.uproject -run=pythonscript \
      -script=scripts/create_spray_material.py -unattended -nosplash -nullrhi
Resultatlinjer er merket [SPRAYMAT].

Hver partikkel er et Engine-plan (100×100 uu i XY) i en UInstancedStaticMeshComponent, vendt mot
kameraet på CPU. Levetidsfraksjonen (0..1) ligger i per-instans custom data 0 (settes med
bMarkRenderStateDirty=false per partikkel; den ene BatchUpdateInstancesTransforms-en per frame
laster opp alt). NB: materialet MÅ ha bruksflagget InstancedStaticMeshes, ellers tegnes
standardmaterialet (brune rutete kvadrater).
  opacity = radial² · støy(uv) · sin(π·life) · SprayStrength
"""
import unreal

mel = unreal.MaterialEditingLibrary
EAL = unreal.EditorAssetLibrary
MAT_DIR = "/Game/Materials/Water"
MAT_NAME = "M_SprayQuad"
MAT_PATH = f"{MAT_DIR}/{MAT_NAME}"
NOISE_TEX = f"{MAT_DIR}/T_FoamNoise"


def log(msg):
    unreal.log(f"[SPRAYMAT] {msg}")


class Graph:
    def __init__(self, material):
        self.m, self.row = material, 0

    def node(self, cls, **props):
        self.row += 1
        n = mel.create_material_expression(self.m, cls, -1800 + (self.row % 10) * 160, (self.row // 10) * 140)
        for k, v in props.items():
            n.set_editor_property(k, v)
        return n

    def link(self, src, dst, pin, out=""):
        if not mel.connect_material_expressions(src, out, dst, pin):
            raise RuntimeError(f"kobling feilet: {src.get_name()} → {dst.get_name()}.{pin}")
        return dst

    def param(self, name, default):
        return self.node(unreal.MaterialExpressionScalarParameter, parameter_name=name, default_value=float(default))

    def binop(self, cls, a, b):
        n = self.node(cls)
        for pin, v in (("A", a), ("B", b)):
            if isinstance(v, (int, float)):
                n.set_editor_property("const_a" if pin == "A" else "const_b", float(v))
            else:
                self.link(v, n, pin)
        return n

    def sub(self, a, b): return self.binop(unreal.MaterialExpressionSubtract, a, b)
    def mul(self, a, b): return self.binop(unreal.MaterialExpressionMultiply, a, b)

    def unary(self, cls, a, **props):
        return self.link(a, self.node(cls, **props), "")

    def sat(self, a): return self.unary(unreal.MaterialExpressionSaturate, a)
    def one_minus(self, a): return self.unary(unreal.MaterialExpressionOneMinus, a)

    def mask(self, a, r=False, g=False, b=False):
        return self.link(a, self.node(unreal.MaterialExpressionComponentMask, r=r, g=g, b=b, a=False), "")


def build():
    if EAL.does_asset_exist(MAT_PATH):
        EAL.delete_asset(MAT_PATH)
    material = unreal.AssetToolsHelpers.get_asset_tools().create_asset(
        MAT_NAME, MAT_DIR, unreal.Material, unreal.MaterialFactoryNew())
    material.set_editor_property("blend_mode", unreal.BlendMode.BLEND_TRANSLUCENT)
    material.set_editor_property("two_sided", True)
    # Unlit: kameravendte, lyste quads får bare himmel-/volumlys og blir brune; skum er hvitt uansett.
    material.set_editor_property("shading_model", unreal.MaterialShadingModel.MSM_UNLIT)
    # Uten dette flagget bruker spillet standardmaterialet på ISM-poolen («missing usage flag
    # InstancedStaticMeshes») — samme fallgruve som Nanite-flagget på M_Land.
    mel.set_material_usage(material, unreal.MaterialUsage.MATUSAGE_INSTANCED_STATIC_MESHES)
    g = Graph(material)

    uv = g.node(unreal.MaterialExpressionTextureCoordinate, coordinate_index=0)

    # Levetid (0..1) fra per-instans custom data 0 (ASailboatPawn::UpdateSpray). Forsøkt først å
    # lese den fra instansens Z-skala via TransformVector(Local→World) — ga alltid 1 (noden ser
    # bare komponentens transform), så sin(π·1)=0 og alt ble usynlig.
    life = g.sat(g.node(unreal.MaterialExpressionPerInstanceCustomData, data_index=0, const_default_value=1.0))
    life_env = g.unary(unreal.MaterialExpressionSine, life, period=2.0)          # sin(π·life)

    # Rund, myk dråpe med støy.
    centered = g.sub(uv, 0.5)
    d2 = g.node(unreal.MaterialExpressionDotProduct)
    g.link(centered, d2, "A"); g.link(centered, d2, "B")
    radial = g.sat(g.one_minus(g.mul(g.unary(unreal.MaterialExpressionSquareRoot, d2), 2.2)))
    radial = g.mul(radial, radial)
    noise_tex = unreal.load_asset(NOISE_TEX)
    if not noise_tex:
        raise RuntimeError(f"{NOISE_TEX} mangler — kjør scripts/add_shore_foam.py først")
    noise = g.node(unreal.MaterialExpressionTextureSample, texture=noise_tex,
                   sampler_type=unreal.MaterialSamplerType.SAMPLERTYPE_LINEAR_GRAYSCALE)
    g.link(g.mul(uv, g.param("SprayNoiseTile", 1.5)), noise, "UVs")
    noise_r = g.sat(g.mul(g.sub(g.mask(noise, r=True), 0.15), 1.8))

    opacity = g.mul(g.mul(g.mul(radial, noise_r), life_env), g.param("SprayStrength", 0.9))
    mel.connect_material_property(g.sat(opacity), "", unreal.MaterialProperty.MP_OPACITY)

    color = g.node(unreal.MaterialExpressionVectorParameter, parameter_name="SprayColor",
                   default_value=unreal.LinearColor(0.85, 0.88, 0.92, 1.0))
    mel.connect_material_property(color, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR)

    mel.recompile_material(material)
    EAL.save_loaded_asset(material)
    log(f"bygget {MAT_PATH}: {mel.get_num_material_expressions(material)} noder")


build()
