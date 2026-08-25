import bpy, sys, math

argv = sys.argv[sys.argv.index("--") + 1:]
blend_path = argv[0]
out_dir = argv[1]
out_fbx = argv[2]

bpy.ops.wm.open_mainfile(filepath=blend_path)

mesh_obj = [o for o in bpy.data.objects if o.type == 'MESH'][0]
arm_obj = [o for o in bpy.data.objects if o.type == 'ARMATURE'][0]
bpy.context.view_layer.objects.active = arm_obj
bpy.ops.object.mode_set(mode='POSE')
pb = arm_obj.pose.bones

for b in pb:
    b.rotation_mode = 'XYZ'


def clear_pose():
    for b in pb:
        b.rotation_euler = (0, 0, 0)
        b.location = (0, 0, 0)


def key_all(frame):
    for b in pb:
        b.keyframe_insert(data_path="rotation_euler", frame=frame)
        b.keyframe_insert(data_path="location", frame=frame)


def new_action(name):
    if name in bpy.data.actions:
        bpy.data.actions.remove(bpy.data.actions[name])
    act = bpy.data.actions.new(name)
    arm_obj.animation_data_create()
    arm_obj.animation_data.action = act
    return act


FPS = 24

# ---------------- Idle ----------------
act = new_action("Idle")
clear_pose()
key_all(0)
pb["Chest"].rotation_euler = (math.radians(-3), 0, 0)
pb["Hips"].location = (0, 0, 0.01)
key_all(12)
clear_pose()
key_all(24)

# ---------------- Move (walk) ----------------
act = new_action("Move")
clear_pose()
key_all(0)

def walk_pose(frame, phase):
    # phase in [0,1): 0 = right leg forward / left leg back
    swing = math.sin(phase * 2 * math.pi)
    hip_amt = math.radians(22)
    knee_amt = math.radians(35)
    pb["Hip_L"].rotation_euler = (swing * hip_amt, 0, 0)
    pb["Hip_R"].rotation_euler = (-swing * hip_amt, 0, 0)
    # knee bends when leg is swinging backward-to-forward (recovery): use a
    # half-rectified sine so the knee only bends during lift, not on plant
    knee_l = max(0.0, math.sin((phase + 0.5) * 2 * math.pi)) * knee_amt
    knee_r = max(0.0, math.sin(phase * 2 * math.pi)) * knee_amt
    pb["Knee_L"].rotation_euler = (knee_l, 0, 0)
    pb["Knee_R"].rotation_euler = (knee_r, 0, 0)
    arm_amt = math.radians(18)
    pb["Shoulder_L"].rotation_euler = (-swing * arm_amt, 0, 0)
    pb["Shoulder_R"].rotation_euler = (swing * arm_amt, 0, 0)
    pb["Hips"].location = (0, 0, abs(swing) * 0.015)
    key_all(frame)

walk_pose(0, 0.0)
walk_pose(6, 0.25)
walk_pose(12, 0.5)
walk_pose(18, 0.75)
walk_pose(24, 1.0)

# ---------------- NormalAttack (dagger stab) ----------------
act = new_action("NormalAttack")
clear_pose()
key_all(0)
# wind-up
pb["Shoulder_R"].rotation_euler = (math.radians(-15), 0, math.radians(-10))
pb["Elbow_R"].rotation_euler = (math.radians(60), 0, 0)
key_all(4)
# stab forward
pb["Shoulder_R"].rotation_euler = (math.radians(45), 0, math.radians(5))
pb["Elbow_R"].rotation_euler = (math.radians(10), 0, 0)
key_all(9)
# return
clear_pose()
key_all(14)

# ---------------- Skill (quick double stab) ----------------
act = new_action("Skill")
clear_pose()
key_all(0)
pb["Shoulder_R"].rotation_euler = (math.radians(-15), 0, math.radians(-10))
pb["Elbow_R"].rotation_euler = (math.radians(60), 0, 0)
key_all(3)
pb["Shoulder_R"].rotation_euler = (math.radians(45), 0, math.radians(5))
pb["Elbow_R"].rotation_euler = (math.radians(10), 0, 0)
key_all(6)
pb["Shoulder_L"].rotation_euler = (math.radians(-15), 0, math.radians(10))
pb["Elbow_L"].rotation_euler = (math.radians(60), 0, 0)
key_all(9)
pb["Shoulder_L"].rotation_euler = (math.radians(45), 0, math.radians(-5))
pb["Elbow_L"].rotation_euler = (math.radians(10), 0, 0)
key_all(12)
clear_pose()
key_all(18)

# ---------------- Death (collapse) ----------------
act = new_action("Death")
clear_pose()
key_all(0)
pb["Hips"].rotation_euler = (math.radians(30), 0, 0)
pb["Hips"].location = (0, -0.05, -0.05)
pb["Knee_L"].rotation_euler = (math.radians(20), 0, 0)
pb["Knee_R"].rotation_euler = (math.radians(20), 0, 0)
key_all(8)
pb["Hips"].rotation_euler = (math.radians(85), 0, 0)
pb["Hips"].location = (0, -0.25, -0.35)
pb["Chest"].rotation_euler = (math.radians(-20), 0, 0)
key_all(20)
# hold
key_all(30)

clear_pose()
bpy.ops.object.mode_set(mode='OBJECT')

# ---------------- render validation frames ----------------
import mathutils
mesh_mat = mesh_obj.matrix_world
verts = [mesh_mat @ v.co for v in mesh_obj.data.vertices]
zs = [v.z for v in verts]
min_z, max_z = min(zs), max(zs)
height = max_z - min_z
center = mathutils.Vector((0, 0, (min_z + max_z) / 2))

cam_data = bpy.data.cameras.new("Cam")
cam_obj = bpy.data.objects.new("Cam", cam_data)
bpy.context.collection.objects.link(cam_obj)
dist = height * 1.7
cam_obj.location = (center.x + dist * 0.5, center.y - dist, center.z + height * 0.05)
cam_obj.rotation_euler = (math.radians(85), 0, math.radians(22))
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

validation = [
    ("Idle", 6),
    ("Move", 6),
    ("Move", 18),
    ("NormalAttack", 9),
    ("Skill", 12),
    ("Death", 20),
]

for act_name, frame in validation:
    arm_obj.animation_data.action = bpy.data.actions[act_name]
    scene.frame_set(frame)
    out_path = f"{out_dir}/goblin_{act_name}_{frame}.png"
    scene.render.filepath = out_path
    bpy.ops.render.render(write_still=True)
    print("RENDER:", out_path)

# ---------------- export FBX with all actions ----------------
arm_obj.animation_data.action = bpy.data.actions["Idle"]
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

bpy.ops.wm.save_as_mainfile(filepath=blend_path)
