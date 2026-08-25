import bpy, sys, math
import mathutils

argv = sys.argv[sys.argv.index("--") + 1:]
fbx_path = argv[0]
out_blend = argv[1]
out_png = argv[2]
out_fbx = argv[3]

bpy.ops.wm.read_factory_settings(use_empty=True)
bpy.ops.import_scene.fbx(filepath=fbx_path)

mesh_obj = [o for o in bpy.data.objects if o.type == 'MESH'][0]
mat = mesh_obj.matrix_world
verts = [mat @ v.co for v in mesh_obj.data.vertices]
# bake transform into mesh data so armature/mesh share identity transform
bpy.ops.object.select_all(action='DESELECT')
mesh_obj.select_set(True)
bpy.context.view_layer.objects.active = mesh_obj
bpy.ops.object.transform_apply(location=True, rotation=True, scale=True)

zs = [v.z for v in verts]
min_z, max_z = min(zs), max(zs)
height = max_z - min_z

N = 40
def band_width(i):
    z0 = min_z + height * i / N
    z1 = min_z + height * (i + 1) / N
    pts = [v for v in verts if z0 <= v.z < z1]
    if not pts:
        return 0.0
    xs = [p.x for p in pts]
    return max(xs) - min(xs)

widths = [band_width(i) for i in range(N)]

best_jump = -1
hip_cut_band = N // 4
for i in range(2, N // 2):
    jump = widths[i] - widths[i - 1]
    if jump > best_jump:
        best_jump = jump
        hip_cut_band = i
hip_cut = min_z + height * hip_cut_band / N

neck_band = None
neck_w = 1e9
for i in range(int(N * 0.5), int(N * 0.88)):
    if widths[i] < neck_w:
        neck_w = widths[i]
        neck_band = i
head_cut = min_z + height * neck_band / N

leg_verts = [v for v in verts if v.z < hip_cut]
cx = sum(v.x for v in leg_verts) / len(leg_verts)
left_leg = [v for v in leg_verts if v.x < cx]
right_leg = [v for v in leg_verts if v.x >= cx]

def leg_landmarks(group):
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
    ankle = centroid_near(zmin, zmin + (zmax - zmin) * 0.20)
    knee = centroid_near(zmin + (zmax - zmin) * 0.40, zmin + (zmax - zmin) * 0.55)
    hip = centroid_near(zmax - (zmax - zmin) * 0.15, zmax)
    toe = centroid_near(zmin, zmin + (zmax - zmin) * 0.05)
    return {"ankle": ankle, "knee": knee, "hip": hip, "toe": toe}

leg_L = leg_landmarks(left_leg)
leg_R = leg_landmarks(right_leg)

torso_arm_verts = [v for v in verts if hip_cut <= v.z < head_cut]
cx2 = sum(v.x for v in torso_arm_verts) / len(torso_arm_verts)
left_arm = [v for v in torso_arm_verts if v.x < cx2]
right_arm = [v for v in torso_arm_verts if v.x >= cx2]

def arm_landmarks(group, sign):
    group_sorted = sorted(group, key=lambda v: sign * (v.x - cx2), reverse=True)
    k = max(3, int(len(group_sorted) * 0.08))
    hand_pts = group_sorted[:k]
    hand = mathutils.Vector((
        sum(p.x for p in hand_pts) / k,
        sum(p.y for p in hand_pts) / k,
        sum(p.z for p in hand_pts) / k,
    ))
    shoulder_z = head_cut - (head_cut - hip_cut) * 0.05
    band_pts = [v for v in group if abs(v.z - shoulder_z) < (head_cut - hip_cut) * 0.08]
    if not band_pts:
        band_pts = group
    shoulder_x_outer = sign * max(sign * (v.x - cx2) for v in band_pts) + cx2
    shoulder = mathutils.Vector((
        cx2 + (shoulder_x_outer - cx2) * 0.5,
        sum(p.y for p in band_pts) / len(band_pts),
        shoulder_z,
    ))
    elbow = (shoulder + hand) * 0.5
    return {"shoulder": shoulder, "elbow": elbow, "hand": hand}

arm_L = arm_landmarks(left_arm, -1)
arm_R = arm_landmarks(right_arm, 1)

hips_pos = mathutils.Vector((0, (leg_L["hip"].y + leg_R["hip"].y) / 2, hip_cut))
neck_pos = mathutils.Vector((0, 0, head_cut))
chest_pos = mathutils.Vector((0, 0, head_cut - (head_cut - hip_cut) * 0.1))
spine_pos = mathutils.Vector((0, 0, hip_cut + (head_cut - hip_cut) * 0.4))
head_top = mathutils.Vector((0, 0, max_z))

# ---- build armature ----
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
    if parent:
        b.parent = parent
        b.use_connect = False
    return b

b_hips = add_bone("Hips", hips_pos, spine_pos)
b_spine = add_bone("Spine", spine_pos, chest_pos, b_hips)
b_chest = add_bone("Chest", chest_pos, neck_pos, b_spine)
b_neck = add_bone("Neck", neck_pos, neck_pos + mathutils.Vector((0, 0, (head_top.z - neck_pos.z) * 0.3)), b_chest)
b_head = add_bone("Head", b_neck.tail, head_top, b_neck)

b_sh_l = add_bone("Shoulder_L", chest_pos, arm_L["shoulder"], b_chest)
b_el_l = add_bone("Elbow_L", arm_L["shoulder"], arm_L["elbow"], b_sh_l)
b_ha_l = add_bone("Hand_L", arm_L["elbow"], arm_L["hand"], b_el_l)
b_ha_l.tail = arm_L["hand"] + (arm_L["hand"] - arm_L["elbow"]) * 0.4

b_sh_r = add_bone("Shoulder_R", chest_pos, arm_R["shoulder"], b_chest)
b_el_r = add_bone("Elbow_R", arm_R["shoulder"], arm_R["elbow"], b_sh_r)
b_ha_r = add_bone("Hand_R", arm_R["elbow"], arm_R["hand"], b_el_r)
b_ha_r.tail = arm_R["hand"] + (arm_R["hand"] - arm_R["elbow"]) * 0.4

def foot_tail(ankle):
    # extend the shin/ankle bone all the way down to true ground level so it
    # dominates heat-weight diffusion for the whole foot mesh (a too-short
    # ankle bone loses the diffusion competition to the thigh/knee bone above it,
    # causing the foot to be bound to the wrong bone and tear off when posed).
    return mathutils.Vector((ankle.x, ankle.y, min_z))

b_hip_l = add_bone("Hip_L", hips_pos, leg_L["hip"], b_hips)
b_kn_l = add_bone("Knee_L", leg_L["hip"], leg_L["knee"], b_hip_l)
b_an_l = add_bone("Ankle_L", leg_L["knee"], foot_tail(leg_L["ankle"]), b_kn_l)

b_hip_r = add_bone("Hip_R", hips_pos, leg_R["hip"], b_hips)
b_kn_r = add_bone("Knee_R", leg_R["hip"], leg_R["knee"], b_hip_r)
b_an_r = add_bone("Ankle_R", leg_R["knee"], foot_tail(leg_R["ankle"]), b_kn_r)

bpy.ops.object.mode_set(mode='OBJECT')

# ---- parent mesh with automatic weights ----
bpy.ops.object.select_all(action='DESELECT')
mesh_obj.select_set(True)
arm_obj.select_set(True)
bpy.context.view_layer.objects.active = arm_obj
bpy.ops.object.parent_set(type='ARMATURE_AUTO')

# save blend for later reuse
bpy.ops.wm.save_as_mainfile(filepath=out_blend)

# ---- pose test: bend elbows and lift a leg to check deformation ----
bpy.ops.object.mode_set(mode='POSE')
pose = arm_obj.pose
pose.bones["Elbow_L"].rotation_mode = 'XYZ'
pose.bones["Elbow_L"].rotation_euler = (math.radians(20), 0, 0)
pose.bones["Elbow_R"].rotation_mode = 'XYZ'
pose.bones["Elbow_R"].rotation_euler = (math.radians(20), 0, 0)
pose.bones["Knee_L"].rotation_mode = 'XYZ'
pose.bones["Knee_L"].rotation_euler = (math.radians(15), 0, 0)
bpy.ops.object.mode_set(mode='OBJECT')

center = mathutils.Vector((0, 0, (min_z + max_z) / 2))
radius = height
cam_data = bpy.data.cameras.new("Cam")
cam_obj = bpy.data.objects.new("Cam", cam_data)
bpy.context.collection.objects.link(cam_obj)
dist = radius * 1.7
cam_obj.location = (center.x + dist * 0.6, center.y - dist, center.z + height * 0.05)
cam_obj.rotation_euler = (math.radians(85), 0, math.radians(25))
bpy.context.scene.camera = cam_obj

light_data = bpy.data.lights.new("Sun", type='SUN')
light_data.energy = 3.0
light_obj = bpy.data.objects.new("Sun", light_data)
bpy.context.collection.objects.link(light_obj)
light_obj.rotation_euler = (math.radians(50), 0, math.radians(20))

scene = bpy.context.scene
scene.render.resolution_x = 700
scene.render.resolution_y = 700
scene.render.filepath = out_png
scene.render.engine = 'BLENDER_EEVEE_NEXT' if hasattr(bpy.types, 'SceneEEVEE') else 'BLENDER_EEVEE'
bpy.ops.render.render(write_still=True)
print("RENDER DONE:", out_png)

# export rigged (bind-pose) fbx: reset pose to rest before export
bpy.context.view_layer.objects.active = arm_obj
bpy.ops.object.mode_set(mode='POSE')
bpy.ops.pose.select_all(action='SELECT')
bpy.ops.pose.transforms_clear()
bpy.ops.object.mode_set(mode='OBJECT')

bpy.ops.object.select_all(action='DESELECT')
mesh_obj.select_set(True)
arm_obj.select_set(True)
bpy.context.view_layer.objects.active = arm_obj
bpy.ops.export_scene.fbx(filepath=out_fbx, use_selection=True, add_leaf_bones=False)
print("FBX EXPORT DONE:", out_fbx)
