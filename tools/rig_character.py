# GodotLuau - Generador de personaje R15 riggeado (para Blender headless)
#
# Crea un personaje estilo Roblox R15 con partes SEPARADAS y un esqueleto
# (armature). Cada parte es una caja atada rigidamente a un hueso, como el R15
# real (partes rigidas unidas por juntas), exportado como .glb para Godot.
#
# Uso:
#   blender --background --python tools/rig_character.py -- <ruta_salida.glb>
import bpy, sys, math

out = "AvatarR15_rigged.glb"
if "--" in sys.argv:
    args = sys.argv[sys.argv.index("--") + 1:]
    if args: out = args[0]

# ── Escena limpia ─────────────────────────────────────────────────────────────
bpy.ops.wm.read_factory_settings(use_empty=True)

# ── Armature ──────────────────────────────────────────────────────────────────
arm_data = bpy.data.armatures.new("Rig")
arm_obj  = bpy.data.objects.new("Skeleton", arm_data)
bpy.context.collection.objects.link(arm_obj)
bpy.context.view_layer.objects.active = arm_obj
bpy.ops.object.mode_set(mode='EDIT')
ebs = arm_data.edit_bones

# name, parent, bone_head, bone_tail, box_center, box_size  (Z arriba, metros)
def P(name, parent, bh, bt, c, s): return dict(name=name, parent=parent, bh=bh, bt=bt, c=c, s=s)
SX = 0.55   # x del hombro
HX = 0.20   # x de la cadera
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
    b = ebs.new(p["name"])
    b.head = p["bh"]; b.tail = p["bt"]
    made[p["name"]] = b
for p in parts:
    if p["parent"]:
        made[p["name"]].parent = made[p["parent"]]
        made[p["name"]].use_connect = False
bpy.ops.object.mode_set(mode='OBJECT')

# ── Materiales ────────────────────────────────────────────────────────────────
def mat(name, rgb):
    m = bpy.data.materials.new(name); m.use_nodes = True
    bsdf = m.node_tree.nodes.get("Principled BSDF")
    if bsdf:
        bsdf.inputs["Base Color"].default_value = (rgb[0], rgb[1], rgb[2], 1.0)
        if "Roughness" in bsdf.inputs: bsdf.inputs["Roughness"].default_value = 0.9
    return m
body_mat = mat("Body", (0.62, 0.62, 0.62))
head_mat = mat("Head", (0.95, 0.80, 0.20))   # cabeza amarilla (estilo Roblox)

# ── Cajas (partes) atadas rigidamente a su hueso ──────────────────────────────
for p in parts:
    if p["c"] is None: continue   # HumanoidRootPart: invisible
    bpy.ops.mesh.primitive_cube_add(size=1.0, location=p["c"])
    obj = bpy.context.active_object
    obj.name = p["name"]
    obj.scale = p["s"]
    bpy.ops.object.transform_apply(scale=True)
    vg = obj.vertex_groups.new(name=p["name"])
    vg.add(list(range(len(obj.data.vertices))), 1.0, 'REPLACE')
    md = obj.modifiers.new("Armature", 'ARMATURE'); md.object = arm_obj
    obj.parent = arm_obj
    obj.data.materials.append(head_mat if p["name"] == "Head" else body_mat)

# ── Exportar GLB ──────────────────────────────────────────────────────────────
bpy.ops.object.select_all(action='SELECT')
bpy.ops.export_scene.gltf(filepath=out, export_format='GLB',
                          export_yup=True, use_selection=True)
print("RIG_EXPORT_OK:", out)
