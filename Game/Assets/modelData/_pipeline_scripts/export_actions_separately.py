import bpy, sys

argv = sys.argv[sys.argv.index("--") + 1:]
blend_path = argv[0]
out_dir = argv[1]
unit_name = argv[2]

bpy.ops.wm.open_mainfile(filepath=blend_path)

mesh_obj = [o for o in bpy.data.objects if o.type == 'MESH'][0]
arm_obj = [o for o in bpy.data.objects if o.type == 'ARMATURE'][0]

actions = ["Idle", "Move", "NormalAttack", "Skill", "Death"]
scene = bpy.context.scene
scene.render.fps = 24

for act_name in actions:
    full = f"{unit_name}_{act_name}"
    act = bpy.data.actions.get(full)
    if act is None:
        act = bpy.data.actions.get(act_name)  # fallback: unprefixed action names
    if act is None:
        print(f"MISSING ACTION: {full}")
        continue
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

print(f"[{unit_name}] SEPARATE EXPORT DONE")
