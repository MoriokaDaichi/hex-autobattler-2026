import bpy, sys, math

argv = sys.argv[sys.argv.index("--") + 1:]
blend_path = argv[0]
out_dir = argv[1]
unit_name = argv[2]

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
    full = f"{unit_name}_{name}"
    if full in bpy.data.actions:
        bpy.data.actions.remove(bpy.data.actions[full])
    act = bpy.data.actions.new(full)
    act.use_fake_user = True  # prevent Blender from discarding it as an orphan on save
    arm_obj.animation_data_create()
    arm_obj.animation_data.action = act
    return act


# purge any stale single-action leftover from the previous buggy run
for a in list(bpy.data.actions):
    bpy.data.actions.remove(a)

new_action("Idle")
clear_pose(); key_all(0)
pb["Chest"].rotation_euler = (math.radians(-3), 0, 0)
pb["Hips"].location = (0, 0, 0.01)
key_all(12)
clear_pose(); key_all(24)

new_action("Move")
clear_pose(); key_all(0)

def walk_pose(frame, phase):
    swing = math.sin(phase * 2 * math.pi)
    hip_amt = math.radians(22)
    knee_amt = math.radians(35)
    pb["Hip_L"].rotation_euler = (swing * hip_amt, 0, 0)
    pb["Hip_R"].rotation_euler = (-swing * hip_amt, 0, 0)
    knee_l = max(0.0, math.sin((phase + 0.5) * 2 * math.pi)) * knee_amt
    knee_r = max(0.0, math.sin(phase * 2 * math.pi)) * knee_amt
    pb["Knee_L"].rotation_euler = (knee_l, 0, 0)
    pb["Knee_R"].rotation_euler = (knee_r, 0, 0)
    arm_amt = math.radians(18)
    pb["Shoulder_L"].rotation_euler = (-swing * arm_amt, 0, 0)
    pb["Shoulder_R"].rotation_euler = (swing * arm_amt, 0, 0)
    pb["Hips"].location = (0, 0, abs(swing) * 0.015)
    key_all(frame)

walk_pose(0, 0.0); walk_pose(6, 0.25); walk_pose(12, 0.5); walk_pose(18, 0.75); walk_pose(24, 1.0)

new_action("NormalAttack")
clear_pose(); key_all(0)
pb["Shoulder_R"].rotation_euler = (math.radians(-15), 0, math.radians(-10))
pb["Elbow_R"].rotation_euler = (math.radians(60), 0, 0)
key_all(4)
pb["Shoulder_R"].rotation_euler = (math.radians(45), 0, math.radians(5))
pb["Elbow_R"].rotation_euler = (math.radians(10), 0, 0)
key_all(9)
clear_pose(); key_all(14)

new_action("Skill")
clear_pose(); key_all(0)
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
clear_pose(); key_all(18)

new_action("Death")
clear_pose(); key_all(0)
pb["Hips"].rotation_euler = (math.radians(30), 0, 0)
pb["Hips"].location = (0, -0.05, -0.05)
pb["Knee_L"].rotation_euler = (math.radians(20), 0, 0)
pb["Knee_R"].rotation_euler = (math.radians(20), 0, 0)
key_all(8)
pb["Hips"].rotation_euler = (math.radians(85), 0, 0)
pb["Hips"].location = (0, -0.25, -0.35)
pb["Chest"].rotation_euler = (math.radians(-20), 0, 0)
key_all(20)
key_all(30)

clear_pose()
bpy.ops.object.mode_set(mode='OBJECT')

scene = bpy.context.scene
scene.render.fps = 24

# ---------------- export one FBX per action ----------------
actions = ["Idle", "Move", "NormalAttack", "Skill", "Death"]
for act_name in actions:
    full = f"{unit_name}_{act_name}"
    act = bpy.data.actions[full]
    arm_obj.animation_data.action = act
    frame_start = int(act.frame_range[0])
    frame_end = int(act.frame_range[1])
    scene.frame_start = frame_start
    scene.frame_end = frame_end

    bpy.ops.object.select_all(action='DESELECT')
    mesh_obj.select_set(True)
    arm_obj.select_set(True)
    bpy.context.view_layer.objects.active = arm_obj

    out_fbx = f"{out_dir}/{unit_name}_{act_name}.fbx"
    bpy.ops.export_scene.fbx(
        filepath=out_fbx,
        use_selection=True,
        add_leaf_bones=False,
        bake_anim=True,
        bake_anim_use_all_actions=False,
        bake_anim_use_nla_strips=False,
        bake_anim_step=1.0,
    )
    print(f"EXPORTED [{unit_name}] {act_name}: frames {frame_start}-{frame_end} -> {out_fbx}")

bpy.ops.wm.save_as_mainfile(filepath=blend_path)
print(f"[{unit_name}] ACTIONS SAVED:", [a.name for a in bpy.data.actions])
print(f"[{unit_name}] REANIMATE DONE")
