# GodotLuau - Render de vista previa de un .obj texturizado (Blender headless)
# Sirve para VER el modelo (cara/textura) sin abrir Godot.
#   blender --background --python tools/render_preview.py -- <obj> <tex.png> <out_dir>
import bpy, sys, math, mathutils

a = sys.argv[sys.argv.index("--") + 1:] if "--" in sys.argv else []
obj_path = a[0]
tex_path = a[1]
out_dir  = a[2].rstrip("/\\") + "/"

bpy.ops.wm.read_factory_settings(use_empty=True)
bpy.ops.wm.obj_import(filepath=obj_path)
meshes = [o for o in bpy.context.scene.objects if o.type == 'MESH']

# Material limpio: textura como BASE COLOR via UV (como lo aplica Godot),
# ignorando el map_d del .mtl que la usaba como opacidad.
img = bpy.data.images.load(tex_path)
mat = bpy.data.materials.new("Preview"); mat.use_nodes = True
nt = mat.node_tree
bsdf = nt.nodes.get("Principled BSDF")
tex = nt.nodes.new("ShaderNodeTexImage"); tex.image = img
nt.links.new(tex.outputs["Color"], bsdf.inputs["Base Color"])
bsdf.inputs["Roughness"].default_value = 0.9
for o in meshes:
    o.data.materials.clear(); o.data.materials.append(mat)

# Bounds
mn = mathutils.Vector((1e9, 1e9, 1e9)); mx = -mn
for o in meshes:
    for c in o.bound_box:
        w = o.matrix_world @ mathutils.Vector(c)
        for i in range(3):
            mn[i] = min(mn[i], w[i]); mx[i] = max(mx[i], w[i])
center = (mn + mx) / 2.0
diag = (mx - mn).length

empty = bpy.data.objects.new("T", None); bpy.context.collection.objects.link(empty)
empty.location = center

light = bpy.data.lights.new("L", 'SUN'); light.energy = 3.0
lo = bpy.data.objects.new("L", light); bpy.context.collection.objects.link(lo)
lo.rotation_euler = (math.radians(55), 0, math.radians(25))

world = bpy.data.worlds.new("W"); world.use_nodes = True
world.node_tree.nodes["Background"].inputs[1].default_value = 1.2
bpy.context.scene.world = world

cam_d = bpy.data.cameras.new("C"); cam = bpy.data.objects.new("C", cam_d)
bpy.context.collection.objects.link(cam); bpy.context.scene.camera = cam
con = cam.constraints.new('TRACK_TO'); con.target = empty

sc = bpy.context.scene
try: sc.render.engine = 'BLENDER_EEVEE_NEXT'
except Exception:
    try: sc.render.engine = 'BLENDER_EEVEE'
    except Exception: pass
sc.render.resolution_x = 420; sc.render.resolution_y = 560

def shoot(name, loc):
    cam.location = loc
    sc.render.filepath = out_dir + name
    bpy.ops.render.render(write_still=True)

d = diag * 1.3
shoot("front.png", (center.x,        center.y - d, center.z + diag * 0.10))
shoot("back.png",  (center.x,        center.y + d, center.z + diag * 0.10))
print("RENDER_OK", out_dir)
