"""
Gjenbrukbare Blender-helpere for Sailing Game.

Kjøres via mcp__blender__execute_blender_code:
    exec(open('/Users/leovonschwind/sailing/scripts/blender/sailing_blender_utils.py').read())

Følger konvensjonene i scripts/blender/CLAUDE.md.
"""

import bpy
import bmesh
from mathutils import Vector


# ----- Collections --------------------------------------------------------

COLLECTION_NAMES = [
    "01_Hull", "02_Rigging", "03_Sails", "04_Fittings", "99_Reference",
]


def ensure_collections(scene=None):
    """Opprett standard-collections hvis de ikke finnes."""
    scene = scene or bpy.context.scene
    for name in COLLECTION_NAMES:
        if name not in bpy.data.collections:
            coll = bpy.data.collections.new(name)
            scene.collection.children.link(coll)
    return {name: bpy.data.collections[name] for name in COLLECTION_NAMES}


def move_to_collection(obj, collection_name):
    """Flytt obj til gitt collection (unlink fra alle andre først)."""
    target = bpy.data.collections.get(collection_name)
    if target is None:
        raise ValueError(f"Collection '{collection_name}' finnes ikke. Kjør ensure_collections() først.")
    for coll in list(obj.users_collection):
        coll.objects.unlink(obj)
    target.objects.link(obj)


# ----- Markers (målinger først) ------------------------------------------

def add_marker(name, location, kind="PLAIN_AXES", size=0.1):
    """
    Plasser en empty som målemarkør. Gir navngivning per konvensjon:
        SOCKET_<Name>  — festepunkt for UE
        PIVOT_<Name>   — rotasjonspunkt
        STN_<idx>      — skrog-stasjon
    """
    if name in bpy.data.objects:
        bpy.data.objects.remove(bpy.data.objects[name], do_unlink=True)
    empty = bpy.data.objects.new(name, None)
    empty.empty_display_type = kind
    empty.empty_display_size = size
    empty.location = Vector(location)
    bpy.context.scene.collection.objects.link(empty)
    return empty


# ----- Materialer --------------------------------------------------------

MATERIAL_COLORS = {
    "M_Hull":   (0.95, 0.95, 0.92, 1.0),  # hvit maling
    "M_Sail":   (0.98, 0.98, 0.96, 1.0),  # off-white
    "M_Spar":   (0.82, 0.82, 0.85, 1.0),  # aluminium
    "M_Foil":   (0.80, 0.80, 0.82, 1.0),  # hvit plast (ror/daggerboard)
    "M_Tiller": (0.55, 0.35, 0.18, 1.0),  # tre
    "M_Wood":   (0.55, 0.35, 0.18, 1.0),  # alias for tre
}


def ensure_material(name, base_color=None, roughness=0.4):
    """Lag material hvis det ikke finnes. Returnerer material."""
    mat = bpy.data.materials.get(name)
    if mat is not None:
        return mat
    mat = bpy.data.materials.new(name=name)
    mat.use_nodes = True
    principled = mat.node_tree.nodes.get("Principled BSDF")
    if principled:
        color = base_color if base_color is not None else MATERIAL_COLORS.get(name, (0.7, 0.7, 0.7, 1.0))
        principled.inputs["Base Color"].default_value = color
        principled.inputs["Roughness"].default_value = roughness
    return mat


def assign_material(obj, mat_name):
    """Tildel ett material til et objekt (fjerner tidligere slots)."""
    mat = ensure_material(mat_name)
    if obj.data is None:
        return
    obj.data.materials.clear()
    obj.data.materials.append(mat)


# ----- Hull fra stasjoner ------------------------------------------------

def create_hull_from_stations(
    name,
    length,
    beam,
    depth,
    bow_beam,
    transom_beam,
    deck_height=0.30,
    bottom_z=None,
    n_stations=11,
    chine_offset=0.04,
    bow_rocker=0.08,
    stern_rocker=0.02,
):
    """
    Bygg et skrog som loftede tverrsnitt langs X-aksen.

    +X peker mot baugen (forover). Båten sentrert i origo.

    Tverrsnittet per stasjon er V-formet med chine (knekk):
        kjøl (bunn, Y=0) — babord chine — babord dekk — styrbord dekk — styrbord chine

    Args:
        length: båtens totale lengde (m)
        beam: maks bredde ved midten (m)
        depth: dybde fra dekk til bunn (m)
        bow_beam: bredde ved pram-baug (m)
        transom_beam: bredde ved akterspeil (m)
        deck_height: Z-koordinat for gunwale/dekk
        bottom_z: Z-koordinat for kjøl (default: -depth * 0.25)
        n_stations: antall tverrsnitt (typisk 11 for 20cm-stasjoner over 2.3m)
        chine_offset: hvor mye chine-punktet er over bunn (m)
        bow_rocker / stern_rocker: hvor mye bunnen stiger mot hver ende

    Returnerer Hull-objektet.
    """
    if bottom_z is None:
        bottom_z = -depth * 0.25

    if name in bpy.data.objects:
        bpy.data.objects.remove(bpy.data.objects[name], do_unlink=True)

    mesh = bpy.data.meshes.new(f"{name}Mesh")
    obj = bpy.data.objects.new(name, mesh)
    bpy.context.scene.collection.objects.link(obj)

    bm = bmesh.new()
    stations = []

    for i in range(n_stations):
        xnorm = i / (n_stations - 1)  # 0=akter, 1=baug
        x = -length / 2 + xnorm * length

        # Beam-profil: smalere i endene, maks i midten
        if xnorm <= 0.5:
            t = xnorm / 0.5
            half_beam = (transom_beam + (beam - transom_beam) * t) / 2
        else:
            t = (xnorm - 0.5) / 0.5
            half_beam = (beam - (beam - bow_beam) * (t ** 1.4)) / 2

        # Rocker (bunn-kurve)
        if xnorm <= 0.5:
            bottom = bottom_z + stern_rocker * (0.5 - xnorm) * 2
        else:
            bottom = bottom_z + bow_rocker * (xnorm - 0.5) * 2

        deck = deck_height + 0.04 * xnorm  # litt høyere foran
        chine_z = bottom + chine_offset

        keel    = bm.verts.new((x, 0.0, bottom))
        p_chine = bm.verts.new((x, -half_beam * 0.85, chine_z))
        p_deck  = bm.verts.new((x, -half_beam,        deck))
        s_deck  = bm.verts.new((x,  half_beam,        deck))
        s_chine = bm.verts.new((x,  half_beam * 0.85, chine_z))
        stations.append([keel, p_chine, p_deck, s_deck, s_chine])

    bm.verts.ensure_lookup_table()

    # Quads mellom tilstøtende stasjoner
    for i in range(n_stations - 1):
        a, b = stations[i], stations[i + 1]
        bm.faces.new([a[0], b[0], b[1], a[1]])
        bm.faces.new([a[1], b[1], b[2], a[2]])
        bm.faces.new([a[3], b[3], b[4], a[4]])
        bm.faces.new([a[4], b[4], b[0], a[0]])

    # Lukk endene (akter-transom og pram-baug)
    a = stations[0]
    bm.faces.new([a[0], a[1], a[2], a[3], a[4]])
    b = stations[-1]
    bm.faces.new([b[4], b[3], b[2], b[1], b[0]])

    bm.normal_update()
    for f in bm.faces:
        f.smooth = True
    bm.to_mesh(mesh)
    bm.free()

    assign_material(obj, "M_Hull")
    return obj, stations


def close_deck_between(obj, stations, i1, i2):
    """Lukk dekket (quad) mellom to stasjoner. Kall før bmesh-arbeidet avsluttes, eller bruk edit-mode."""
    # Dette krever at stations-referanser fortsatt er gyldige; enklere å bygge dekk
    # direkte i create_hull_from_stations med deck_start/deck_end-parametre.
    raise NotImplementedError("Bruk create_hull_from_stations's deck_ranges-parameter i stedet")


# ----- Pram-hull (Optimist-type: flat bunn, hard chine) ------------------

def create_pram_hull(name, stations, close_bottom=True, close_transoms=True,
                     shade_chine_flat=True):
    """
    Bygg et pram-skrog (Optimist-type) fra en liste stasjoner.

    En pram er fundamentalt forskjellig fra V-skrog: FLAT bunn mellom to parallelle
    chines, hard knekk til nesten-vertikale topsider, og flate rake transoms i baug
    og akter. Bygget som sammenstilte flate paneler (developable surface), akkurat
    som en ekte Optimist er bygget av kryssfiner-plater.

    +X peker mot baugen (forover, matcher UE5). Båten sentrert i origo langs X.
    Vannlinje z=0 oppnås ved at base-line legges litt under: typisk bottom_z rundt
    0.10 m og gunwale_z rundt 0.50 m i meters (etter mm→m konvertering).

    Args:
        name: objekt-navn (f.eks. 'SM_Boat_Optimist_Hull_LOD0')
        stations: liste av tupler per stasjon:
            (x, bottom_half_width, gunwale_half_width, bottom_z, gunwale_z)
            — alle i METER (konverter fra mm før kall).
            — x øker mot baug. Første stasjon = akter-transom, siste = baug-transom.
            Senter i x langs lista: bmesh sentrerer ikke automatisk.
        close_bottom: hvis True, bygg flat bunnpanel som quads mellom bottom-verts.
        close_transoms: hvis True, lukk endeflatene som N-gons (raket transoms).
        shade_chine_flat: hvis True, marker chine-kanter som sharp (flat shading over
            kniken). Ellers full smooth.

    Returnerer Hull-objektet.

    Topologi per stasjon (4 verts, sett akterfra):
        port_gunwale --------- stbd_gunwale         z = gunwale_z
              |                       |
              |   (side panels)       |
              |                       |
        port_chine ----------- stbd_chine           z = bottom_z
              \_______________________/
                  (flat bunnpanel)
    """
    if name in bpy.data.objects:
        bpy.data.objects.remove(bpy.data.objects[name], do_unlink=True)

    mesh = bpy.data.meshes.new(f"{name}Mesh")
    obj = bpy.data.objects.new(name, mesh)
    bpy.context.scene.collection.objects.link(obj)

    bm = bmesh.new()
    station_verts = []

    for (x, bhw, ghw, bz, gz) in stations:
        p_chine   = bm.verts.new((x, -bhw, bz))
        s_chine   = bm.verts.new((x,  bhw, bz))
        p_gunwale = bm.verts.new((x, -ghw, gz))
        s_gunwale = bm.verts.new((x,  ghw, gz))
        # Rekkefølge: [port_chine, stbd_chine, port_gunwale, stbd_gunwale]
        station_verts.append([p_chine, s_chine, p_gunwale, s_gunwale])

    bm.verts.ensure_lookup_table()

    # Side-panel (babord og styrbord): quads mellom chine og gunwale per segment.
    # Winding verifisert med cross-product-test: begge sider har normal som peker
    # utover hull (port -Y, stbd +Y). Se commit-historikk for normal-verifisering.
    chine_edges = []
    for i in range(len(stations) - 1):
        a, b = station_verts[i], station_verts[i + 1]
        # Port side panel: port_chine[a] → port_chine[b] → port_gunwale[b] → port_gunwale[a]
        bm.faces.new([a[0], b[0], b[2], a[2]])
        # Stbd side panel: stbd_gunwale[a] → stbd_gunwale[b] → stbd_chine[b] → stbd_chine[a]
        bm.faces.new([a[3], b[3], b[1], a[1]])
        # Spar chine-edges for sharp-marking senere
        chine_edges.append((a[0], b[0]))
        chine_edges.append((a[1], b[1]))

    # Bunnpanel: flat, quads mellom port_chine og stbd_chine per segment
    if close_bottom:
        for i in range(len(stations) - 1):
            a, b = station_verts[i], station_verts[i + 1]
            # port_chine[a] → stbd_chine[a] → stbd_chine[b] → port_chine[b]
            bm.faces.new([a[0], a[1], b[1], b[0]])

    # Endeflater (baug og akter-transom): flate N-gon (fire hjørner hver).
    # Winding valgt så normal peker utover (-X for akter, +X for baug) —
    # verifisert via cross-product-test.
    if close_transoms:
        first = station_verts[0]   # akter (antatt første i lista)
        last = station_verts[-1]   # baug
        # Akter-transom (normal → -X, utover akter):
        # stbd_gunwale → stbd_chine → port_chine → port_gunwale
        bm.faces.new([first[3], first[1], first[0], first[2]])
        # Baug-transom (normal → +X, utover baug):
        # stbd_chine → stbd_gunwale → port_gunwale → port_chine
        bm.faces.new([last[1], last[3], last[2], last[0]])

    bm.normal_update()

    # Shading: smooth over panels, sharp på chine (via split-normals via edge.smooth)
    for f in bm.faces:
        f.smooth = True

    if shade_chine_flat:
        # Merk chine-kanter som non-smooth. Siden faces er smooth, markerer vi
        # chine-kantene som sharp ved å sette edge.smooth=False. Blender respekterer
        # dette via Auto Smooth / Custom Split Normals.
        for edge in bm.edges:
            v1, v2 = edge.verts
            # En chine-kant går mellom to chine-verts (indexes 0 eller 1 innen
            # station_verts). Vi sjekker om begge endepunkter var chine-verts.
            v1_is_chine = any(v1 == sv[0] or v1 == sv[1] for sv in station_verts)
            v2_is_chine = any(v2 == sv[0] or v2 == sv[1] for sv in station_verts)
            if v1_is_chine and v2_is_chine:
                edge.smooth = False

    bm.to_mesh(mesh)
    bm.free()

    # Auto Smooth for å respektere flat-merkede edges (sharp chines).
    # Blender 4.1+ fjernet mesh.use_auto_smooth — nå via modifier eller operator.
    # Bruk shade_auto_smooth operator hvis tilgjengelig, ellers fallback.
    try:
        # Blender 4.1+
        bpy.context.view_layer.objects.active = obj
        obj.select_set(True)
        bpy.ops.object.shade_auto_smooth(angle=1.0472)  # 60 grader
    except Exception:
        # Eldre Blender: fallback til flag hvis tilgjengelig
        if hasattr(mesh, "use_auto_smooth"):
            mesh.use_auto_smooth = True
            mesh.auto_smooth_angle = 1.0472

    assign_material(obj, "M_Hull")
    return obj


def optimist_class_stations():
    """
    Returnerer standard Optimist klasseregel-stasjoner for bruk med create_pram_hull().

    Verdier fra scripts/blender/reference/optimist_class_measurements.md.
    Alle verdier konvertert til meter. Base-line ved z=0. Båt sentrert i origo
    langs X (baug på +1.15, akter på -1.15).

    Returnerer: liste med 6 tupler (x, bhw, ghw, bz, gz), akter → baug.
    """
    # Rådata i mm (fra measurement-fil), x=0 ved baug-transom:
    raw_mm = [
        # (x_from_bow, bottom_half, gunwale_half, bottom_z, gunwale_z)
        (0,    215, 390, 165, 535),  # baug-transom
        (460,  380, 510, 120, 520),
        (920,  500, 555, 105, 510),
        (1380, 565, 565, 105, 505),  # max beam 1130 mm
        (1840, 480, 550, 115, 500),
        (2300, 340, 540, 135, 465),  # akter-transom
    ]
    # Konverter: mm→m, sentrer langs X (bow positiv), og reverser så liste går
    # akter → baug (for at create_pram_hull skal bygge akter-transom først).
    # Vertikalt: skift så rocker-lavpunktet kommer under vannlinjen (z=0).
    # CLAUDE.md forventer hull bottom ≈ -0.10, gunwale ≈ +0.30.
    # Klasseregel-baseline er 105 mm under hull-lavpunkt → shift -0.205 m.
    length_m = 2.300
    z_offset_m = -0.205  # flytter baseline slik at bunn-lavpunkt ~-0.10 m
    stations = []
    for (x_mm, bhw_mm, ghw_mm, bz_mm, gz_mm) in raw_mm:
        # x_from_bow=0 → x_game = +length/2 (baug på +X). x_from_bow=2300 → -length/2.
        x = (length_m / 2) - (x_mm / 1000.0)
        stations.append((
            x,
            bhw_mm / 1000.0,
            ghw_mm / 1000.0,
            bz_mm / 1000.0 + z_offset_m,
            gz_mm / 1000.0 + z_offset_m,
        ))
    # Sorter akter (lavest x) → baug (høyest x)
    stations.sort(key=lambda s: s[0])
    return stations


# ----- Seil --------------------------------------------------------------

def create_optimist_sail(name="Sail", tack=None, clew=None, throat=None, peak=None,
                         nu=8, nv=10, draft=0.10, lee=1.0, material="M_Sail",
                         leech_corner=None, leech_v=0.6):
    """
    Bygg et Optimist spri-seil med bukt (camber).

    Optimist-seilet er FEMKANTET: leech (akterkanten) har et knekkpunkt (roach)
    der øvre battenen sitter. Sett `leech_corner` (verdenskoord.) for å aktivere
    det 5. hjørnet; `leech_v` er høyde-fraksjonen (0=clew, 1=peak) der knekken
    sitter. Uten `leech_corner` bygges en rett kvadrilateral leech (4 hjørner).

    Hjørner i verdenskoordinater (X=baug forover, Z=opp, Y=tverrskips):
        tack         — nedre forlig (bom forende ved mast)
        clew         — nedre akter (bom ytterende)
        throat       — øvre forlig (mast-topp)
        peak         — øvre akter (sprit øvre ende)
        leech_corner — knekkpunkt i leech (roach), mellom clew og peak

    Grid: nu segmenter langs korden (luff→leech), nv opp (foot→head). Hjørnene
    snappes eksakt til riggforankringene, så luff følger masta, foot følger bommen,
    og peak sitter på sprit-enden. For skarp roach-knekk: velg nv slik at en
    grid-rad lander på leech_v (f.eks. nv=10, leech_v=0.6 → rad 6).

    Camber legges som Y-forskyvning på interiør-vertsene:
        belly = draft * korde * sin(pi*u**0.85) * sin(pi*v) * lee
    Dette gir 0 bukt langs hele kanten (luff/foot/leech/head ligger i seilplanet)
    og maks bukt ~44 % akter for luff, midt på høyden — en jevn, realistisk seilsetting.
    `draft` = maks bukt som andel av lokal korde. `lee` = +1/-1 (hvilken side bukten går).

    Bevarer collection-medlemskap hvis et objekt med samme navn finnes fra før.
    Returnerer det nye mesh-objektet. Solidify/recalc gjøres av kalleren.
    """
    import math
    tack, clew = Vector(tack), Vector(clew)
    throat, peak = Vector(throat), Vector(peak)
    lc = Vector(leech_corner) if leech_corner is not None else None

    def leech_at(v):
        # Todelt leech gjennom knekkpunkt (roach), ellers rett clew→peak
        if lc is None:
            return clew.lerp(peak, v)
        if v <= leech_v:
            return clew.lerp(lc, v / leech_v) if leech_v > 0 else clew.copy()
        return lc.lerp(peak, (v - leech_v) / (1.0 - leech_v))

    verts = []
    for j in range(nv + 1):
        v = j / nv
        luff_pt = tack.lerp(throat, v)   # forlig kant (mast) ved denne høyden
        leech_pt = leech_at(v)           # akter kant (todelt leech) ved denne høyden
        chord = (leech_pt - luff_pt).length
        for i in range(nu + 1):
            u = i / nu
            p = luff_pt.lerp(leech_pt, u).copy()
            p.y += draft * chord * math.sin(math.pi * (u ** 0.85)) * math.sin(math.pi * v) * lee
            verts.append(p)

    faces = []
    row = nu + 1
    for j in range(nv):
        for i in range(nu):
            a = j * row + i
            faces.append((a, a + 1, a + row + 1, a + row))

    # Bevar collection fra evt. eksisterende objekt med samme navn
    target_colls = None
    if name in bpy.data.objects:
        old = bpy.data.objects[name]
        target_colls = list(old.users_collection)
        bpy.data.objects.remove(old, do_unlink=True)

    me = bpy.data.meshes.new(name + "_mesh")
    me.from_pydata([tuple(p) for p in verts], [], faces)
    me.update()
    for poly in me.polygons:
        poly.use_smooth = True

    obj = bpy.data.objects.new(name, me)
    if target_colls:
        for c in target_colls:
            c.objects.link(obj)
    else:
        bpy.context.scene.collection.objects.link(obj)
    assign_material(obj, material)
    return obj


# ----- FBX-eksport -------------------------------------------------------

def setup_ue_export(filepath, objects=None):
    """
    Eksporter valgte objekter til FBX med UE5-kompatible innstillinger.

    Args:
        filepath: absolutt sti til .fbx
        objects: liste av objektnavn (eller None = bruk aktivt valg)

    Returnerer: antall bytes skrevet.
    """
    if objects is not None:
        bpy.ops.object.select_all(action="DESELECT")
        for name in objects:
            o = bpy.data.objects.get(name)
            if o is None:
                raise ValueError(f"Objekt '{name}' finnes ikke")
            o.hide_viewport = False
            o.hide_render = False
            o.select_set(True)
        # Sett aktivt til første
        bpy.context.view_layer.objects.active = bpy.data.objects[objects[0]]

    # Bygg modellen i Blender med:
    #   +X = baug (forover, matcher UE5 pawn forward)
    #   +Z = opp
    # Disse akse-innstillingene mapper Blender-aksene direkte til UE5 uten omrotasjon.
    bpy.ops.export_scene.fbx(
        filepath=filepath,
        use_selection=True,
        object_types={"MESH"},
        apply_unit_scale=True,
        apply_scale_options="FBX_SCALE_ALL",
        axis_forward="X",
        axis_up="Z",
        bake_space_transform=True,
        mesh_smooth_type="FACE",
        use_mesh_modifiers=True,
        add_leaf_bones=False,
        embed_textures=False,
        path_mode="COPY",
        global_scale=1.0,
    )

    import os
    return os.path.getsize(filepath)


# ----- Rendering for verifisering ----------------------------------------

def render_orthos(output_dir, name, resolution=1024):
    """
    Render front/side/top ortho til <output_dir>/<name>_{front,side,top}.png.
    Bruk før/etter modelleringssteg for sammenligning.
    """
    import os
    os.makedirs(output_dir, exist_ok=True)

    scene = bpy.context.scene
    render = scene.render
    render.resolution_x = resolution
    render.resolution_y = resolution
    render.film_transparent = True

    # Opprett eller finn ortho-kamera
    cam_name = "_OrthoRenderCam"
    cam_data = bpy.data.cameras.get(cam_name)
    if cam_data is None:
        cam_data = bpy.data.cameras.new(cam_name)
    cam_data.type = "ORTHO"
    cam_data.ortho_scale = 3.5

    cam = bpy.data.objects.get(cam_name)
    if cam is None:
        cam = bpy.data.objects.new(cam_name, cam_data)
        scene.collection.objects.link(cam)

    scene.camera = cam

    views = {
        "front": ((0, -5, 1), (1.5708, 0, 0)),     # se fra -Y
        "side":  ((5, 0, 1),  (1.5708, 0, 1.5708)),  # se fra +X
        "top":   ((0, 0, 5),  (0, 0, 0)),            # se fra +Z
    }

    for view_name, (loc, rot) in views.items():
        cam.location = loc
        cam.rotation_euler = rot
        render.filepath = os.path.join(output_dir, f"{name}_{view_name}.png")
        bpy.ops.render.render(write_still=True)

    return output_dir
