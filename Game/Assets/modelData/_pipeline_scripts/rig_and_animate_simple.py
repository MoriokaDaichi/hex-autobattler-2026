import bpy, sys, math
import mathutils

argv = sys.argv[sys.argv.index("--") + 1:]
fbx_path = argv[0]
out_blend = argv[1]
out_fbx = argv[2]
out_dir = argv[3]
unit_name = argv[4]

bpy.ops.wm.read_factory_settings(use_empty=True)
bpy.ops.import_scene.fbx(filepath=fbx_path)

mesh_objs = [o for o in bpy.data.objects if o.type == 'MESH']
if len(mesh_objs) > 1:
    bpy.ops.object.select_all(action='DESELECT')
    for o in mesh_objs:
        o.select_set(True)
    bpy.context.view_layer.objects.active = mesh_objs[0]
    bpy.ops.object.join()
mesh_obj = [o for o in bpy.data.objects if o.type == 'MESH'][0]

bpy.ops.object.select_all(action='DESELECT')
mesh_obj.select_set(True)
bpy.context.view_layer.objects.active = mesh_obj
bpy.ops.object.transform_apply(location=True, rotation=True, scale=True)

mat = mesh_obj.matrix_world
verts = [mat @ v.co for v in mesh_obj.data.vertices]
print(f"[{unit_name}] vertex count: {len(verts)}")

zs = [v.z for v in verts]
min_z, max_z = min(zs), max(zs)
height = max_z - min_z
center = mathutils.Vector((0, 0, min_z))

# ---------------- single-bone rig ----------------
arm_data = bpy.data.armatures.new("UnitArmature")
arm_obj = bpy.data.objects.new("Armature", arm_data)
bpy.context.collection.objects.link(arm_obj)
bpy.context.view_layer.objects.active = arm_obj
bpy.ops.object.mode_set(mode='EDIT')
eb = arm_data.edit_bones

b_root = eb.new("Root")
b_root.head = center
b_root.tail = center + mathutils.Vector((0, 0, height * 0.5))

bpy.ops.object.mode_set(mode='OBJECT')

# bind entire mesh to Root with full weight (single-bone skinning)
vg = mesh_obj.vertex_groups.new(name="Root")
vg.add(range(len(mesh_obj.data.vertices)), 1.0, 'REPLACE')

mod = mesh_obj.modifiers.new(name="Armature", type='ARMATURE')
mod.object = arm_obj
mesh_obj.parent = arm_obj

bpy.ops.wm.save_as_mainfile(filepath=out_blend)

# ---------------- animations ----------------
bpy.context.view_layer.objects.active = arm_obj
bpy.ops.object.mode_set(mode='POSE')
pb = arm_obj.pose.bones
for b in pb:
    b.rotation_mode = 'XYZ'

def clear_pose():
    for b in pb:
        b.rotation_euler = (0, 0, 0)
        b.location = (0, 0, 0)
        b.scale = (1, 1, 1)

def key_all(frame):
    for b in pb:
        b.keyframe_insert(data_path="rotation_euler", frame=frame)
        b.keyframe_insert(data_path="location", frame=frame)
        b.keyframe_insert(data_path="scale", frame=frame)

def new_action(name):
    full = f"{unit_name}_{name}"
    if full in bpy.data.actions:
        bpy.data.actions.remove(bpy.data.actions[full])
    act = bpy.data.actions.new(full)
    act.use_fake_user = True
    arm_obj.animation_data_create()
    arm_obj.animation_data.action = act
    return act

root = pb["Root"]

# Idle: slow breathing squash/stretch
new_action("Idle")
clear_pose(); key_all(0)
root.scale = (1.03, 1.03, 0.96)
root.location = (0, 0, height * 0.01)
key_all(12)
clear_pose(); key_all(24)

# Move: bounce hop
new_action("Move")
clear_pose(); key_all(0)
root.scale = (1.15, 1.15, 0.75)
key_all(4)
root.scale = (0.9, 0.9, 1.2)
root.location = (0, 0, height * 0.25)
key_all(12)
root.scale = (1.15, 1.15, 0.75)
root.location = (0, 0, 0)
key_all(20)
clear_pose(); key_all(24)

# NormalAttack: lunge forward stretch
new_action("NormalAttack")
clear_pose(); key_all(0)
root.scale = (0.85, 0.85, 1.15)
key_all(4)
root.scale = (1.25, 1.25, 0.7)
root.location = (0, height * 0.2, 0)
key_all(9)
clear_pose(); key_all(14)

# Skill: big inflate then burst outward
new_action("Skill")
clear_pose(); key_all(0)
root.scale = (0.8, 0.8, 0.8)
key_all(6)
root.scale = (1.4, 1.4, 1.4)
key_all(10)
root.scale = (0.95, 0.95, 1.05)
key_all(14)
clear_pose(); key_all(18)

# Death: melt/collapse
new_action("Death")
clear_pose(); key_all(0)
root.scale = (1.1, 1.1, 0.8)
root.location = (0, 0, -height * 0.05)
key_all(8)
root.scale = (1.4, 1.4, 0.15)
root.location = (0, 0, -height * 0.42)
key_all(22)
key_all(30)

clear_pose()
bpy.ops.object.mode_set(mode='OBJECT')

# ---------------- validation renders ----------------
bbox_center = mathutils.Vector((0, 0, (min_z + max_z) / 2))
cam_data = bpy.data.cameras.new("Cam")
cam_obj = bpy.data.objects.new("Cam", cam_data)
bpy.context.collection.objects.link(cam_obj)
dist = height * 2.2
cam_obj.location = (bbox_center.x + dist * 0.5, bbox_center.y - dist, bbox_center.z + height * 0.1)
direction = bbox_center - cam_obj.location
cam_obj.rotation_euler = direction.to_track_quat('-Z', 'Y').to_euler()
bpy.context.scene.camera = cam_obj

light_data = bpy.data.lights.new("Sun", type='SUN')
light_data.energy = 3.0
light_obj = bpy.data.objects.new("Sun", light_data)
bpy.context.collection.objects.link(light_obj)
light_obj.rotation_euler = (math.radians(50), 0, math.radians(20))

scene = bpy.context.scene
scene.render.resolution_x = 500
scene.render.resolution_y = 500
scene.render.engine = 'BLENDER_EEVEE_NEXT' if hasattr(bpy.types, 'SceneEEVEE') else 'BLENDER_EEVEE'

validation = [("Idle", 6), ("Move", 12), ("NormalAttack", 9), ("Skill", 10), ("Death", 22)]
for act_name, frame in validation:
    arm_obj.animation_data.action = bpy.data.actions[f"{unit_name}_{act_name}"]
    scene.frame_set(frame)
    out_path = f"{out_dir}/{unit_name}_{act_name}_{frame}.png"
    scene.render.filepath = out_path
    bpy.ops.render.render(write_still=True)
    print("RENDER:", out_path)

arm_obj.animation_data.action = bpy.data.actions[f"{unit_name}_Idle"]
bpy.ops.object.select_all(action='DESELECT')
mesh_obj.select_set(True)
arm_obj.select_set(True)
bpy.context.view_layer.objects.active = arm_obj
bpy.ops.export_scene.fbx(
    filepath=out_fbx,
    use_selection=True,
    add_leaf_bones=False,
    bake_anim=True,
    bake_anim_use_all_actions=True,
    bake_anim_use_nla_strips=False,
)
print("FBX EXPORT DONE:", out_fbx)
bpy.ops.wm.save_as_mainfile(filepath=out_blend)
print(f"[{unit_name}] ALL DONE")
