"""Builds the game's imported content from the versioned sources. Run in the editor commandlet:

    source Tools/ue.sh && SLOP_IMPORT_ONLY=audio,materials editor_py "$PROJ_DIR/Tools/import_assets.py"

SLOP_IMPORT_ONLY (comma list of steps: audio, materials, fighters, surfaces, props,
stages, slots, portraits) limits the run; without it every step runs. `slots` gives the imported fighters and
stages their material instances; run it in a second editor session after importing them (edits
made in the session that imported a mesh do not stick).

Sources: SourceArt/Audio (first-party), SourceArt/ThirdParty (CC0, see ledger.json),
SourceArt/Fighters/<id>/<id>.fbx (built by Tools/blender/build_fighter.py). Everything lands
under /Game/Slop and is overwritten in place, so references stay valid.
"""
import json
import os
import sys

import unreal

PROJECT = unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir())
SRC = os.path.join(PROJECT, "SourceArt")
EAL = unreal.EditorAssetLibrary
MEL = unreal.MaterialEditingLibrary
TOOLS = unreal.AssetToolsHelpers.get_asset_tools()


def log(msg):
    unreal.log_warning("[Import] " + msg)


def args():
    only = os.environ.get("SLOP_IMPORT_ONLY", "").strip()
    return set(only.split(",")) if only else None


def import_files(files, dest, replace=True):
    """Imports files (absolute paths) into the content folder `dest`; returns object paths."""
    tasks = []
    for f in files:
        t = unreal.AssetImportTask()
        t.set_editor_property("filename", f)
        t.set_editor_property("destination_path", dest)
        t.set_editor_property("automated", True)
        t.set_editor_property("replace_existing", replace)
        t.set_editor_property("save", True)
        tasks.append(t)
    TOOLS.import_asset_tasks(tasks)
    # Interchange finishes asynchronously; anything changed before it is done gets overwritten.
    unreal.InterchangeManager.get_interchange_manager_scripted().wait_until_all_tasks_done(False)
    out = []
    for t in tasks:
        out.extend(t.get_editor_property("imported_object_paths"))
    return out


# ------------------------------------------------------------------------------ audio
def audio():
    orig = os.path.join(SRC, "Audio", "Original")
    files = sorted(os.path.join(orig, f) for f in os.listdir(orig) if f.endswith(".wav"))
    got = import_files(files, "/Game/Slop/Audio/Original")
    log(f"original sounds: {len(got)} of {len(files)}")
    kenney = os.path.join(SRC, "ThirdParty", "Kenney")
    for pack, sub in [("impact-sounds", "Impact"), ("interface-sounds", "Interface"), ("rpg-audio", "RPG")]:
        root = os.path.join(kenney, pack)
        oggs = []
        for d, _, fs in os.walk(root):
            oggs += [os.path.join(d, f) for f in fs if f.endswith(".ogg")]
        got = import_files(sorted(oggs), "/Game/Slop/Audio/Kenney/" + sub)
        log(f"kenney {pack}: {len(got)} of {len(oggs)}")
    # Music loops forever; effects never.
    for p in EAL.list_assets("/Game/Slop/Audio", recursive=True, include_folder=False):
        a = EAL.load_asset(p)
        if isinstance(a, unreal.SoundWave):
            a.set_editor_property("looping", os.path.basename(p).split(".")[0].startswith("music_"))
            EAL.save_loaded_asset(a)


# ------------------------------------------------------------------------------ materials
def fresh_material(path):
    """The material at `path`, emptied for rebuilding; kept in place so instances stay parented."""
    if EAL.does_asset_exist(path):
        m = EAL.load_asset(path)
        MEL.delete_all_material_expressions(m)
        return m
    d, n = path.rsplit("/", 1)
    return TOOLS.create_asset(n, d, unreal.Material, unreal.MaterialFactoryNew())


def link(a, a_out, b, b_in):
    """Connects two expressions; the editor returns False (it does not raise) for a wrong pin name."""
    if not MEL.connect_material_expressions(a, a_out, b, b_in):
        raise RuntimeError(f"cannot link {a.get_class().get_name()}.{a_out!r} -> {b.get_class().get_name()}.{b_in!r}")


def output(a, a_out, prop):
    if not MEL.connect_material_property(a, a_out, prop):
        raise RuntimeError(f"cannot connect {a.get_class().get_name()}.{a_out!r} to {prop}")


def expr(m, cls, x, y):
    return MEL.create_material_expression(m, cls, x, y)


def scalar(m, name, default, x, y):
    e = expr(m, unreal.MaterialExpressionScalarParameter, x, y)
    e.set_editor_property("parameter_name", name)
    e.set_editor_property("default_value", default)
    return e


def vector(m, name, default, x, y):
    e = expr(m, unreal.MaterialExpressionVectorParameter, x, y)
    e.set_editor_property("parameter_name", name)
    e.set_editor_property("default_value", unreal.LinearColor(*default))
    return e


def texparam(m, name, default_path, x, y, sampler=None):
    e = expr(m, unreal.MaterialExpressionTextureSampleParameter2D, x, y)
    e.set_editor_property("parameter_name", name)
    e.set_editor_property("texture", EAL.load_asset(default_path))
    if sampler is not None:
        e.set_editor_property("sampler_type", sampler)
    return e


def mul(m, a, b, x, y, ao="", bo=""):
    e = expr(m, unreal.MaterialExpressionMultiply, x, y)
    link(a, ao, e, "A")
    link(b, bo, e, "B")
    return e


def add(m, a, b, x, y, ao="", bo=""):
    e = expr(m, unreal.MaterialExpressionAdd, x, y)
    link(a, ao, e, "A")
    link(b, bo, e, "B")
    return e


def lerp(m, a, b, alpha, x, y, ao="", bo="", alo=""):
    e = expr(m, unreal.MaterialExpressionLinearInterpolate, x, y)
    link(a, ao, e, "A")
    link(b, bo, e, "B")
    link(alpha, alo, e, "Alpha")
    return e


WHITE = "/Engine/EngineResources/WhiteSquareTexture"
WHITE_LINEAR = "/Game/Slop/Materials/T_WhiteLinear"


def white_linear():
    """A small white texture stored linear (masks), the default for ORM samplers."""
    import struct, tempfile, zlib
    raw = b"".join(b"\x00" + b"\xff\xff\xff" * 4 for _ in range(4))
    def chunk(t, d):
        return struct.pack(">I", len(d)) + t + d + struct.pack(">I", zlib.crc32(t + d) & 0xffffffff)
    png = b"\x89PNG\r\n\x1a\n" + chunk(b"IHDR", struct.pack(">IIBBBBB", 4, 4, 8, 2, 0, 0, 0)) + chunk(b"IDAT", zlib.compress(raw)) + chunk(b"IEND", b"")
    d = tempfile.mkdtemp()
    path = os.path.join(d, "T_WhiteLinear.png")
    with open(path, "wb") as f:
        f.write(png)
    t = import_as(path, "/Game/Slop/Materials", "T_WhiteLinear")
    t.set_editor_property("compression_settings", unreal.TextureCompressionSettings.TC_MASKS)
    t.set_editor_property("srgb", False)
    EAL.save_loaded_asset(t)
FLAT_NORMAL = "/Engine/EngineMaterials/DefaultNormal"
BLACK = "/Engine/EngineResources/Black"
GREY = "/Engine/EngineResources/DefaultTexture"


def const(m, v, x, y):
    e = expr(m, unreal.MaterialExpressionConstant, x, y)
    e.set_editor_property("r", v)
    return e


def mask(m, src, chans, x, y, so=""):
    e = expr(m, unreal.MaterialExpressionComponentMask, x, y)
    for ch in "rgba":
        e.set_editor_property(ch, ch in chans)
    link(src, so, e, "")
    return e


def sat(m, a, x, y):
    e = expr(m, unreal.MaterialExpressionSaturate, x, y)
    link(a, "", e, "")
    return e


def sub(m, a, b, x, y):
    e = expr(m, unreal.MaterialExpressionSubtract, x, y)
    link(a, "", e, "A")
    link(b, "", e, "B")
    return e


def noise(m, src, scale, levels, x, y):
    """Noise from 0 to 1. Its octaves can sum past the range, so it is clamped: a mask thresholded
    at 1 must show nothing."""
    e = expr(m, unreal.MaterialExpressionNoise, x, y)
    e.set_editor_property("scale", scale)
    e.set_editor_property("quality", 1)
    e.set_editor_property("levels", levels)
    e.set_editor_property("output_min", 0.0)
    e.set_editor_property("output_max", 1.0)
    link(src, "", e, "")
    return sat(m, e, x + 120, y)


def skin_damage(m, col, pre):
    """The marks a fight leaves on skin, region by region. The fighter sets Damage (R head, G body,
    B legs: 0 fresh to 1 beaten; A how much blood shows, from the gore setting) and NeckZ, WaistZ,
    HeadRadius and BodyRadius (rest-pose centimetres) to split the body; the arms, out past
    BodyRadius in the rest pose, take none. Damage first flushes the skin red, then raises soft
    bruises in patches (noise in rest-pose space, so they ride with the body and ignore UV seams)
    that spread to about a quarter of a region; a beaten head and torso bleed in thin streaks that
    run downward. Returns the marked colour and the blood mask (for roughness)."""
    x0, y0 = -3400, -1700
    z = mask(m, pre, "b", x0, y0)
    xy = mask(m, pre, "rg", x0, y0 + 80)
    d2 = expr(m, unreal.MaterialExpressionDotProduct, x0 + 150, y0 + 80)
    link(xy, "", d2, "A")
    link(xy, "", d2, "B")
    r = expr(m, unreal.MaterialExpressionSquareRoot, x0 + 300, y0 + 80)
    link(d2, "", r, "")
    neck = scalar(m, "NeckZ", 150.0, x0, y0 + 180)
    waist = scalar(m, "WaistZ", 95.0, x0, y0 + 260)
    head_r = scalar(m, "HeadRadius", 13.0, x0, y0 + 340)
    body_r = scalar(m, "BodyRadius", 27.0, x0, y0 + 400)
    # Soft edges of about three centimetres at the neck and around the head, ten at the waist.
    above = sat(m, mul(m, sub(m, z, neck, x0 + 300, y0 + 180), const(m, 0.33, x0 + 300, y0 + 240), x0 + 450, y0 + 180), x0 + 600, y0 + 180)
    inside = sat(m, mul(m, sub(m, head_r, r, x0 + 450, y0 + 80), const(m, 0.33, x0 + 450, y0 + 140), x0 + 600, y0 + 80), x0 + 750, y0 + 80)
    head = mul(m, above, inside, x0 + 900, y0 + 120)
    legs = sat(m, mul(m, sub(m, waist, z, x0 + 300, y0 + 300), const(m, 0.1, x0 + 300, y0 + 360), x0 + 450, y0 + 300), x0 + 600, y0 + 300)
    body = sat(m, sub(m, sub(m, const(m, 1.0, x0 + 750, y0 + 200), head, x0 + 900, y0 + 220), legs, x0 + 1050, y0 + 240), x0 + 1200, y0 + 240)
    # The torso and legs fade out over five centimetres past BodyRadius (the arms).
    trunk = sat(m, mul(m, sub(m, body_r, r, x0 + 450, y0 + 420), const(m, 0.2, x0 + 450, y0 + 480), x0 + 600, y0 + 420), x0 + 750, y0 + 420)
    body = mul(m, body, trunk, x0 + 1300, y0 + 300)
    legs = mul(m, legs, trunk, x0 + 750, y0 + 340)
    dmg = vector(m, "Damage", (0, 0, 0, 0), x0, y0 + 460)
    k = add(m, add(m, mul(m, head, dmg, x0 + 1050, y0 + 400, "", "R"), mul(m, body, dmg, x0 + 1050, y0 + 480, "", "G"), x0 + 1200, y0 + 440),
            mul(m, legs, dmg, x0 + 1050, y0 + 560, "", "B"), x0 + 1350, y0 + 480)
    # A flush of red over the struck region, up to a third of the way.
    flush_col = expr(m, unreal.MaterialExpressionConstant3Vector, x0 + 1350, y0 + 600)
    flush_col.set_editor_property("constant", unreal.LinearColor(1.0, 0.8, 0.78, 1.0))
    flushed = mul(m, col, lerp(m, const(m, 1.0, x0 + 1500, y0 + 560), flush_col, mul(m, k, const(m, 0.35, x0 + 1500, y0 + 700), x0 + 1650, y0 + 680), x0 + 1800, y0 + 600), x0 + 1950, y0 + 520)
    # Bruises: soft patches of about fifteen centimetres over a threshold that falls as damage
    # grows, to about a quarter of the region when it is fully beaten.
    n1 = noise(m, pre, 0.065, 2, x0 + 1350, y0 + 760)
    t1 = sub(m, const(m, 1.0, x0 + 1500, y0 + 820), mul(m, k, const(m, 0.42, x0 + 1500, y0 + 880), x0 + 1650, y0 + 860), x0 + 1800, y0 + 840)
    bruise = sat(m, mul(m, sub(m, n1, t1, x0 + 1950, y0 + 780), const(m, 2.5, x0 + 1950, y0 + 860), x0 + 2100, y0 + 800), x0 + 2250, y0 + 800)
    bruise_col = expr(m, unreal.MaterialExpressionConstant3Vector, x0 + 2100, y0 + 900)
    bruise_col.set_editor_property("constant", unreal.LinearColor(0.62, 0.42, 0.48, 1.0))
    bruised = mul(m, flushed, lerp(m, const(m, 1.0, x0 + 2250, y0 + 900), bruise_col, mul(m, bruise, const(m, 0.6, x0 + 2250, y0 + 980), x0 + 2400, y0 + 960), x0 + 2550, y0 + 900), x0 + 2700, y0 + 700)
    # Blood: thin vertical streaks (noise squashed upright) on a beaten head, half as much on the
    # torso, none on the legs; it starts once a region is about a third of the way gone.
    stretch = expr(m, unreal.MaterialExpressionConstant3Vector, x0 + 1350, y0 + 1060)
    stretch.set_editor_property("constant", unreal.LinearColor(1.0, 1.0, 0.25, 1.0))
    n2 = noise(m, mul(m, pre, stretch, x0 + 1500, y0 + 1040), 0.3, 2, x0 + 1650, y0 + 1040)
    where = add(m, head, mul(m, body, const(m, 0.5, x0 + 1500, y0 + 1180), x0 + 1650, y0 + 1160), x0 + 1800, y0 + 1140)
    grown = sat(m, sub(m, mul(m, k, const(m, 1.4, x0 + 1500, y0 + 1260), x0 + 1650, y0 + 1240), const(m, 0.45, x0 + 1650, y0 + 1300), x0 + 1800, y0 + 1240), x0 + 1950, y0 + 1240)
    bk = mul(m, mul(m, mul(m, where, grown, x0 + 2100, y0 + 1180), dmg, x0 + 2250, y0 + 1200, "", "A"), const(m, 0.4, x0 + 2250, y0 + 1280), x0 + 2400, y0 + 1220)
    t2 = sub(m, const(m, 1.0, x0 + 2400, y0 + 1100), bk, x0 + 2550, y0 + 1120)
    blood = sat(m, mul(m, sub(m, n2, t2, x0 + 2700, y0 + 1060), const(m, 8.0, x0 + 2700, y0 + 1140), x0 + 2850, y0 + 1080), x0 + 3000, y0 + 1080)
    blood_col = expr(m, unreal.MaterialExpressionConstant3Vector, x0 + 2850, y0 + 1180)
    blood_col.set_editor_property("constant", unreal.LinearColor(0.16, 0.012, 0.01, 1.0))
    marked = lerp(m, bruised, blood_col, mul(m, blood, const(m, 0.9, x0 + 3000, y0 + 1180), x0 + 3150, y0 + 1160), x0 + 3300, y0 + 800)
    return marked, blood


def character_material(path="/Game/Slop/Materials/M_Character", skin=False):
    """M_Character: base colour x tint, normal, ORM; the fighter's accent rim; a hit flash; a tint
    toward a colour (the Reflection, ash, salt); a dissolve with a glowing edge (finishers).
    Two realism controls, off by default: ClothDetail (folds, wear and dirt from noise in the
    mesh's rest-pose space, so the pattern rides with the fabric and ignores UV seams) and
    HairRoughness / HairSpecDrop (a matte floor and less specular on the hair half of a hair
    atlas, which otherwise reads as a glossy shell).
    `skin` builds M_CharacterSkin: the same graph with subsurface scattering (light through skin,
    the engine's default profile) and Sweat, which wets the skin (smoother, a little more specular),
    and the damage a fight leaves (see skin_damage).
    Sway (0 off; cloth slots) moves loose fabric with the body's motion: the fighter sets SwayVec
    (world cm, from a spring on how the body moves) and SwayTop/SwaySpan (cm), and each vertex is
    offset by SwayVec x Sway x the square of how far below SwayTop it hangs in the rest pose, so a
    coat's tails and a gown's hem swing while the waist stays put."""
    m = fresh_material(path)
    if skin:
        m.set_editor_property("shading_model", unreal.MaterialShadingModel.MSM_SUBSURFACE_PROFILE)
    m.set_editor_property("blend_mode", unreal.BlendMode.BLEND_MASKED)
    m.set_editor_property("used_with_skeletal_mesh", True)
    base = texparam(m, "BaseColorTex", WHITE, -1400, -400)
    tint = vector(m, "Tint", (1, 1, 1, 1), -1400, -150)
    col = mul(m, base, tint, -1100, -300, "RGB", "")
    towards = vector(m, "TintColor", (0.05, 0.08, 0.05, 1), -1100, -100)
    amount = scalar(m, "TintAmount", 0.0, -1100, 0)
    # The rest-pose position exists only in the vertex shader; an interpolator carries it over.
    pre_v = expr(m, unreal.MaterialExpressionPreSkinnedPosition, -2500, -700)
    pre = expr(m, unreal.MaterialExpressionVertexInterpolator, -2300, -700)
    link(pre_v, "", pre, "")
    blood = None
    if skin:
        col, blood = skin_damage(m, col, pre)
    col2 = lerp(m, col, towards, amount, -850, -200)
    cloth_k = scalar(m, "ClothDetail", 0.0, -2300, -560)
    wear = expr(m, unreal.MaterialExpressionNoise, -2050, -700)
    wear.set_editor_property("scale", 0.025)
    wear.set_editor_property("quality", 1)
    wear.set_editor_property("output_min", 0.0)
    wear.set_editor_property("output_max", 1.0)
    link(pre, "", wear, "")
    wear_col = lerp(m, const(m, 0.78, -1850, -760), const(m, 1.08, -1850, -700), wear, -1650, -720)
    wear_k = lerp(m, const(m, 1.0, -1650, -820), wear_col, cloth_k, -1450, -760)
    col3 = mul(m, col2, wear_k, -650, -260)
    output(col3, "", unreal.MaterialProperty.MP_BASE_COLOR)
    normal = texparam(m, "NormalTex", FLAT_NORMAL, -1400, 150, unreal.MaterialSamplerType.SAMPLERTYPE_NORMAL)
    folds = expr(m, unreal.MaterialExpressionVectorNoise, -1850, 100)
    folds.set_editor_property("noise_function", unreal.VectorNoiseFunction.VNF_GRADIENT_ALU)
    link(mul(m, pre, const(m, 0.08, -2050, 40), -2050, 100), "", folds, "")
    folds3 = expr(m, unreal.MaterialExpressionComponentMask, -1700, 40)
    for ch, on in (("r", True), ("g", True), ("b", True), ("a", False)):
        folds3.set_editor_property(ch, on)
    link(folds, "", folds3, "")
    bump = mul(m, mul(m, folds3, cloth_k, -1650, 100), const(m, 0.3, -1650, 180), -1450, 120)
    nsum = add(m, normal, bump, -1250, 150, "RGB", "")
    nrm = expr(m, unreal.MaterialExpressionNormalize, -1050, 150)
    link(nsum, "", nrm, "")
    output(nrm, "", unreal.MaterialProperty.MP_NORMAL)
    orm = texparam(m, "ORMTex", WHITE_LINEAR, -1400, 450, unreal.MaterialSamplerType.SAMPLERTYPE_MASKS)
    rough_k = scalar(m, "Roughness", 0.6, -1100, 400)
    metal_k = scalar(m, "Metallic", 0.0, -1100, 520)
    rough = mul(m, orm, rough_k, -850, 400, "G", "")
    gloss = scalar(m, "TintGloss", 0.0, -1100, 300)
    rough2 = lerp(m, rough, gloss, amount, -650, 400)
    one = expr(m, unreal.MaterialExpressionConstant, -850, 300)
    one.set_editor_property("r", 0.08)
    link(one, "", rough2, "B")
    # Cloth wear roughens and smooths the fabric in patches.
    wear_r = mul(m, mul(m, add(m, wear, const(m, -0.5, -850, 640), -700, 620), cloth_k, -550, 620), const(m, 0.2, -550, 700), -400, 640)
    rough3 = add(m, rough2, wear_r, -350, 420)
    # The hair half of a hair atlas (texture v below one half) gets a matte floor.
    tc = expr(m, unreal.MaterialExpressionTextureCoordinate, -1400, 760)
    v = expr(m, unreal.MaterialExpressionComponentMask, -1250, 760)
    v.set_editor_property("r", False)
    v.set_editor_property("g", True)
    link(tc, "", v, "")
    below = expr(m, unreal.MaterialExpressionSubtract, -1100, 760)
    link(const(m, 0.5, -1250, 820), "", below, "A")
    link(v, "", below, "B")
    hair = expr(m, unreal.MaterialExpressionSaturate, -800, 760)
    link(mul(m, below, const(m, 200.0, -950, 820), -950, 760), "", hair, "")
    floor_ = mul(m, hair, scalar(m, "HairRoughness", 0.0, -800, 850), -650, 780)
    rough4 = expr(m, unreal.MaterialExpressionMax, -200, 420)
    link(rough3, "", rough4, "A")
    link(floor_, "", rough4, "B")
    spec = expr(m, unreal.MaterialExpressionSubtract, -200, 560)
    link(const(m, 0.5, -350, 560), "", spec, "A")
    link(mul(m, hair, scalar(m, "HairSpecDrop", 0.0, -500, 900), -350, 900), "", spec, "B")
    if skin:
        sweat = scalar(m, "Sweat", 0.0, -500, 1000)
        dry = expr(m, unreal.MaterialExpressionOneMinus, -350, 1000)
        link(mul(m, sweat, const(m, 0.5, -500, 1060), -400, 1000), "", dry, "")
        rough4 = mul(m, rough4, dry, -100, 460)
        spec = add(m, spec, mul(m, sweat, const(m, 0.15, -350, 1100), -250, 1060), -100, 600)
        # Blood is wet: smooth and a little brighter in the highlight.
        rough4 = lerp(m, rough4, const(m, 0.18, -100, 380), blood, 0, 460)
        spec = add(m, spec, mul(m, blood, const(m, 0.12, -250, 1160), -100, 1140), 0, 600)
    output(rough4, "", unreal.MaterialProperty.MP_ROUGHNESS)
    output(spec, "", unreal.MaterialProperty.MP_SPECULAR)
    metal = mul(m, orm, metal_k, -850, 520, "B", "")
    output(metal, "", unreal.MaterialProperty.MP_METALLIC)
    output(orm, "R", unreal.MaterialProperty.MP_AMBIENT_OCCLUSION)
    # Emissive: accent rim (fresnel) + flash + dissolve edge + self glow (eyes, embers).
    fres = expr(m, unreal.MaterialExpressionFresnel, -1100, 700)
    fres.set_editor_property("exponent", 3.0)
    accent = vector(m, "AccentColor", (1, 1, 1, 1), -1100, 820)
    accent_k = scalar(m, "AccentStrength", 0.24, -1100, 940)
    rim = mul(m, mul(m, fres, accent, -850, 760), accent_k, -650, 800)
    flash = scalar(m, "Flash", 0.0, -1100, 1060)
    flash_col = vector(m, "FlashColor", (1, 0.95, 0.85, 1), -1100, 1160)
    fl = mul(m, flash_col, flash, -850, 1100)
    glow = scalar(m, "Glow", 0.0, -1100, 1280)
    glow_e = mul(m, col, glow, -850, 1260)
    # Painted self-light (eyes, embers, runes): an emissive map x strength.
    em_tex = texparam(m, "EmissiveTex", BLACK, -1400, 1380)
    em_k = scalar(m, "EmissiveStrength", 1.0, -1100, 1400)
    painted = mul(m, em_tex, em_k, -850, 1380, "RGB", "")
    em = add(m, add(m, add(m, rim, fl, -450, 900), glow_e, -250, 1000), painted, -150, 1100)
    # Dissolve: world-space noise against a threshold; the band just above it glows. The mask is
    # noise + 0.5 - 1.5 x dissolve against a clip of 0.5, so nothing is cut at rest and everything
    # is gone at 1; the base colour's alpha (lashes, brows, hair cards) cuts at one half.
    noise = expr(m, unreal.MaterialExpressionNoise, -1400, 1500)
    noise.set_editor_property("scale", 0.06)
    noise.set_editor_property("quality", 1)
    noise.set_editor_property("output_min", 0.0)
    noise.set_editor_property("output_max", 1.0)
    dissolve = scalar(m, "Dissolve", 0.0, -1400, 1700)
    diff = expr(m, unreal.MaterialExpressionSubtract, -1100, 1550)
    link(noise, "", diff, "A")
    link(dissolve, "", diff, "B")
    half = expr(m, unreal.MaterialExpressionConstant, -1250, 1620)
    half.set_editor_property("r", 0.5)
    k15 = expr(m, unreal.MaterialExpressionConstant, -1250, 1680)
    k15.set_editor_property("r", 1.5)
    mask = expr(m, unreal.MaterialExpressionSubtract, -900, 1500)
    link(add(m, noise, half, -1050, 1480), "", mask, "A")
    link(mul(m, dissolve, k15, -1050, 1620), "", mask, "B")
    cut = expr(m, unreal.MaterialExpressionMin, -700, 1500)
    link(mask, "", cut, "A")
    link(base, "A", cut, "B")
    output(cut, "", unreal.MaterialProperty.MP_OPACITY_MASK)
    m.set_editor_property("opacity_mask_clip_value", 0.5)
    edge = expr(m, unreal.MaterialExpressionOneMinus, -850, 1650)
    sat = expr(m, unreal.MaterialExpressionSaturate, -1000, 1650)
    band = mul(m, diff, scalar(m, "EdgeWidth", 14.0, -1100, 1800), -1050, 1700)
    link(band, "", sat, "")
    link(sat, "", edge, "")
    on = expr(m, unreal.MaterialExpressionCeil, -850, 1780)
    link(dissolve, "", on, "")
    edge_col = vector(m, "DissolveColor", (1, 0.45, 0.1, 1), -850, 1880)
    e1 = mul(m, mul(m, edge, on, -650, 1700), edge_col, -450, 1750)
    e2 = mul(m, e1, scalar(m, "EdgeGlow", 8.0, -650, 1880), -300, 1800)
    em2 = add(m, em, e2, -100, 1200)
    output(em2, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    if not skin:
        # Loose fabric swings with the body (see the docstring); the rest-pose height is used
        # directly, as world position offset runs in the vertex shader.
        z = expr(m, unreal.MaterialExpressionComponentMask, -2300, -1000)
        for ch, on in (("r", False), ("g", False), ("b", True), ("a", False)):
            z.set_editor_property(ch, on)
        link(pre_v, "", z, "")
        below_top = expr(m, unreal.MaterialExpressionSubtract, -2100, -1000)
        link(scalar(m, "SwayTop", 95.0, -2300, -900), "", below_top, "A")
        link(z, "", below_top, "B")
        hang = expr(m, unreal.MaterialExpressionDivide, -1900, -1000)
        link(below_top, "", hang, "A")
        link(scalar(m, "SwaySpan", 70.0, -2100, -880), "", hang, "B")
        hang01 = expr(m, unreal.MaterialExpressionSaturate, -1750, -1000)
        link(hang, "", hang01, "")
        w = mul(m, mul(m, hang01, hang01, -1600, -1000), scalar(m, "Sway", 0.0, -1750, -880), -1450, -980)
        sway = vector(m, "SwayVec", (0, 0, 0, 0), -1600, -1120)
        sway3 = expr(m, unreal.MaterialExpressionComponentMask, -1450, -1120)
        for ch, on in (("r", True), ("g", True), ("b", True), ("a", False)):
            sway3.set_editor_property(ch, on)
        link(sway, "", sway3, "")
        output(mul(m, sway3, w, -1250, -1050), "", unreal.MaterialProperty.MP_WORLD_POSITION_OFFSET)
    MEL.recompile_material(m)
    EAL.save_loaded_asset(m)
    log(path.rsplit("/", 1)[1] + " built")


def surface_material():
    """M_Surface: tiled stage surfaces (colour, normal, ORM) with a tint, world-aligned option off."""
    m = fresh_material("/Game/Slop/Materials/M_Surface")
    tc = expr(m, unreal.MaterialExpressionTextureCoordinate, -1600, 0)
    tiling = scalar(m, "Tiling", 1.0, -1600, 150)
    uv = mul(m, tc, tiling, -1400, 50)
    def sample(name, default, y, sampler=None):
        e = texparam(m, name, default, -1150, y, sampler)
        link(uv, "", e, "UVs")
        return e
    base = sample("BaseColorTex", WHITE, -300)
    tint = vector(m, "Tint", (1, 1, 1, 1), -1150, -60)
    col = mul(m, base, tint, -850, -200, "RGB", "")
    output(col, "", unreal.MaterialProperty.MP_BASE_COLOR)
    nrm = sample("NormalTex", FLAT_NORMAL, 100, unreal.MaterialSamplerType.SAMPLERTYPE_NORMAL)
    flat = expr(m, unreal.MaterialExpressionConstant3Vector, -850, 250)
    flat.set_editor_property("constant", unreal.LinearColor(0, 0, 1, 1))
    nk = scalar(m, "NormalStrength", 1.0, -850, 330)
    nrm2 = lerp(m, flat, nrm, nk, -600, 150, "", "RGB", "")
    output(nrm2, "", unreal.MaterialProperty.MP_NORMAL)
    orm = sample("ORMTex", WHITE_LINEAR, 450, unreal.MaterialSamplerType.SAMPLERTYPE_MASKS)
    rk = scalar(m, "Roughness", 1.0, -850, 450)
    output(mul(m, orm, rk, -600, 450, "G", ""), "", unreal.MaterialProperty.MP_ROUGHNESS)
    mk = scalar(m, "Metallic", 0.0, -850, 570)
    output(mul(m, orm, mk, -600, 570, "B", ""), "", unreal.MaterialProperty.MP_METALLIC)
    output(orm, "R", unreal.MaterialProperty.MP_AMBIENT_OCCLUSION)
    em_col = vector(m, "EmissiveColor", (0, 0, 0, 1), -850, 700)
    em_k = scalar(m, "Emissive", 0.0, -850, 820)
    output(mul(m, mul(m, em_col, em_k, -600, 750), base, -400, 750, "", "RGB"), "", unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    MEL.recompile_material(m)
    EAL.save_loaded_asset(m)
    log("M_Surface built")


def glow_material(path, blend, two_sided=True):
    """Unlit, coloured, optionally additive: effects, lantern glass, slop, lightning."""
    m = fresh_material(path)
    m.set_editor_property("shading_model", unreal.MaterialShadingModel.MSM_UNLIT)
    m.set_editor_property("blend_mode", blend)
    m.set_editor_property("two_sided", two_sided)
    m.set_editor_property("used_with_instanced_static_meshes", True)
    col = vector(m, "Color", (1, 1, 1, 1), -800, 0)
    k = scalar(m, "Intensity", 1.0, -800, 200)
    vc = expr(m, unreal.MaterialExpressionVertexColor, -800, 350)
    e = mul(m, mul(m, col, k, -500, 100), vc, -300, 200, "", "")
    output(e, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    if blend != unreal.BlendMode.BLEND_OPAQUE:
        op = scalar(m, "Opacity", 1.0, -800, 500)
        output(mul(m, op, vc, -500, 500, "", "A"), "", unreal.MaterialProperty.MP_OPACITY)
    MEL.recompile_material(m)
    EAL.save_loaded_asset(m)


def billboard_material(path, ring, blend=unreal.BlendMode.BLEND_ADDITIVE):
    """Camera-facing quads: a soft disc (flashes, puffs) or a thin ring (impact rings), shaped
    from the quad's UVs; colour x intensity x a per-instance fade. Additive by default; the
    translucent variant (smoke, mist) puts the shape and fade in the opacity instead."""
    m = fresh_material(path)
    m.set_editor_property("shading_model", unreal.MaterialShadingModel.MSM_UNLIT)
    m.set_editor_property("blend_mode", blend)
    m.set_editor_property("two_sided", True)
    m.set_editor_property("used_with_instanced_static_meshes", True)
    tc = expr(m, unreal.MaterialExpressionTextureCoordinate, -1400, 0)
    c = expr(m, unreal.MaterialExpressionConstant2Vector, -1400, 150)
    c.set_editor_property("r", 0.5)
    c.set_editor_property("g", 0.5)
    dist = expr(m, unreal.MaterialExpressionDistance, -1150, 50)
    link(tc, "", dist, "A")
    link(c, "", dist, "B")
    two = expr(m, unreal.MaterialExpressionConstant, -1150, 200)
    two.set_editor_property("r", 2.0)
    r = mul(m, dist, two, -950, 100)
    if ring:
        # |r - 0.8| narrow band.
        k = expr(m, unreal.MaterialExpressionConstant, -950, 250)
        k.set_editor_property("r", 0.8)
        sub = expr(m, unreal.MaterialExpressionSubtract, -800, 150)
        link(r, "", sub, "A")
        link(k, "", sub, "B")
        ab = expr(m, unreal.MaterialExpressionAbs, -650, 150)
        link(sub, "", ab, "")
        w = scalar(m, "Width", 8.0, -650, 300)
        band = mul(m, ab, w, -500, 200)
        one_m = expr(m, unreal.MaterialExpressionOneMinus, -350, 200)
        link(band, "", one_m, "")
        shape_in = one_m
    else:
        one_m = expr(m, unreal.MaterialExpressionOneMinus, -800, 150)
        link(r, "", one_m, "")
        shape_in = one_m
    sat = expr(m, unreal.MaterialExpressionSaturate, -200, 200)
    link(shape_in, "", sat, "")
    pw = expr(m, unreal.MaterialExpressionPower, -50, 200)
    link(sat, "", pw, "Base")
    ex = scalar(m, "Sharpness", 2.0, -200, 350)
    link(ex, "", pw, "Exp")
    col = vector(m, "Color", (1, 1, 1, 1), -200, -150)
    k2 = scalar(m, "Intensity", 2.0, -200, -50)
    # Per-instance fade: custom data 0 (1 when unused).
    fade = expr(m, unreal.MaterialExpressionPerInstanceCustomData, -200, 500)
    fade.set_editor_property("data_index", 0)
    fade.set_editor_property("const_default_value", 1.0)
    if blend == unreal.BlendMode.BLEND_ADDITIVE:
        e = mul(m, mul(m, mul(m, col, k2, 50, -100), pw, 250, 0), fade, 450, 100)
        output(e, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    else:
        output(mul(m, col, k2, 50, -100), "", unreal.MaterialProperty.MP_EMISSIVE_COLOR)
        op = mul(m, mul(m, pw, fade, 250, 200), scalar(m, "Opacity", 1.0, 50, 350), 450, 250)
        # Softened where the quad meets the floor or a wall.
        df = expr(m, unreal.MaterialExpressionDepthFade, 450, 400)
        link(op, "", df, "Opacity")
        link(scalar(m, "FadeDistance", 80.0, 250, 450), "", df, "FadeDistance")
        output(df, "", unreal.MaterialProperty.MP_OPACITY)
    MEL.recompile_material(m)
    EAL.save_loaded_asset(m)


def stage_glow_material():
    """M_StageGlow: lamps, slop, glass and fire in the sets: colour x intensity, broken up by an
    animated noise (Pattern 0..1 at PatternScale per metre) or by horizontal paper ribs
    (Ribs 0/1 every RibSpacing metres)."""
    m = fresh_material("/Game/Slop/Materials/M_StageGlow")
    m.set_editor_property("shading_model", unreal.MaterialShadingModel.MSM_UNLIT)
    m.set_editor_property("two_sided", True)
    col = mul(m, vector(m, "Color", (1, 1, 1, 1), -900, -200), scalar(m, "Intensity", 1.0, -900, -80), -650, -150)
    # Noise: world position drifting slowly upward through 3D noise reads as churning.
    wp = expr(m, unreal.MaterialExpressionWorldPosition, -1600, 100)
    t = expr(m, unreal.MaterialExpressionTime, -1600, 250)
    drift = mul(m, t, scalar(m, "PatternSpeed", 25.0, -1600, 350), -1400, 300)
    up = expr(m, unreal.MaterialExpressionAppendVector, -1250, 300)
    zero2 = expr(m, unreal.MaterialExpressionConstant2Vector, -1400, 420)
    link(zero2, "", up, "A")
    link(drift, "", up, "B")
    pos = add(m, wp, up, -1100, 150)
    nscale = mul(m, pos, mul(m, scalar(m, "PatternScale", 1.0, -1250, 500), scalar(m, "Centimetre", 0.01, -1250, 600), -1100, 520), -950, 200)
    noise = expr(m, unreal.MaterialExpressionNoise, -800, 200)
    noise.set_editor_property("scale", 1.0)
    noise.set_editor_property("quality", 1)
    noise.set_editor_property("levels", 3)
    noise.set_editor_property("output_min", 0.0)
    noise.set_editor_property("output_max", 1.0)
    link(nscale, "", noise, "")
    lo = expr(m, unreal.MaterialExpressionConstant, -800, 350)
    lo.set_editor_property("r", 0.2)
    hi = expr(m, unreal.MaterialExpressionConstant, -800, 420)
    hi.set_editor_property("r", 1.6)
    nf = lerp(m, lo, hi, noise, -600, 300)
    one = expr(m, unreal.MaterialExpressionConstant, -600, 450)
    one.set_editor_property("r", 1.0)
    pf = lerp(m, one, nf, scalar(m, "Pattern", 0.0, -600, 520), -400, 350)
    # Ribs: a sine along world Z.
    z = expr(m, unreal.MaterialExpressionComponentMask, -1100, 700)
    for c, on in (("r", False), ("g", False), ("b", True), ("a", False)):
        z.set_editor_property(c, on)
    link(wp, "", z, "")
    zm = mul(m, z, scalar(m, "RibCentimetre", 0.01, -1100, 800), -950, 750)
    sine = expr(m, unreal.MaterialExpressionSine, -800, 750)
    link(zm, "", sine, "")
    rib_sp = scalar(m, "RibSpacing", 0.03, -950, 870)
    # Sine's period is fixed per expression; divide by the spacing instead.
    div = expr(m, unreal.MaterialExpressionDivide, -900, 750)
    link(zm, "", div, "A")
    link(rib_sp, "", div, "B")
    link(div, "", sine, "")
    half = expr(m, unreal.MaterialExpressionConstant, -800, 900)
    half.set_editor_property("r", 0.5)
    s01 = add(m, mul(m, sine, half, -650, 800), half, -500, 820)
    rlo = expr(m, unreal.MaterialExpressionConstant, -500, 950)
    rlo.set_editor_property("r", 0.45)
    rhi = expr(m, unreal.MaterialExpressionConstant, -500, 1020)
    rhi.set_editor_property("r", 1.15)
    rf = lerp(m, rlo, rhi, s01, -350, 900)
    rk = lerp(m, one, rf, scalar(m, "Ribs", 0.0, -350, 1050), -200, 950)
    e = mul(m, mul(m, col, pf, -200, 0), rk, 0, 200)
    output(e, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    MEL.recompile_material(m)
    EAL.save_loaded_asset(m)


def water_material():
    """M_Water: still dark water, tar and puddles: near-black, glossy, so the screen-space
    reflections carry the lights; an optional faint glow (the Roost's tar falls). Broad, faint
    ripples (world-space noise of size 1/RippleScale cm drifting slowly, RippleStrength) and a
    roughness floor make the reflections waver as standing water does instead of mirroring like
    glass; strong or fine ripples read as wet cobbles instead. A glowing liquid (the Roost's slop
    lake) glows from below through churning noise (Pattern 0..1 at PatternScale per metre, with
    dark crusted patches) under the same lit, glossy surface; Intensity is what the stage pulses."""
    m = fresh_material("/Game/Slop/Materials/M_Water")
    col = vector(m, "Color", (0.02, 0.02, 0.02, 1), -600, 0)
    output(col, "", unreal.MaterialProperty.MP_BASE_COLOR)
    rough = expr(m, unreal.MaterialExpressionMax, -350, 150)
    link(scalar(m, "Roughness", 0.05, -600, 150), "", rough, "A")
    link(const(m, 0.09, -600, 200), "", rough, "B")
    output(rough, "", unreal.MaterialProperty.MP_ROUGHNESS)
    wp = expr(m, unreal.MaterialExpressionWorldPosition, -1300, 600)
    t = expr(m, unreal.MaterialExpressionTime, -1300, 700)
    drift = mul(m, t, vector(m, "RippleDrift", (0.25, 0.15, 0.3, 0), -1300, 760), -1100, 700)
    pos = add(m, mul(m, wp, scalar(m, "RippleScale", 0.012, -1300, 650), -1100, 600), drift, -950, 640)
    ripples = expr(m, unreal.MaterialExpressionVectorNoise, -800, 640)
    ripples.set_editor_property("noise_function", unreal.VectorNoiseFunction.VNF_GRADIENT_ALU)
    link(pos, "", ripples, "")
    r3 = expr(m, unreal.MaterialExpressionComponentMask, -650, 640)
    r3.set_editor_property("r", True)
    r3.set_editor_property("g", True)
    r3.set_editor_property("b", False)
    r3.set_editor_property("a", False)
    link(ripples, "", r3, "")
    tilt = mul(m, r3, scalar(m, "RippleStrength", 0.03, -650, 720), -500, 660)
    nrm = expr(m, unreal.MaterialExpressionAppendVector, -350, 660)
    link(tilt, "", nrm, "A")
    link(const(m, 1.0, -500, 760), "", nrm, "B")
    nn = expr(m, unreal.MaterialExpressionNormalize, -200, 660)
    link(nrm, "", nn, "")
    output(nn, "", unreal.MaterialProperty.MP_NORMAL)
    output(scalar(m, "Specular", 0.5, -600, 250), "", unreal.MaterialProperty.MP_SPECULAR)
    em = mul(m, vector(m, "EmissiveColor", (0, 0, 0, 1), -600, 350), scalar(m, "Emissive", 0.0, -600, 480), -350, 400)
    em = mul(m, em, scalar(m, "Intensity", 1.0, -600, 540), -200, 420)
    churn = mul(m, t, vector(m, "PatternDrift", (0.02, 0.01, 0.1, 0), -1300, 1000), -1100, 960)
    gscale = mul(m, scalar(m, "PatternScale", 1.0, -1300, 880), const(m, 0.01, -1300, 930), -1150, 900)
    gpos = add(m, mul(m, wp, gscale, -1000, 880), churn, -850, 920)
    noise = expr(m, unreal.MaterialExpressionNoise, -700, 920)
    noise.set_editor_property("scale", 1.0)
    noise.set_editor_property("quality", 1)
    noise.set_editor_property("levels", 3)
    noise.set_editor_property("output_min", 0.0)
    noise.set_editor_property("output_max", 1.0)
    link(gpos, "", noise, "")
    nf = lerp(m, const(m, 0.1, -550, 880), const(m, 1.7, -550, 940), noise, -400, 920)
    pf = lerp(m, const(m, 1.0, -400, 860), nf, scalar(m, "Pattern", 0.0, -400, 1000), -250, 900)
    output(mul(m, em, pf, -100, 500), "", unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    MEL.recompile_material(m)
    EAL.save_loaded_asset(m)


def sky_material():
    """M_Sky: an HDR sky on the inside of a dome, looked up by the view direction in the dome's
    own space (turning the dome turns the sky)."""
    m = fresh_material("/Game/Slop/Materials/M_Sky")
    m.set_editor_property("shading_model", unreal.MaterialShadingModel.MSM_UNLIT)
    m.set_editor_property("two_sided", True)
    cam = expr(m, unreal.MaterialExpressionCameraVectorWS, -1000, 0)
    neg = expr(m, unreal.MaterialExpressionConstant, -1000, 100)
    neg.set_editor_property("r", -1.0)
    view = mul(m, cam, neg, -800, 50)
    tr = expr(m, unreal.MaterialExpressionTransform, -600, 50)
    tr.set_editor_property("transform_source_type", unreal.MaterialVectorCoordTransformSource.TRANSFORMSOURCE_WORLD)
    tr.set_editor_property("transform_type", unreal.MaterialVectorCoordTransform.TRANSFORM_LOCAL)
    link(view, "", tr, "")
    cube = expr(m, unreal.MaterialExpressionTextureSampleParameterCube, -350, 0)
    cube.set_editor_property("parameter_name", "Sky")
    cube.set_editor_property("texture", EAL.load_asset("/Game/Slop/Skies/HDR_lakeside_night"))
    link(tr, "", cube, "UVs")
    e = mul(m, mul(m, cube, scalar(m, "Intensity", 1.0, -350, 250), -100, 100, "RGB", ""), vector(m, "Tint", (1, 1, 1, 1), -350, 350), 100, 150)
    output(e, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    MEL.recompile_material(m)
    EAL.save_loaded_asset(m)


def materials():
    white_linear()
    billboard_material("/Game/Slop/Materials/M_SoftDisc", False)
    billboard_material("/Game/Slop/Materials/M_Ring", True)
    billboard_material("/Game/Slop/Materials/M_Puff", False, unreal.BlendMode.BLEND_TRANSLUCENT)
    water_material()
    sky_material()
    stage_glow_material()
    character_material()
    character_material("/Game/Slop/Materials/M_CharacterSkin", skin=True)
    surface_material()
    glow_material("/Game/Slop/Materials/M_GlowAdd", unreal.BlendMode.BLEND_ADDITIVE)
    glow_material("/Game/Slop/Materials/M_GlowTranslucent", unreal.BlendMode.BLEND_TRANSLUCENT)
    glow_material("/Game/Slop/Materials/M_GlowOpaque", unreal.BlendMode.BLEND_OPAQUE, False)
    log("materials done")


# ------------------------------------------------------------------------------ fighters
def texture(path_png, dest, srgb=True, normal=False):
    got = import_files([path_png], dest)
    if not got:
        return None
    t = EAL.load_asset(got[0])
    if normal:
        t.set_editor_property("compression_settings", unreal.TextureCompressionSettings.TC_NORMALMAP)
        t.set_editor_property("srgb", False)
    elif not srgb:
        t.set_editor_property("compression_settings", unreal.TextureCompressionSettings.TC_MASKS)
        t.set_editor_property("srgb", False)
    EAL.save_loaded_asset(t)
    return t


# Realism controls of M_Character per slot (a slot name for every fighter, or <id>_<slot>).
SLOT_EXTRA = {
    "hairface": {"HairRoughness": 0.72, "HairSpecDrop": 0.2},
    "suit": {"ClothDetail": 1.0},
    "canvas": {"ClothDetail": 0.8},
    "coat": {"ClothDetail": 0.6, "Sway": 1.0},
    "cloth": {"ClothDetail": 0.6, "Sway": 0.5},
    "gown": {"ClothDetail": 0.5, "Sway": 1.0},
    "leather": {"ClothDetail": 0.3},
    "ashka_cloth": {"ClothDetail": 0.35, "Sway": 0.2},
    # The stranger in white stays clean; the robe hangs long.
    "daxion_cloth": {"ClothDetail": 0.15, "Sway": 1.0},
    "mirela_gown": {"ClothDetail": 0.25, "Sway": 1.0},
    "gorm_canvas": {"ClothDetail": 0.8, "Sway": 0.6},
}


def fighter(fid, fbx, import_meshes=True):
    dest = f"/Game/Slop/Fighters/{fid}"
    folder = os.path.dirname(fbx)
    if import_meshes:
        got = import_files([fbx], dest)
        log(f"{fid}: imported {len(got)} assets")
        return
    meshes = [EAL.load_asset(p) for p in EAL.list_assets(dest, recursive=False, include_folder=False) if isinstance(EAL.load_asset(p), unreal.SkeletalMesh)]
    if not meshes:
        log(f"{fid}: no skeletal mesh under {dest}")
        return
    mesh = meshes[0]
    # One material instance per slot, from the textures beside the FBX named after the slot.
    base = EAL.load_asset("/Game/Slop/Materials/M_Character")
    skin_base = EAL.load_asset("/Game/Slop/Materials/M_CharacterSkin") or base
    mats = mesh.get_editor_property("materials")
    for i, sm in enumerate(mats):
        slot = str(sm.get_editor_property("material_slot_name"))
        # Slots are named M_<id>_<slot>; their textures <id>_<slot>_<map>.png.
        key = slot[len(f"M_{fid}_"):] if slot.startswith(f"M_{fid}_") else slot
        prefix = f"{fid}_{key}_".lower()
        if not any(c.lower().endswith(".png") and c.lower().startswith(prefix) for c in os.listdir(folder)):
            # No textures of ours for this slot: keep what the FBX brought.
            continue
        mi_path = f"{dest}/MI_{fid}_{key}"
        if EAL.does_asset_exist(mi_path):
            mi = EAL.load_asset(mi_path)
        else:
            mi = TOOLS.create_asset(f"MI_{fid}_{key}", dest, unreal.MaterialInstanceConstant, unreal.MaterialInstanceConstantFactoryNew())
        unreal.MaterialEditingLibrary.set_material_instance_parent(mi, skin_base if key == "skin" else base)
        for suffix, param, kind in [("basecolor", "BaseColorTex", "srgb"), ("normal", "NormalTex", "normal"), ("orm", "ORMTex", "linear"), ("emissive", "EmissiveTex", "srgb")]:
            png = os.path.join(folder, f"{fid}_{key}_{suffix}.png")
            if not os.path.exists(png):
                png = None
            if png:
                t = texture(png, dest + "/Textures", srgb=(kind == "srgb"), normal=(kind == "normal"))
                if t:
                    unreal.MaterialEditingLibrary.set_material_instance_texture_parameter_value(mi, param, t)
        # The maps carry the values: roughness in G, metalness in B, taken as they are.
        MEL.set_material_instance_scalar_parameter_value(mi, "Roughness", 1.0)
        MEL.set_material_instance_scalar_parameter_value(mi, "Metallic", 1.0)
        for param, value in {**SLOT_EXTRA.get(key, {}), **SLOT_EXTRA.get(f"{fid}_{key}", {})}.items():
            MEL.set_material_instance_scalar_parameter_value(mi, param, value)
        EAL.save_loaded_asset(mi)
        sm.set_editor_property("material_interface", mi)
        mats[i] = sm
    mesh.set_editor_property("materials", mats)
    EAL.save_loaded_asset(mesh, False)
    ours = all(str(m.get_editor_property("material_interface").get_name()).startswith("MI_") for m in mats if m.get_editor_property("material_interface"))
    if ours:
        # The importer's own materials and its copies of the textures are no longer used.
        for p in EAL.list_assets(dest, recursive=False, include_folder=False):
            a = EAL.load_asset(p)
            if (isinstance(a, unreal.MaterialInterface) and not a.get_name().startswith("MI_")) or isinstance(a, unreal.Texture):
                EAL.delete_asset(p.split(".")[0])
    log(f"{fid}: {mesh.get_path_name()} with {len(mats)} material slots")


def fighters(import_meshes=True):
    root = os.path.join(SRC, "Fighters")
    if not os.path.isdir(root):
        log("no SourceArt/Fighters yet")
        return
    for fid in sorted(os.listdir(root)):
        fbx = os.path.join(root, fid, fid + ".fbx")
        if os.path.exists(fbx):
            fighter(fid, fbx, import_meshes)


# ------------------------------------------------------------------------------ stages
def import_as(src, dest_dir, name):
    t = unreal.AssetImportTask()
    t.set_editor_property("filename", src)
    t.set_editor_property("destination_path", dest_dir)
    t.set_editor_property("destination_name", name)
    t.set_editor_property("automated", True)
    t.set_editor_property("replace_existing", True)
    t.set_editor_property("save", True)
    TOOLS.import_asset_tasks([t])
    paths = t.get_editor_property("imported_object_paths")
    return EAL.load_asset(paths[0]) if paths else None


def surfaces():
    """SourceArt/Surfaces (prepared by Tools/prep_surfaces.py) -> /Game/Slop/Surfaces/<Set>/T_<Set>_*."""
    root = os.path.join(SRC, "Surfaces")
    n = 0
    for set_id in sorted(os.listdir(root)):
        d = os.path.join(root, set_id)
        dest = f"/Game/Slop/Surfaces/{set_id}"
        for suffix, name, kind in [("basecolor", "BaseColor", "srgb"), ("normal", "Normal", "normal"), ("orm", "ORM", "linear")]:
            png = os.path.join(d, f"{set_id}_{suffix}.png")
            if not os.path.exists(png):
                continue
            tex = import_as(png, dest, f"T_{set_id}_{name}")
            if not tex:
                log(f"{set_id}: {suffix} failed")
                continue
            if kind == "normal":
                tex.set_editor_property("compression_settings", unreal.TextureCompressionSettings.TC_NORMALMAP)
                tex.set_editor_property("srgb", False)
            elif kind == "linear":
                tex.set_editor_property("compression_settings", unreal.TextureCompressionSettings.TC_MASKS)
                tex.set_editor_property("srgb", False)
            tex.set_editor_property("lod_group", unreal.TextureGroup.TEXTUREGROUP_WORLD)
            EAL.save_loaded_asset(tex)
            n += 1
    log(f"surfaces: {n} textures")


def props():
    """Poly Haven models (glTF) -> /Game/Slop/Props/<name>; skies (.hdr) -> /Game/Slop/Skies."""
    models = os.path.join(SRC, "ThirdParty", "PolyHaven", "models")
    for name in sorted(os.listdir(models)):
        d = os.path.join(models, name)
        gltf = None
        for dd, _, fs in os.walk(d):
            for f in fs:
                if f.endswith(".gltf") or f.endswith(".glb"):
                    gltf = os.path.join(dd, f)
        if gltf:
            got = import_files([gltf], f"/Game/Slop/Props/{name}")
            log(f"prop {name}: {len(got)} assets")
    skies = os.path.join(SRC, "ThirdParty", "PolyHaven", "hdris")
    for name in sorted(os.listdir(skies)):
        for f in os.listdir(os.path.join(skies, name)):
            if f.endswith(".hdr"):
                tex = import_as(os.path.join(skies, name, f), "/Game/Slop/Skies", f"HDR_{name}")
                log(f"sky {name}: {'ok' if tex else 'failed'}")


def material_instance(name, parent, dest):
    path = f"{dest}/{name}"
    if EAL.does_asset_exist(path):
        mi = EAL.load_asset(path)
    else:
        mi = TOOLS.create_asset(name, dest, unreal.MaterialInstanceConstant, unreal.MaterialInstanceConstantFactoryNew())
    MEL.set_material_instance_parent(mi, parent)
    return mi


def set_two_sided(mi, on):
    o = mi.get_editor_property("base_property_overrides")
    o.set_editor_property("override_two_sided", bool(on))
    o.set_editor_property("two_sided", bool(on))
    mi.set_editor_property("base_property_overrides", o)


def colour(v, a=1.0):
    return unreal.LinearColor(v[0], v[1], v[2], v[3] if len(v) > 3 else a)


def prop_material(name):
    """PH_<model>_main / PH_<model>_glass: the material the Poly Haven glTF import made."""
    body, part = name[3:].rsplit("_", 1)
    want = body if part == "main" else f"{body}_{part}"
    for p in EAL.list_assets(f"/Game/Slop/Props/{body}", recursive=True, include_folder=False):
        if p.split(".")[-1] == want and isinstance(EAL.load_asset(p), unreal.MaterialInterface):
            return EAL.load_asset(p)
    return None


def water_params(mi, w):
    """An M_Water instance from a stage's water entry (or a glow's `liquid`)."""
    MEL.set_material_instance_vector_parameter_value(mi, "Color", colour(w["color"]))
    MEL.set_material_instance_scalar_parameter_value(mi, "Roughness", w.get("roughness", 0.05))
    MEL.set_material_instance_scalar_parameter_value(mi, "RippleStrength", w.get("ripple", 0.03))
    MEL.set_material_instance_scalar_parameter_value(mi, "RippleScale", w.get("ripple_scale", 0.012))
    if "emissive" in w:
        MEL.set_material_instance_vector_parameter_value(mi, "EmissiveColor", colour(w.get("emissive_color", (1, 1, 1))))
        MEL.set_material_instance_scalar_parameter_value(mi, "Emissive", w["emissive"])
    if w.get("pattern"):
        MEL.set_material_instance_scalar_parameter_value(mi, "Pattern", float(w["pattern"]))
        MEL.set_material_instance_scalar_parameter_value(mi, "PatternScale", w.get("pattern_scale", 1.0))


def stage_materials(st, d, dest):
    """One material instance per material name the stage FBX uses, from the stage's JSON."""
    out = {}
    surf = EAL.load_asset("/Game/Slop/Materials/M_Surface")
    for name, sd in d["surfaces"].items():
        mi = material_instance(f"MI_{st}_{name}", surf, dest)
        ts = sd["set"]
        for param, suffix in [("BaseColorTex", "BaseColor"), ("NormalTex", "Normal"), ("ORMTex", "ORM")]:
            tp = f"/Game/Slop/Surfaces/{ts}/T_{ts}_{suffix}"
            if EAL.does_asset_exist(tp):
                MEL.set_material_instance_texture_parameter_value(mi, param, EAL.load_asset(tp))
        MEL.set_material_instance_scalar_parameter_value(mi, "Tiling", sd.get("tiling", 1.0))
        MEL.set_material_instance_vector_parameter_value(mi, "Tint", colour(sd.get("tint", (1, 1, 1))))
        MEL.set_material_instance_scalar_parameter_value(mi, "Roughness", sd.get("roughness", 1.0))
        MEL.set_material_instance_scalar_parameter_value(mi, "Metallic", sd.get("metallic", 0.0))
        MEL.set_material_instance_scalar_parameter_value(mi, "NormalStrength", sd.get("normal_strength", 1.0))
        if "emissive" in sd:
            MEL.set_material_instance_vector_parameter_value(mi, "EmissiveColor", colour(sd.get("emissive_color", (1, 1, 1))))
            MEL.set_material_instance_scalar_parameter_value(mi, "Emissive", sd["emissive"])
        set_two_sided(mi, sd.get("two_sided", False))
        out[name] = mi
    glow = EAL.load_asset("/Game/Slop/Materials/M_StageGlow")
    water = EAL.load_asset("/Game/Slop/Materials/M_Water")
    for g in d.get("glows", []):
        if "liquid" in g:
            # A glowing liquid: the water's lit, rippled surface over the glow.
            mi = material_instance(f"MI_{st}_{g['material']}", water, dest)
            MEL.clear_all_material_instance_parameters(mi)
            water_params(mi, dict(g["liquid"], emissive_color=g["color"], emissive=g.get("strength", 1.0),
                                  pattern=g.get("pattern", 0.0), pattern_scale=g.get("pattern_scale", 1.0)))
            out[g["material"]] = mi
            continue
        mi = material_instance(f"MI_{st}_{g['material']}", glow, dest)
        MEL.set_material_instance_vector_parameter_value(mi, "Color", colour(g["color"]))
        MEL.set_material_instance_scalar_parameter_value(mi, "Intensity", g.get("strength", 1.0))
        pat = g.get("pattern", 0)
        if pat == "ribs":
            MEL.set_material_instance_scalar_parameter_value(mi, "Ribs", 1.0)
            MEL.set_material_instance_scalar_parameter_value(mi, "RibSpacing", g.get("pattern_scale", 0.03))
        elif pat:
            MEL.set_material_instance_scalar_parameter_value(mi, "Pattern", float(pat))
            MEL.set_material_instance_scalar_parameter_value(mi, "PatternScale", g.get("pattern_scale", 1.0))
        out[g["material"]] = mi
    for w in d.get("water", []):
        mi = material_instance(f"MI_{st}_{w['material']}", water, dest)
        water_params(mi, w)
        set_two_sided(mi, "tarfall" in w["material"])
        out[w["material"]] = mi
    for mi in out.values():
        MEL.update_material_instance(mi)
        EAL.save_loaded_asset(mi)
    return out


def stage_slots(st, d, mats):
    """Gives every mesh of a stage the material instances for its slots; returns (meshes, unmatched)."""
    dest = f"/Game/Slop/Stages/{st}"
    n, missing = 0, set()
    for p in EAL.list_assets(dest, recursive=False, include_folder=False):
        a = EAL.load_asset(p)
        if not isinstance(a, unreal.StaticMesh):
            continue
        if a.get_name().startswith("ORIGIN_MARKER"):
            # The 1 m scale check the stage builder leaves in the file.
            EAL.delete_asset(p.split(".")[0])
            continue
        n += 1
        sms = a.get_editor_property("static_materials")
        for i, sm in enumerate(sms):
            slot = str(sm.get_editor_property("material_slot_name"))
            mi = mats.get(slot) or (prop_material(slot) if slot.startswith("PH_") else None)
            if mi is None:
                missing.add(slot)
                continue
            sm.set_editor_property("material_interface", mi)
            sms[i] = sm
        a.set_editor_property("static_materials", sms)
        EAL.save_loaded_asset(a, False)
    return n, missing


def stages(import_meshes=True):
    """SourceArt/Stages/<id>/<id>.fbx (built by Tools/blender/stages) -> /Game/Slop/Stages/<id>:
    one static mesh per object, its slots given material instances made from <id>.json, which is
    copied to Content/Data/Stages for the game to place lights, fog, sway and weather. The slots
    are assigned in a later pass (step `slots`): changes made in the import's own run do not hold."""
    root = os.path.join(SRC, "Stages")
    data_dir = os.path.join(PROJECT, "Content", "Data", "Stages")
    os.makedirs(data_dir, exist_ok=True)
    for st in sorted(os.listdir(root)):
        fbx = os.path.join(root, st, st + ".fbx")
        js = os.path.join(root, st, st + ".json")
        if not (os.path.exists(fbx) and os.path.exists(js)):
            continue
        with open(js) as f:
            d = json.load(f)
        dest = f"/Game/Slop/Stages/{st}"
        if import_meshes:
            got = import_files([fbx], dest)
            with open(js) as f, open(os.path.join(data_dir, st + ".json"), "w") as g:
                g.write(f.read())
            log(f"stage {st}: imported {len(got)} assets")
            continue
        mats = stage_materials(st, d, dest + "/Materials")
        n, missing = stage_slots(st, d, mats)
        if not missing:
            # The importer's own placeholder materials are no longer used.
            for p in EAL.list_assets(dest, recursive=False, include_folder=False):
                if isinstance(EAL.load_asset(p), unreal.MaterialInterface):
                    EAL.delete_asset(p.split(".")[0])
        log(f"stage {st}: {n} meshes, {len(mats)} materials, unmatched slots {sorted(missing)}")


def slots():
    stages(import_meshes=False)
    fighters(import_meshes=False)


def portraits():
    """SourceArt/Portraits/<id>.png (Tools/portraits.py) -> /Game/Slop/Portraits/T_portrait_<id>."""
    root = os.path.join(SRC, "Portraits")
    n = 0
    for f in sorted(os.listdir(root)) if os.path.isdir(root) else []:
        if not f.endswith(".png"):
            continue
        tex = import_as(os.path.join(root, f), "/Game/Slop/Portraits", "T_portrait_" + f[:-4])
        if tex:
            tex.set_editor_property("lod_group", unreal.TextureGroup.TEXTUREGROUP_UI)
            tex.set_editor_property("mip_gen_settings", unreal.TextureMipGenSettings.TMGS_NO_MIPMAPS)
            tex.set_editor_property("never_stream", True)
            EAL.save_loaded_asset(tex)
            n += 1
    log(f"portraits: {n}")


def main():
    only = args()
    steps = [("audio", audio), ("materials", materials), ("fighters", fighters),
             ("surfaces", surfaces), ("props", props), ("stages", stages), ("slots", slots), ("portraits", portraits)]
    for name, fn in steps:
        if only is None or name in only:
            log("step " + name)
            fn()
    log("all done")


main()
