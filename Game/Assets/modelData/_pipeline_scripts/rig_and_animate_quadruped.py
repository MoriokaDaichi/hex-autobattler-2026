import bpy, sys, math
import mathutils

argv = sys.argv[sys.argv.index("--") + 1:]
fbx_path = argv[0]
out_blend = argv[1]
out_fbx = argv[2]
out_dir = argv[3]
unit_name = argv[4]
head_sign = float(argv[5])   # +1.0 if head is at +Y, -1.0 if head is at -Y
has_wings = argv[6] == "1"

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

def fy(v):
    return v.y * head_sign

xs = [v.x for v in verts]
zs = [v.z for v in verts]
fys = [fy(v) for v in verts]
min_x, max_x = min(xs), max(xs)
min_z, max_z = min(zs), max(zs)
min_fy, max_fy = min(fys), max(fys)
height = max_z - min_z
length = max_fy - min_fy
print(f"[{unit_name}] height={height:.3f} length={length:.3f}")

# ---------------- torso profile along the forward axis ----------------
N = 40
band_edges = [min_fy + length * i / N for i in range(N + 1)]

def band_stats(i):
    y0, y1 = band_edges[i], band_edges[i + 1]
    pts = [v for v in verts if y0 <= fy(v) < y1]
    if not pts:
        return 0.0, 0.0, 0
    xs_b = [p.x for p in pts]
    zs_b = [p.z for p in pts]
    return (max(xs_b) - min(xs_b)), (max(zs_b) - min(zs_b)), len(pts)

widths, heights_b, counts = [], [], []
for i in range(N):
    w, h, c = band_stats(i)
    widths.append(w); heights_b.append(h); counts.append(c)

bulk = [widths[i] * heights_b[i] for i in range(N)]
torso_band = max(range(N), key=lambda i: bulk[i])

# neck: local minimum of bulk searching toward the head (increasing i)
neck_band = None
neck_val = 1e18
search_end = min(N - 1, torso_band + max(2, int(N * 0.45)))
for i in range(torso_band + 2, search_end):
    if bulk[i] < neck_val:
        neck_val = bulk[i]
        neck_band = i
if neck_band is None:
    neck_band = min(N - 1, torso_band + int(N * 0.15))

head_tip_band = N - 1
while counts[head_tip_band] == 0 and head_tip_band > neck_band:
    head_tip_band -= 1

# tail root: moving from torso toward the rear (decreasing i) until bulk drops sharply
threshold = bulk[torso_band] * 0.32
tail_root_band = max(0, torso_band - 2)
for i in range(torso_band, -1, -1):
    if bulk[i] < threshold:
        tail_root_band = i
        break

tail_tip_band = 0
while counts[tail_tip_band] == 0 and tail_tip_band < tail_root_band:
    tail_tip_band += 1

def fy_to_world_y(fy_val):
    return fy_val * head_sign

def band_pos(i, z=None):
    y0, y1 = band_edges[i], band_edges[i + 1]
    pts = [v for v in verts if y0 <= fy(v) < y1]
    if not pts:
        pts = verts
    y = sum(p.y for p in pts) / len(pts)
    zc = sum(p.z for p in pts) / len(pts) if z is None else z
    return mathutils.Vector((0.0, y, zc))

hips_pos = band_pos(torso_band, z=min_z + height * 0.55)
chest_pos = band_pos(neck_band, z=min_z + height * 0.58)
spine_pos = mathutils.Vector((0.0, (hips_pos.y + chest_pos.y) / 2, (hips_pos.z + chest_pos.z) / 2))
neck_pos = band_pos(neck_band, z=min_z + height * 0.68)
head_end_y = sum(p.y for p in verts if fy(p) >= band_edges[head_tip_band])
head_pts = [p for p in verts if fy(p) >= band_edges[max(0, head_tip_band - 2)]]
if not head_pts:
    head_pts = verts
head_top = mathutils.Vector((0.0, sum(p.y for p in head_pts) / len(head_pts), max(p.z for p in head_pts)))

tail_root_pos = band_pos(tail_root_band, z=min_z + height * 0.55)
tail_mid_band = max(0, (tail_root_band + tail_tip_band) // 2)
tail_mid_pos = band_pos(tail_mid_band)
tail_tip_pts = [p for p in verts if fy(p) <= band_edges[min(N - 1, tail_tip_band + 2)]]
if not tail_tip_pts:
    tail_tip_pts = verts
tail_tip_pos = mathutils.Vector((0.0, sum(p.y for p in tail_tip_pts) / len(tail_tip_pts), sum(p.z for p in tail_tip_pts) / len(tail_tip_pts)))

print(f"[{unit_name}] torso_band={torso_band} neck_band={neck_band} head_tip_band={head_tip_band} tail_root_band={tail_root_band} tail_tip_band={tail_tip_band}")

# ---------------- legs ----------------
leg_cut_z = min_z + height * 0.42
leg_verts = [v for v in verts if v.z < leg_cut_z]
cx_leg = sum(v.x for v in leg_verts) / len(leg_verts)
divider_y = (hips_pos.y + chest_pos.y) / 2  # world-space Y midpoint between hips and chest

def in_front(v):
    return (fy(v) - fy(mathutils.Vector((0, divider_y, 0)))) >= 0

front_left  = [v for v in leg_verts if v.x <  cx_leg and in_front(v)]
front_right = [v for v in leg_verts if v.x >= cx_leg and in_front(v)]
back_left   = [v for v in leg_verts if v.x <  cx_leg and not in_front(v)]
back_right  = [v for v in leg_verts if v.x >= cx_leg and not in_front(v)]

def leg_landmarks(group):
    if not group:
        group = leg_verts
    zs_g = sorted(v.z for v in group)
    def centroid_near(z0, z1):
        pts = [v for v in group if z0 <= v.z <= z1]
        if not pts:
            pts = group
        x = sum(p.x for p in pts) / len(pts)
        y = sum(p.y for p in pts) / len(pts)
        z = sum(p.z for p in pts) / len(pts)
        return mathutils.Vector((x, y, z))
    zmin, zmax = zs_g[0], zs_g[-1]
    ankle = centroid_near(zmin, zmin + (zmax - zmin) * 0.25)
    knee = centroid_near(zmin + (zmax - zmin) * 0.45, zmin + (zmax - zmin) * 0.65)
    top = centroid_near(zmax - (zmax - zmin) * 0.20, zmax)
    return {"ankle": ankle, "knee": knee, "top": top}

leg_fl = leg_landmarks(front_left)
leg_fr = leg_landmarks(front_right)
leg_bl = leg_landmarks(back_left)
leg_br = leg_landmarks(back_right)

def foot_tail(ankle):
    return mathutils.Vector((ankle.x, ankle.y, min_z))

# ---------------- wings ----------------
wing_L = wing_R = None
if has_wings:
    body_half_width = max(max_x, -min_x)
    wing_z_min = min_z + height * 0.45
    wing_pts = [v for v in verts if v.z > wing_z_min and abs(v.x) > body_half_width * 0.55]
    wl = [v for v in wing_pts if v.x < 0]
    wr = [v for v in wing_pts if v.x >= 0]

    def wing_landmarks(group, sign):
        if not group:
            return None
        group_sorted = sorted(group, key=lambda v: sign * v.x)
        k = max(3, int(len(group_sorted) * 0.06))
        tip_pts = group_sorted[:k]
        tip = mathutils.Vector((
            sum(p.x for p in tip_pts) / k,
            sum(p.y for p in tip_pts) / k,
            sum(p.z for p in tip_pts) / k,
        ))
        root_pts = sorted(group, key=lambda v: sign * v.x, reverse=True)[:k]
        root = mathutils.Vector((
            sum(p.x for p in root_pts) / k,
            sum(p.y for p in root_pts) / k,
            sum(p.z for p in root_pts) / k,
        ))
        mid = (root + tip) * 0.5
        return {"root": root, "mid": mid, "tip": tip}

    wing_L = wing_landmarks(wl, -1)
    wing_R = wing_landmarks(wr, 1)

# ---------------- build armature ----------------
arm_data = bpy.data.armatures.new("UnitArmature")
arm_obj = bpy.data.objects.new("Armature", arm_data)
bpy.context.collection.objects.link(arm_obj)
bpy.context.view_layer.objects.active = arm_obj
bpy.ops.object.mode_set(mode='EDIT')
eb = arm_data.edit_bones

def add_bone(name, head, tail, parent=None):
    b = eb.new(name)
    b.head = head
    b.tail = tail
    if (b.tail - b.head).length < 1e-5:
        b.tail = b.head + mathutils.Vector((0, 0, height * 0.02))
    if parent:
        b.parent = parent
        b.use_connect = False
    return b

b_hips = add_bone("Hips", hips_pos, spine_pos)
b_spine = add_bone("Spine", spine_pos, chest_pos, b_hips)
b_chest = add_bone("Chest", chest_pos, neck_pos, b_spine)
b_neck = add_bone("Neck", neck_pos, neck_pos + (head_top - neck_pos) * 0.4, b_chest)
b_head = add_bone("Head", b_neck.tail, head_top, b_neck)

b_tail1 = add_bone("TailRoot", hips_pos, tail_root_pos, b_hips)
b_tail2 = add_bone("TailMid", tail_root_pos, tail_mid_pos, b_tail1)
b_tail3 = add_bone("TailTip", tail_mid_pos, tail_tip_pos, b_tail2)

b_fsh_l = add_bone("FrontShoulder_L", chest_pos, leg_fl["top"], b_chest)
b_fel_l = add_bone("FrontElbow_L", leg_fl["top"], leg_fl["knee"], b_fsh_l)
b_fan_l = add_bone("FrontAnkle_L", leg_fl["knee"], foot_tail(leg_fl["ankle"]), b_fel_l)

b_fsh_r = add_bone("FrontShoulder_R", chest_pos, leg_fr["top"], b_chest)
b_fel_r = add_bone("FrontElbow_R", leg_fr["top"], leg_fr["knee"], b_fsh_r)
b_fan_r = add_bone("FrontAnkle_R", leg_fr["knee"], foot_tail(leg_fr["ankle"]), b_fel_r)

b_bhi_l = add_bone("BackHip_L", hips_pos, leg_bl["top"], b_hips)
b_bkn_l = add_bone("BackKnee_L", leg_bl["top"], leg_bl["knee"], b_bhi_l)
b_ban_l = add_bone("BackAnkle_L", leg_bl["knee"], foot_tail(leg_bl["ankle"]), b_bkn_l)

b_bhi_r = add_bone("BackHip_R", hips_pos, leg_br["top"], b_hips)
b_bkn_r = add_bone("BackKnee_R", leg_br["top"], leg_br["knee"], b_bhi_r)
b_ban_r = add_bone("BackAnkle_R", leg_br["knee"], foot_tail(leg_br["ankle"]), b_bkn_r)

if has_wings and wing_L and wing_R:
    b_wr_l = add_bone("WingRoot_L", chest_pos, wing_L["root"], b_chest)
    b_wm_l = add_bone("WingMid_L", wing_L["root"], wing_L["mid"], b_wr_l)
    b_wt_l = add_bone("WingTip_L", wing_L["mid"], wing_L["tip"], b_wm_l)

    b_wr_r = add_bone("WingRoot_R", chest_pos, wing_R["root"], b_chest)
    b_wm_r = add_bone("WingMid_R", wing_R["root"], wing_R["mid"], b_wr_r)
    b_wt_r = add_bone("WingTip_R", wing_R["mid"], wing_R["tip"], b_wm_r)

bpy.ops.object.mode_set(mode='OBJECT')

bpy.ops.object.select_all(action='DESELECT')
mesh_obj.select_set(True)
arm_obj.select_set(True)
bpy.context.view_layer.objects.active = arm_obj
bpy.ops.object.parent_set(type='ARMATURE_AUTO')

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

def key_all(frame):
    for b in pb:
        b.keyframe_insert(data_path="rotation_euler", frame=frame)
        b.keyframe_insert(data_path="location", frame=frame)

def new_action(name):
    full = f"{unit_name}_{name}"
    if full in bpy.data.actions:
        bpy.data.actions.remove(bpy.data.actions[full])
    act = bpy.data.actions.new(full)
    act.use_fake_user = True
    arm_obj.animation_data_create()
    arm_obj.animation_data.action = act
    return act

fsign = head_sign  # local +X rotation sign convention helper (unused directly, kept for clarity)

new_action("Idle")
clear_pose(); key_all(0)
pb["Chest"].rotation_euler = (math.radians(-2) * head_sign, 0, 0)
pb["Hips"].location = (0, 0, 0.01)
if has_wings:
    pb["WingRoot_L"].rotation_euler = (0, 0, math.radians(3))
    pb["WingRoot_R"].rotation_euler = (0, 0, math.radians(-3))
key_all(12)
clear_pose(); key_all(24)

new_action("Move")
clear_pose(); key_all(0)

def walk_pose(frame, phase):
    swing = math.sin(phase * 2 * math.pi)
    hip_amt = math.radians(25) * head_sign
    knee_amt = math.radians(35)
    # trot gait: front-left & back-right move together, front-right & back-left opposite
    pb["FrontShoulder_L"].rotation_euler = (swing * hip_amt, 0, 0)
    pb["BackHip_R"].rotation_euler = (swing * hip_amt, 0, 0)
    pb["FrontShoulder_R"].rotation_euler = (-swing * hip_amt, 0, 0)
    pb["BackHip_L"].rotation_euler = (-swing * hip_amt, 0, 0)

    knee_fl = max(0.0, math.sin((phase + 0.5) * 2 * math.pi)) * knee_amt
    knee_br = max(0.0, math.sin((phase + 0.5) * 2 * math.pi)) * knee_amt
    knee_fr = max(0.0, math.sin(phase * 2 * math.pi)) * knee_amt
    knee_bl = max(0.0, math.sin(phase * 2 * math.pi)) * knee_amt
    pb["FrontElbow_L"].rotation_euler = (-knee_fl * head_sign, 0, 0)
    pb["BackKnee_R"].rotation_euler = (knee_br * head_sign, 0, 0)
    pb["FrontElbow_R"].rotation_euler = (-knee_fr * head_sign, 0, 0)
    pb["BackKnee_L"].rotation_euler = (knee_bl * head_sign, 0, 0)

    pb["Hips"].location = (0, 0, abs(swing) * 0.015)
    pb["TailMid"].rotation_euler = (0, 0, swing * math.radians(10))
    key_all(frame)

walk_pose(0, 0.0); walk_pose(6, 0.25); walk_pose(12, 0.5); walk_pose(18, 0.75); walk_pose(24, 1.0)

new_action("NormalAttack")
clear_pose(); key_all(0)
pb["Neck"].rotation_euler = (math.radians(-15) * head_sign, 0, 0)
pb["Head"].rotation_euler = (math.radians(-10) * head_sign, 0, 0)
key_all(4)
pb["Neck"].rotation_euler = (math.radians(20) * head_sign, 0, 0)
pb["Head"].rotation_euler = (math.radians(15) * head_sign, 0, 0)
pb["FrontShoulder_L"].rotation_euler = (math.radians(-15) * head_sign, 0, 0)
pb["FrontShoulder_R"].rotation_euler = (math.radians(-15) * head_sign, 0, 0)
key_all(9)
clear_pose(); key_all(14)

new_action("Skill")
clear_pose(); key_all(0)
pb["Chest"].rotation_euler = (math.radians(-10) * head_sign, 0, 0)
pb["Neck"].rotation_euler = (math.radians(-20) * head_sign, 0, 0)
if has_wings:
    pb["WingRoot_L"].rotation_euler = (0, 0, math.radians(35))
    pb["WingRoot_R"].rotation_euler = (0, 0, math.radians(-35))
key_all(6)
pb["Chest"].rotation_euler = (math.radians(15) * head_sign, 0, 0)
pb["Neck"].rotation_euler = (math.radians(25) * head_sign, 0, 0)
pb["Head"].rotation_euler = (math.radians(20) * head_sign, 0, 0)
if has_wings:
    pb["WingRoot_L"].rotation_euler = (0, 0, math.radians(-10))
    pb["WingRoot_R"].rotation_euler = (0, 0, math.radians(10))
key_all(12)
clear_pose(); key_all(18)

new_action("Death")
clear_pose(); key_all(0)
pb["BackKnee_L"].rotation_euler = (math.radians(45) * head_sign, 0, 0)
pb["BackKnee_R"].rotation_euler = (math.radians(45) * head_sign, 0, 0)
pb["FrontElbow_L"].rotation_euler = (math.radians(-35) * head_sign, 0, 0)
pb["FrontElbow_R"].rotation_euler = (math.radians(-35) * head_sign, 0, 0)
pb["Hips"].location = (0, 0, -height * 0.12)
key_all(8)
pb["BackKnee_L"].rotation_euler = (math.radians(75) * head_sign, 0, 0)
pb["BackKnee_R"].rotation_euler = (math.radians(75) * head_sign, 0, 0)
pb["FrontElbow_L"].rotation_euler = (math.radians(-65) * head_sign, 0, 0)
pb["FrontElbow_R"].rotation_euler = (math.radians(-65) * head_sign, 0, 0)
pb["Chest"].rotation_euler = (0, 0, math.radians(25))
pb["Neck"].rotation_euler = (0, 0, math.radians(20))
pb["Hips"].location = (0, 0, -height * 0.32)
key_all(20)
key_all(30)

clear_pose()
bpy.ops.object.mode_set(mode='OBJECT')

# ---------------- validation renders ----------------
center = mathutils.Vector((0, (min_fy + max_fy) / 2 * head_sign, (min_z + max_z) / 2))
cam_data = bpy.data.cameras.new("Cam")
cam_obj = bpy.data.objects.new("Cam", cam_data)
bpy.context.collection.objects.link(cam_obj)
dist = max(height, length) * 1.8
cam_obj.location = (center.x + dist * 0.6, center.y - dist * head_sign, center.z + height * 0.3)
direction = center - cam_obj.location
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

validation = [("Idle", 6), ("Move", 6), ("NormalAttack", 9), ("Skill", 12), ("Death", 20)]
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
