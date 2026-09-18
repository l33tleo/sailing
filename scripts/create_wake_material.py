"""Bygger /Game/Materials/Water/M_Wake: skum-materialet på kjølvannstrimmelen (UWakeRibbonComponent).

Kjør headless (editoren LUKKET):
  UnrealEditor-Cmd Sailing.uproject -run=pythonscript \
      -script=scripts/create_wake_material.py -unattended -nosplash -nullrhi
Resultatlinjer er merket [WAKEMAT].

Strimmelen er en translucent flate noen cm over vannflaten (Single Layer Water skriver dybde, så
den tegnes riktig oppå). UV0: u = tvers (0 babord … 1 styrbord), v = avstand akterover / 200 cm.
Vertexfargens alfa = alder- og fartsfading fra C++.
  opacity = støy(u·2 + t·0,1, v) · VertexColor.a · sin(πu)^0.7 · (1 − 0,45·midtdipp) · WakeStrength
→ to skumstriper fra hjørnene av akterspeilet med tynnere midt (kjølvannets V), som brytes opp av
støyen og dør ut med alderen. Ikke DepthFade (leser 0 mot Single Layer Water).
"""
import os

import unreal

mel = unreal.MaterialEditingLibrary
EAL = unreal.EditorAssetLibrary
MAT_DIR = "/Game/Materials/Water"
MAT_NAME = "M_Wake"
MAT_PATH = f"{MAT_DIR}/{MAT_NAME}"
NOISE_TEX = f"{MAT_DIR}/T_FoamNoise"


def log(msg):
    unreal.log(f"[WAKEMAT] {msg}")


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

    def add(self, a, b): return self.binop(unreal.MaterialExpressionAdd, a, b)
    def sub(self, a, b): return self.binop(unreal.MaterialExpressionSubtract, a, b)
    def mul(self, a, b): return self.binop(unreal.MaterialExpressionMultiply, a, b)
    def div(self, a, b): return self.binop(unreal.MaterialExpressionDivide, a, b)

    def unary(self, cls, a, **props):
        return self.link(a, self.node(cls, **props), "")

    def sat(self, a): return self.unary(unreal.MaterialExpressionSaturate, a)
    def one_minus(self, a): return self.unary(unreal.MaterialExpressionOneMinus, a)
    def abs(self, a): return self.unary(unreal.MaterialExpressionAbs, a)

    def power(self, a, exp):
        return self.link(a, self.node(unreal.MaterialExpressionPower, const_exponent=float(exp)), "Base")

    def mask(self, a, r=False, g=False, b=False, a_=False):
        return self.link(a, self.node(unreal.MaterialExpressionComponentMask, r=r, g=g, b=b, a=a_), "")

    def ramp(self, x, lo, hi):
        return self.sat(self.div(self.sub(x, lo), float(hi - lo)))


def build():
    if EAL.does_asset_exist(MAT_PATH):
        EAL.delete_asset(MAT_PATH)
    material = unreal.AssetToolsHelpers.get_asset_tools().create_asset(
        MAT_NAME, MAT_DIR, unreal.Material, unreal.MaterialFactoryNew())
    material.set_editor_property("blend_mode", unreal.BlendMode.BLEND_TRANSLUCENT)
    material.set_editor_property("two_sided", True)
    g = Graph(material)

    uv = g.node(unreal.MaterialExpressionTextureCoordinate, coordinate_index=0)
    u, v = g.mask(uv, r=True), g.mask(uv, g=True)
    vc = g.node(unreal.MaterialExpressionVertexColor)
    t = g.node(unreal.MaterialExpressionTime)

    noise_tex = unreal.load_asset(NOISE_TEX)
    if not noise_tex:
        raise RuntimeError(f"{NOISE_TEX} mangler — kjør scripts/add_shore_foam.py først")
    nuv = g.node(unreal.MaterialExpressionAppendVector)
    g.link(g.add(g.mul(u, 2.0), g.mul(t, 0.1)), nuv, "A")
    g.link(g.mul(v, g.param("WakeNoiseTile", 1.0)), nuv, "B")
    noise = g.node(unreal.MaterialExpressionTextureSample, texture=noise_tex,
                   sampler_type=unreal.MaterialSamplerType.SAMPLERTYPE_LINEAR_GRAYSCALE)
    g.link(nuv, noise, "UVs")
    noise_r = g.sat(g.mul(g.sub(g.mask(noise, r=True), g.param("WakeNoiseCut", 0.25)), 2.2))

    edge = g.power(g.unary(unreal.MaterialExpressionSine, u, period=2.0), 0.7)      # 0 i kantene
    mid = g.one_minus(g.ramp(g.abs(g.sub(u, 0.5)), 0.08, 0.22))                     # 1 midt på strimmelen
    shape = g.mul(edge, g.one_minus(g.mul(mid, g.param("WakeMidDip", 0.45))))

    # VertexColor-nodens hovedutgang er RGB; alfa ligger på egen utgang «A».
    vc_alpha = g.node(unreal.MaterialExpressionMultiply)
    g.link(vc, vc_alpha, "A", out="A")
    vc_alpha.set_editor_property("const_b", 1.0)
    opacity = g.mul(g.mul(g.mul(noise_r, shape), vc_alpha), g.param("WakeStrength", 0.85))
    if os.environ.get("WAKE_DEBUG"):
        # Feilsøking: rød, helt ugjennomsiktig strimmel — er den synlig i det hele tatt?
        opacity = g.node(unreal.MaterialExpressionConstant, r=1.0)
    mel.connect_material_property(g.sat(opacity), "", unreal.MaterialProperty.MP_OPACITY)

    color = g.node(unreal.MaterialExpressionVectorParameter, parameter_name="WakeColor",
                   default_value=unreal.LinearColor(1.0, 0.0, 0.0, 1.0) if os.environ.get("WAKE_DEBUG")
                   else unreal.LinearColor(0.92, 0.94, 0.95, 1.0))
    mel.connect_material_property(color, "", unreal.MaterialProperty.MP_BASE_COLOR)
    mel.connect_material_property(g.param("WakeRoughness", 0.75), "", unreal.MaterialProperty.MP_ROUGHNESS)
    mel.connect_material_property(g.node(unreal.MaterialExpressionConstant, r=0.0), "", unreal.MaterialProperty.MP_SPECULAR)

    mel.recompile_material(material)
    EAL.save_loaded_asset(material)
    log(f"bygget {MAT_PATH}: {mel.get_num_material_expressions(material)} noder")


build()
