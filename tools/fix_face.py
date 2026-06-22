# GodotLuau - Arreglar la cara del modelo del usuario, conservando su geometria.
# Quita la textura rota (cuerpo gris limpio) y pone ojos + boca en la CABEZA
# (frente). Renderiza para verificar y exporta un .glb con la MISMA forma.
#   blender --background --python tools/fix_face.py -- <obj> <out_dir> <out_glb>
import bpy, sys, math, mathutils

a = sys.argv[sys.argv.index("--") + 1:] if "--" in sys.argv else []
obj_path, out_dir, out_glb = a[0], a[1].rstrip("/\\") + "/", a[2]

bpy.ops.wm.read_factory_settings(use_empty=True)
bpy.ops.wm.obj_import(filepath=obj_path)
meshes = [o for o in bpy.context.scene.objects if o.type == 'MESH']

def mat(name, rgb):
    m = bpy.data.materials.new(name); m.use_nodes = True
    b = m.node_tree.nodes.get("Principled BSDF")
    b.inputs["Base Color"].default_value = (rgb[0], rgb[1], rgb[2], 1.0)
    b.inputs["Roughness"].default_value = 0.92
    return m
gray = mat("Body", (0.62, 0.62, 0.62))
dark = mat("Face", (0.06, 0.06, 0.06))
for o in meshes:
    o.data.materials.clear(); o.data.materials.append(gray)

# Bounds (Blender Z arriba; frente = -Y tras importar el .obj)
mn = mathutils.Vector((1e9, 1e9, 1e9)); mx = -mn
for o in meshes:
    for c in o.bound_box:
        w = o.matrix_world @ mathutils.Vector(c)
        for i in range(3): mn[i] = min(mn[i], w[i]); mx[i] = max(mx[i], w[i])
H = mx.z - mn.z; cx = (mn.x + mx.x) / 2.0
print("BOUNDS min=", tuple(round(v,3) for v in mn), " max=", tuple(round(v,3) for v in mx), " H=", round(H,3))

def box(name, center, size, material):
    bpy.ops.mesh.primitive_cube_add(size=1.0, location=center)
    o = bpy.context.active_object; o.name = name; o.scale = size
    bpy.ops.object.transform_apply(scale=True)
    o.data.materials.clear(); o.data.materials.append(material)

fy = mn.y - 0.01            # justo en la cara frontal (-Y)
ez = mx.z - 0.085 * H       # altura de los ojos
my = mx.z - 0.135 * H       # altura de la boca (sonrisa)
es = 0.038 * H              # tamano ojo
box("Eye_L", (cx - 0.05 * H, fy, ez), (es, 0.03 * H, es * 1.15), dark)
box("Eye_R", (cx + 0.05 * H, fy, ez), (es, 0.03 * H, es * 1.15), dark)
box("Mouth", (cx,            fy, my), (0.105 * H, 0.03 * H, 0.022 * H), dark)

# ── Render para verificar ─────────────────────────────────────────────────────
center = (mn + mx) / 2.0; diag = (mx - mn).length
empty = bpy.data.objects.new("T", None); bpy.context.collection.objects.link(empty); empty.location = center
light = bpy.data.lights.new("L", 'SUN'); light.energy = 3.0
lo = bpy.data.objects.new("L", light); bpy.context.collection.objects.link(lo); lo.rotation_euler = (math.radians(55), 0, math.radians(20))
world = bpy.data.worlds.new("W"); world.use_nodes = True
world.node_tree.nodes["Background"].inputs[1].default_value = 1.2
bpy.context.scene.world = world
cam_d = bpy.data.cameras.new("C"); cam = bpy.data.objects.new("C", cam_d)
bpy.context.collection.objects.link(cam); bpy.context.scene.camera = cam
cam.constraints.new('TRACK_TO').target = empty
sc = bpy.context.scene
try: sc.render.engine = 'BLENDER_EEVEE_NEXT'
except Exception: pass
sc.render.resolution_x = 420; sc.render.resolution_y = 560
def shoot(name, loc):
    cam.location = loc; sc.render.filepath = out_dir + name
    bpy.ops.render.render(write_still=True)
d = diag * 1.25
shoot("face_front.png", (center.x,            center.y - d, center.z + diag * 0.12))
shoot("face_34.png",    (center.x - d * 0.6,  center.y - d * 0.8, center.z + diag * 0.18))

bpy.ops.object.select_all(action='SELECT')
bpy.ops.export_scene.gltf(filepath=out_glb, export_format='GLB', export_yup=True, use_selection=True)
print("FIXFACE_OK", out_glb)
