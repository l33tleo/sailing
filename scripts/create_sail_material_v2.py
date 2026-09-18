"""Bygger /Game/ModelsV2/M_SailV2: seilduk med bukt og flagring i shaderen (WorldPositionOffset).

Kjør headless (editoren LUKKET):
  UnrealEditor-Cmd Sailing.uproject -run=pythonscript \
      -script=scripts/create_sail_material_v2.py -unattended -nosplash -nullrhi
Resultatlinjer er merket [SAILMAT]. Setter også materialet på SM_Boat_Rig sin M_Sail-slot.

Seilet (SM_Boat_Rig, se scripts/blender/export_optimist_parts.py) er FLATT og enkeltsidig med
UV0: u = forlig(0)→leech(1), v = fot→topp i Blender — UE flipper V ved import, så v_UE=0 er
toppen. Bukten legges langs riggens lokale +Y-akse (transformert til verden), som roterer med
bommen: SailSide=+1 → bukt mot styrbord (= le når bommen står ut til styrbord).

Per-frame-parametre (settes av USailRigComponent::ApplyToMesh på en MID):
  SailFill  0..1  hvor mye bukt (fra tilsynelatende vind og angrepsvinkel)
  SailSide  ±1    hvilken side bukten er på (bommens side)
  Flutter   0..1  flagring (seilet står i vinden / i jern)

  belly   = SailBellyMaxCm · SailFill · SailSide · sin(π·u^0.85) · sin(π·v)   (null langs alle kanter)
  flutter = FlutterAmpCm · Flutter · u² · sat(1.5·sin(π·v)) · (0.6·sin(2π(3u−4t)) + 0.4·sin(2π(5v+1.2u−6.3t)))
  WPO     = LocalY_WS · (belly + flutter)

Utseende: lys duk, prosedyrale tverrgående paneler/sømmer (PanelCount) og to spilelommer i leech,
TwoSided + TwoSidedFoliage så sola lyser gjennom duken (SubsurfaceColor). Båten kaster ikke skygge
(se ASailboatPawn::BeginPlay), så transmisjonen «skygges» aldri bort.

Bevisst unngått (krasjer editoren ved skripting): SetMaterialAttributes-noden og
get_material_expression_input_names() på Make-noden. Alt kobles pinne for pinne.
"""
import unreal

mel = unreal.MaterialEditingLibrary
EAL = unreal.EditorAssetLibrary
MAT_DIR = "/Game/ModelsV2"
MAT_NAME = "M_SailV2"
MAT_PATH = f"{MAT_DIR}/{MAT_NAME}"
RIG_MESH = f"{MAT_DIR}/SM_Boat_Rig"
NOISE_TEX = "/Game/Materials/Water/T_FoamNoise"


def log(msg):
    unreal.log(f"[SAILMAT] {msg}")


class Graph:
    def __init__(self, material):
        self.m = material
        self.row = 0

    def node(self, cls, **props):
        self.row += 1
        n = mel.create_material_expression(self.m, cls, -2000 + (self.row % 12) * 160, (self.row // 12) * 140)
        for k, v in props.items():
            n.set_editor_property(k, v)
        return n

    def link(self, src, dst, pin, out=""):
        if not mel.connect_material_expressions(src, out, dst, pin):
            raise RuntimeError(f"kobling feilet: {src.get_name()} → {dst.get_name()}.{pin}")
        return dst

    def const(self, v):
        return self.node(unreal.MaterialExpressionConstant, r=float(v))

    def vec3(self, r, g, b):
        return self.node(unreal.MaterialExpressionConstant3Vector, constant=unreal.LinearColor(r, g, b, 1.0))

    def param(self, name, default):
        return self.node(unreal.MaterialExpressionScalarParameter, parameter_name=name, default_value=float(default))

    def vparam(self, name, r, g, b):
        return self.node(unreal.MaterialExpressionVectorParameter, parameter_name=name,
                         default_value=unreal.LinearColor(r, g, b, 1.0))

    def binop(self, cls, a, b):
        n = self.node(cls)
        for pin, v in (("A", a), ("B", b)):
            if isinstance(v, (int, float)):
                n.set_editor_property("const_a" if pin == "A" else "const_b", float(v))
            else:
                self.link(v, n, pin)
        return n

    def add(self, a, b): return self.binop(unreal.MaterialExpressionAdd, a, b)
    def sub(self, a, b): return self.binop(unreal.MaterialExpressionSubtract, a, b)
    def mul(self, a, b): return self.binop(unreal.MaterialExpressionMultiply, a, b)
    def div(self, a, b): return self.binop(unreal.MaterialExpressionDivide, a, b)

    def unary(self, cls, a, **props):
        return self.link(a, self.node(cls, **props), "")

    def sat(self, a): return self.unary(unreal.MaterialExpressionSaturate, a)
    def one_minus(self, a): return self.unary(unreal.MaterialExpressionOneMinus, a)
    def abs(self, a): return self.unary(unreal.MaterialExpressionAbs, a)
    def frac(self, a): return self.unary(unreal.MaterialExpressionFrac, a)

    def sin_pi(self, a):
        """sin(π·a): Sine-noden er sin(2π·x/period)."""
        return self.unary(unreal.MaterialExpressionSine, a, period=2.0)

    def sin_2pi(self, a):
        return self.unary(unreal.MaterialExpressionSine, a, period=1.0)

    def power(self, a, exp):
        n = self.node(unreal.MaterialExpressionPower, const_exponent=float(exp))
        return self.link(a, n, "Base")

    def lerp(self, a, b, alpha):
        n = self.node(unreal.MaterialExpressionLinearInterpolate)
        for pin, v in (("A", a), ("B", b), ("Alpha", alpha)):
            if isinstance(v, (int, float)):
                n.set_editor_property({"A": "const_a", "B": "const_b", "Alpha": "const_alpha"}[pin], float(v))
            else:
                self.link(v, n, pin)
        return n

    def mask(self, a, r=False, g=False, b=False):
        n = self.node(unreal.MaterialExpressionComponentMask, r=r, g=g, b=b, a=False)
        return self.link(a, n, "")

    def ramp(self, x, lo, hi):
        """saturate((x - lo) / (hi - lo)); lo/hi kan være noder eller tall."""
        both_numbers = isinstance(lo, (int, float)) and isinstance(hi, (int, float))
        span = float(hi - lo) if both_numbers else self.sub(hi, lo)
        return self.sat(self.div(self.sub(x, lo), span))

    def band(self, x, center, half_width):
        """1 innenfor |x-center| < half_width, myk kant."""
        return self.one_minus(self.ramp(self.abs(self.sub(x, center)), half_width * 0.5, half_width))


def build():
    if EAL.does_asset_exist(MAT_PATH):
        EAL.delete_asset(MAT_PATH)
    material = unreal.AssetToolsHelpers.get_asset_tools().create_asset(
        MAT_NAME, MAT_DIR, unreal.Material, unreal.MaterialFactoryNew())
    material.set_editor_property("two_sided", True)
    material.set_editor_property("shading_model", unreal.MaterialShadingModel.MSM_TWO_SIDED_FOLIAGE)
    g = Graph(material)

    # --- UV: u forlig→leech, v fot→topp (Blender-retning; UE har flippet V ved import) ---
    uv = g.node(unreal.MaterialExpressionTextureCoordinate, coordinate_index=0)
    u = g.mask(uv, r=True)
    v = g.one_minus(g.mask(uv, g=True))

    # --- Per-frame-parametre ---
    fill = g.param("SailFill", 0.0)
    side = g.param("SailSide", 1.0)
    flutter = g.param("Flutter", 0.0)

    # --- Bukt: null langs alle kanter, maks ~44 % akter for forliget, midt på høyden ---
    shape = g.mul(g.sin_pi(g.power(u, 0.85)), g.sin_pi(v))
    belly = g.mul(g.mul(g.mul(shape, fill), side), g.param("SailBellyMaxCm", 22.0))

    # --- Flagring: bølger som løper akterover, størst ved leech, null ved forlig/fot/topp ---
    t = g.node(unreal.MaterialExpressionTime)
    r1 = g.sin_2pi(g.sub(g.mul(u, 3.0), g.mul(t, 4.0)))
    r2 = g.sin_2pi(g.sub(g.add(g.mul(v, 5.0), g.mul(u, 1.2)), g.mul(t, 6.3)))
    ripple = g.add(g.mul(r1, 0.6), g.mul(r2, 0.4))
    env = g.mul(g.mul(u, u), g.sat(g.mul(g.sin_pi(v), 1.5)))
    flap = g.mul(g.mul(g.mul(ripple, env), flutter), g.param("FlutterAmpCm", 6.0))

    # --- Forskyvningsretning: riggens lokale +Y i verden (roterer med bommen) ---
    local_y = g.node(unreal.MaterialExpressionTransform,
                     transform_source_type=unreal.MaterialVectorCoordTransformSource.TRANSFORMSOURCE_LOCAL,
                     transform_type=unreal.MaterialVectorCoordTransform.TRANSFORM_WORLD)
    g.link(g.vec3(0.0, 1.0, 0.0), local_y, "")
    wpo = g.mul(local_y, g.add(belly, flap))
    mel.connect_material_property(wpo, "", unreal.MaterialProperty.MP_WORLD_POSITION_OFFSET)

    # --- Duk: lys farge, svak vev, tverrgående paneler med sømmer, spilelommer i leech ---
    cloth = g.vparam("ClothColor", 0.93, 0.92, 0.87)
    noise_tex = unreal.load_asset(NOISE_TEX)
    weave = None
    if noise_tex:
        smp = g.node(unreal.MaterialExpressionTextureSample, texture=noise_tex)
        g.link(g.mul(uv, g.param("WeaveTiling", 40.0)), smp, "UVs")
        contrast = g.param("WeaveContrast", 0.06)
        weave = g.add(g.sub(1.0, g.mul(contrast, 0.5)), g.mul(g.mask(smp, r=True), contrast))
    else:
        log(f"ADVARSEL: {NOISE_TEX} mangler — ingen vev-tekstur")

    seam_pos = g.abs(g.sub(g.frac(g.add(g.mul(v, g.param("PanelCount", 6.0)), 0.5)), 0.5))
    seam = g.one_minus(g.ramp(seam_pos, 0.0, g.param("SeamWidth", 0.012)))
    # Spilelommer: to striper i leech (u > 0.78) ved 35 % og 65 % høyde.
    in_leech = g.ramp(u, 0.76, 0.80)
    pockets = g.mul(g.sat(g.add(g.band(v, 0.35, 0.02), g.band(v, 0.65, 0.02))), in_leech)
    marks = g.sat(g.add(seam, g.mul(pockets, 0.7)))
    darken = g.lerp(1.0, g.param("SeamDarken", 0.85), marks)

    color = g.mul(cloth, darken)
    if weave is not None:
        color = g.mul(color, weave)
    mel.connect_material_property(color, "", unreal.MaterialProperty.MP_BASE_COLOR)

    rough = g.lerp(g.param("Roughness", 0.62), g.param("SeamRoughness", 0.52), marks)
    mel.connect_material_property(rough, "", unreal.MaterialProperty.MP_ROUGHNESS)
    mel.connect_material_property(g.const(0.0), "", unreal.MaterialProperty.MP_SPECULAR)

    # Gjennomskinn (TwoSidedFoliage): sol bak seilet lyser gjennom duken; sømmene er tettere.
    trans = g.mul(g.mul(cloth, g.vparam("SubsurfaceTint", 0.9, 0.88, 0.8)), g.param("Translucency", 0.8))
    trans = g.mul(trans, g.lerp(1.0, 0.5, marks))
    mel.connect_material_property(trans, "", unreal.MaterialProperty.MP_SUBSURFACE_COLOR)

    mel.recompile_material(material)
    EAL.save_loaded_asset(material)
    log(f"bygget {MAT_PATH}: {mel.get_num_material_expressions(material)} noder")
    return material


def assign_to_rig(material):
    mesh = unreal.load_asset(RIG_MESH)
    if not mesh:
        log(f"{RIG_MESH} finnes ikke — kjør scripts/import_boat_parts.py")
        return
    for i, sm in enumerate(mesh.get_editor_property("static_materials")):
        if str(sm.get_editor_property("material_slot_name")) == "M_Sail":
            mesh.set_material(i, material)
            EAL.save_loaded_asset(mesh, only_if_is_dirty=False)
            log(f"satte {MAT_NAME} på {RIG_MESH} slot {i}")
            return
    log(f"ADVARSEL: {RIG_MESH} har ingen M_Sail-slot")


assign_to_rig(build())
