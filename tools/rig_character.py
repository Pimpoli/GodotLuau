# GodotLuau - Generador de personaje R15 riggeado (para Blender headless)
#
# Crea un personaje estilo Roblox R15 con partes SEPARADAS, un esqueleto
# (armature) de 16 huesos y una cara en la cabeza. Cada parte es una caja atada
# rigidamente a su hueso, como el R15 real, exportado como .glb para Godot.
#
# Los HUESOS llevan nombre limpio (Head, LeftUpperArm, ...) para poder animarlos
# desde codigo (skel.find_bone("LeftUpperArm")); las mallas usan sufijo "_M".
#
# Uso:
#   blender --background --python tools/rig_character.py -- <salida.glb>
import bpy, sys

out = "AvatarR15_rigged.glb"
if "--" in sys.argv:
    a = sys.argv[sys.argv.index("--") + 1:]
    if a: out = a[0]

bpy.ops.wm.read_factory_settings(use_empty=True)

# ── Armature ──────────────────────────────────────────────────────────────────
arm_data = bpy.data.armatures.new("Rig")
arm_obj  = bpy.data.objects.new("Skeleton", arm_data)
bpy.context.collection.objects.link(arm_obj)
bpy.context.view_layer.objects.active = arm_obj
bpy.ops.object.mode_set(mode='EDIT')
ebs = arm_data.edit_bones

def P(name, parent, bh, bt, c, s): return dict(name=name, parent=parent, bh=bh, bt=bt, c=c, s=s)
SX, HX = 0.55, 0.20
parts = [
    P("HumanoidRootPart", None, (0,0,1.0), (0,0,1.1), None, None),
    P("LowerTorso", "HumanoidRootPart", (0,0,1.0), (0,0,1.2), (0,0,1.10), (0.80,0.45,0.25)),
    P("UpperTorso", "LowerTorso",       (0,0,1.2), (0,0,1.6), (0,0,1.42), (0.95,0.50,0.45)),
    P("Head",       "UpperTorso",       (0,0,1.6), (0,0,1.95),(0,0,1.78), (0.60,0.60,0.55)),
]
for side, sx in (("Left", 1.0), ("Right", -1.0)):
    parts += [
        P(side+"UpperArm","UpperTorso",(sx*SX,0,1.55),(sx*SX,0,1.15),(sx*SX,0,1.35),(0.30,0.30,0.45)),
        P(side+"LowerArm",side+"UpperArm",(sx*SX,0,1.15),(sx*SX,0,0.80),(sx*SX,0,0.97),(0.28,0.28,0.42)),
        P(side+"Hand",    side+"LowerArm",(sx*SX,0,0.80),(sx*SX,0,0.65),(sx*SX,0,0.72),(0.30,0.30,0.18)),
        P(side+"UpperLeg","HumanoidRootPart",(sx*HX,0,1.00),(sx*HX,0,0.55),(sx*HX,0,0.77),(0.35,0.35,0.50)),
        P(side+"LowerLeg",side+"UpperLeg",(sx*HX,0,0.55),(sx*HX,0,0.12),(sx*HX,0,0.33),(0.33,0.33,0.45)),
        P(side+"Foot",    side+"LowerLeg",(sx*HX,0,0.12),(sx*HX,0.15,0.05),(sx*HX,0.03,0.06),(0.34,0.50,0.14)),
    ]

made = {}
for p in parts:
    b = ebs.new(p["name"]); b.head = p["bh"]; b.tail = p["bt"]; made[p["name"]] = b
for p in parts:
    if p["parent"]:
        made[p["name"]].parent = made[p["parent"]]; made[p["name"]].use_connect = False
bpy.ops.object.mode_set(mode='OBJECT')

# ── Materiales ────────────────────────────────────────────────────────────────
def mat(name, rgb):
    m = bpy.data.materials.new(name); m.use_nodes = True
    n = m.node_tree.nodes.get("Principled BSDF")
    if n:
        n.inputs["Base Color"].default_value = (rgb[0], rgb[1], rgb[2], 1.0)
        if "Roughness" in n.inputs: n.inputs["Roughness"].default_value = 0.9
    return m
body_mat = mat("Body", (0.62, 0.62, 0.62))
head_mat = mat("Head", (0.96, 0.80, 0.22))   # cabeza amarilla estilo Roblox
face_mat = mat("Face", (0.05, 0.05, 0.05))    # ojos / boca

# ── Caja atada rigidamente a un hueso ─────────────────────────────────────────
def make_box(obj_name, bone, center, size, material):
    bpy.ops.mesh.primitive_cube_add(size=1.0, location=center)
    o = bpy.context.active_object
    o.name = obj_name
    o.scale = size
    bpy.ops.object.transform_apply(scale=True)
    vg = o.vertex_groups.new(name=bone)
    vg.add(list(range(len(o.data.vertices))), 1.0, 'REPLACE')
    md = o.modifiers.new("Armature", 'ARMATURE'); md.object = arm_obj
    o.parent = arm_obj
    o.data.materials.append(material)
    return o

for p in parts:
    if p["c"] is None: continue
    make_box(p["name"] + "_M", p["name"], p["c"], p["s"], head_mat if p["name"] == "Head" else body_mat)

# ── Cara en la cabeza (front = -Y en Blender). Ojos + boca, atados al hueso Head.
FY = -0.31           # justo en la cara frontal de la cabeza (half-depth 0.30)
make_box("Eye_L_M", "Head", (-0.13, FY, 1.84), (0.09, 0.03, 0.10), face_mat)
make_box("Eye_R_M", "Head", ( 0.13, FY, 1.84), (0.09, 0.03, 0.10), face_mat)
make_box("Mouth_M", "Head", ( 0.00, FY, 1.71), (0.22, 0.03, 0.05), face_mat)

# ── Exportar GLB ──────────────────────────────────────────────────────────────
bpy.ops.object.select_all(action='SELECT')
bpy.ops.export_scene.gltf(filepath=out, export_format='GLB', export_yup=True, use_selection=True)
print("RIG_EXPORT_OK:", out)
