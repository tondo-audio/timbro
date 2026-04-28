"""Render the BYPASS control — minimal: a small chrome push-button + a red LED.

Outputs:
- resources/ui/switch_strip.png  (2 frames stacked vertically)
  - Frame 0 = UP / ON  (button raised, LED red)
  - Frame 1 = DOWN / OFF (button pushed in, LED dark)

Same camera as render_knob.py so the switch shares the panel's perspective.
Run: blender --background --python render_switch.py
"""

import os
import math
import shutil
import bpy
import bmesh

HERE = os.path.dirname(os.path.abspath(__file__))
HDRI_PATH = os.path.abspath(os.path.join(HERE, "_hdri", "studio_small_03_1k.hdr"))
OUT_PATH = os.path.abspath(
    os.path.join(HERE, "..", "..", "resources", "ui", "switch_strip.png"))

FRAME_SIZE = 192
TOGGLE_X = 0.00          # toggle on the asset's vertical axis (labels align)
LED_X    = +0.26         # LED off to the right
TILT_DEG = 18.0          # how far the lever leans in each state
# Pivot at panel surface — the lever's base stays put while only the
# upper portion swings, just like a real mil-spec toggle inside its slot.
# Smaller tilt keeps the cap clear of the silkscreened ON / OFF labels.
TOGGLE_Z_OFFSET = 0.0


# ---------------------------------------------------------------------------
# Reset scene
# ---------------------------------------------------------------------------
bpy.ops.wm.read_factory_settings(use_empty=True)
scene = bpy.context.scene


def new_pbr(name, base_color, metallic=0.0, roughness=0.5,
            emission=None, emission_strength=0.0):
    mat = bpy.data.materials.new(name)
    mat.use_nodes = True
    nt = mat.node_tree
    for n in list(nt.nodes):
        nt.nodes.remove(n)
    out = nt.nodes.new("ShaderNodeOutputMaterial")
    bsdf = nt.nodes.new("ShaderNodeBsdfPrincipled")
    bsdf.inputs["Base Color"].default_value = (*base_color, 1.0)
    bsdf.inputs["Metallic"].default_value = metallic
    bsdf.inputs["Roughness"].default_value = roughness
    if emission is not None:
        for ek in ("Emission Color", "Emission"):
            try:
                bsdf.inputs[ek].default_value = (*emission, 1.0)
                break
            except KeyError:
                continue
        try:
            bsdf.inputs["Emission Strength"].default_value = emission_strength
        except KeyError:
            pass
    nt.links.new(bsdf.outputs["BSDF"], out.inputs["Surface"])
    return mat


# Materials
mat_chrome  = new_pbr("Chrome", (0.85, 0.85, 0.88), 1.0, 0.32)         # satin steel
mat_bezel   = new_pbr("Bezel",  (0.92, 0.68, 0.20), 1.0, 0.30)         # gold like screws
mat_led_on  = new_pbr("LedOn",  (0.05, 1.0, 0.12), 0.0, 0.15,
                        emission=(0.05, 1.0, 0.12), emission_strength=14.0)
mat_led_off = new_pbr("LedOff", (0.16, 0.018, 0.012), 0.0, 0.18)


# ---------------------------------------------------------------------------
# Toggle switch — fixed chrome slot bezel on the panel + pivoting lever.
# The slot is narrow on X and long on Y so it reads as a vertical cutout
# in the rendered image. The frame is NOT parented to the pivot, so it
# stays static while the lever above it tilts ±TILT_DEG.
# ---------------------------------------------------------------------------
SLOT_W_OUTER = 0.130    # X (image horizontal) — narrow
SLOT_W_INNER = 0.092
SLOT_H_OUTER = 0.290    # Y (image vertical) — long
SLOT_H_INNER = 0.250
SLOT_DEPTH   = 0.022

bpy.ops.mesh.primitive_cube_add(size=1, location=(TOGGLE_X, 0, SLOT_DEPTH / 2))
slot_outer = bpy.context.active_object
slot_outer.scale = (SLOT_W_OUTER, SLOT_H_OUTER, SLOT_DEPTH)
bpy.ops.object.transform_apply(scale=True)

bpy.ops.mesh.primitive_cube_add(size=1, location=(TOGGLE_X, 0, SLOT_DEPTH / 2))
slot_inner = bpy.context.active_object
slot_inner.scale = (SLOT_W_INNER, SLOT_H_INNER, SLOT_DEPTH * 1.6)
bpy.ops.object.transform_apply(scale=True)

bpy.ops.object.select_all(action="DESELECT")
slot_outer.select_set(True)
bpy.context.view_layer.objects.active = slot_outer
m = slot_outer.modifiers.new(name="Hole", type="BOOLEAN")
m.operation = "DIFFERENCE"
m.object = slot_inner
bpy.ops.object.modifier_apply(modifier=m.name)
bpy.data.objects.remove(slot_inner)
bpy.ops.object.modifier_add(type="BEVEL")
slot_outer.modifiers["Bevel"].width = 0.002
slot_outer.modifiers["Bevel"].segments = 2
slot_outer.name = "SlotFrame"
slot_outer.data.materials.append(mat_chrome)

# Dark interior plane — sits at panel level inside the cutout so the slot
# reads as a solid black slit even when composited over the panel image.
mat_slot_dark = new_pbr("SlotDark", (0.004, 0.003, 0.002), 0.0, 0.95)
bpy.ops.mesh.primitive_plane_add(size=1, location=(TOGGLE_X, 0, 0.0008))
slot_interior = bpy.context.active_object
slot_interior.scale = (SLOT_W_INNER * 0.98, SLOT_H_INNER * 0.98, 1)
bpy.ops.object.transform_apply(scale=True)
slot_interior.name = "SlotInterior"
slot_interior.data.materials.append(mat_slot_dark)

bpy.ops.object.empty_add(type="PLAIN_AXES", location=(TOGGLE_X, 0, TOGGLE_Z_OFFSET))
pivot = bpy.context.active_object
pivot.name = "TogglePivot"

# Lever stem — thicker so it reads at preview scale. Bottom sits inside the
# bushing so it stays hidden even when the lever tilts.
bpy.ops.mesh.primitive_cylinder_add(vertices=32, radius=0.045, depth=0.24,
                                     location=(TOGGLE_X, 0, 0.12))
stem = bpy.context.active_object
stem.name = "Stem"
bpy.ops.object.modifier_add(type="BEVEL")
stem.modifiers["Bevel"].width = 0.002
stem.modifiers["Bevel"].segments = 2
bpy.ops.object.shade_smooth()
stem.data.materials.append(mat_chrome)
stem.parent = pivot

# Cap at top of lever (chrome ball, slightly bigger than stem)
bpy.ops.mesh.primitive_uv_sphere_add(segments=32, ring_count=16,
                                      radius=0.065, location=(TOGGLE_X, 0, 0.26))
cap = bpy.context.active_object
cap.name = "Cap"
bpy.ops.object.shade_smooth()
cap.data.materials.append(mat_chrome)
cap.parent = pivot


# ---------------------------------------------------------------------------
# LED — gold bezel ring + glass dome
# ---------------------------------------------------------------------------
bpy.ops.mesh.primitive_cylinder_add(vertices=40, radius=0.095, depth=0.022,
                                     location=(LED_X, 0, 0.011))
bezel = bpy.context.active_object
bezel.name = "Bezel"

bpy.ops.mesh.primitive_cylinder_add(vertices=40, radius=0.068, depth=0.06,
                                     location=(LED_X, 0, 0.020))
inner = bpy.context.active_object

bpy.ops.object.select_all(action="DESELECT")
bezel.select_set(True)
bpy.context.view_layer.objects.active = bezel
m = bezel.modifiers.new(name="Hole", type="BOOLEAN")
m.operation = "DIFFERENCE"
m.object = inner
bpy.ops.object.modifier_apply(modifier=m.name)
bpy.data.objects.remove(inner)
bpy.ops.object.modifier_add(type="BEVEL")
bezel.modifiers["Bevel"].width = 0.004
bezel.modifiers["Bevel"].segments = 3
bpy.ops.object.shade_smooth()
bezel.data.materials.append(mat_bezel)

# Glass dome (material is set per frame for ON/OFF)
bpy.ops.mesh.primitive_uv_sphere_add(segments=40, ring_count=20, radius=0.075,
                                      location=(LED_X, 0, 0.018))
dome = bpy.context.active_object
dome.name = "Dome"
bm = bmesh.new()
bm.from_mesh(dome.data)
bmesh.ops.delete(bm,
                  geom=[v for v in bm.verts if v.co.z < 0],
                  context="VERTS")
bm.to_mesh(dome.data)
bm.free()
bpy.ops.object.shade_smooth()


# ---------------------------------------------------------------------------
# World — same HDRI as the panel/knob renders
# ---------------------------------------------------------------------------
world = bpy.data.worlds.new("W")
world.use_nodes = True
nt = world.node_tree
for n in list(nt.nodes):
    nt.nodes.remove(n)
out_node = nt.nodes.new("ShaderNodeOutputWorld")
bg = nt.nodes.new("ShaderNodeBackground")
bg.inputs["Strength"].default_value = 0.85
env = nt.nodes.new("ShaderNodeTexEnvironment")
env.image = bpy.data.images.load(HDRI_PATH)
mapping = nt.nodes.new("ShaderNodeMapping")
mapping.inputs["Rotation"].default_value = (0, 0, math.radians(60))
texcoord = nt.nodes.new("ShaderNodeTexCoord")
nt.links.new(texcoord.outputs["Generated"], mapping.inputs["Vector"])
nt.links.new(mapping.outputs["Vector"], env.inputs["Vector"])
nt.links.new(env.outputs["Color"], bg.inputs["Color"])
nt.links.new(bg.outputs["Background"], out_node.inputs["Surface"])
scene.world = world


# ---------------------------------------------------------------------------
# Camera — identical to render_knob.py for matching perspective
# ---------------------------------------------------------------------------
bpy.ops.object.camera_add(location=(0, -0.3, 1.4),
                          rotation=(math.radians(12), 0, 0))
cam = bpy.context.active_object
cam.data.type = "ORTHO"
cam.data.ortho_scale = 1.20
scene.camera = cam


# ---------------------------------------------------------------------------
# Render settings
# ---------------------------------------------------------------------------
scene.render.engine = "BLENDER_EEVEE"
scene.view_settings.view_transform = "Standard"
scene.view_settings.exposure = -1.0
scene.view_settings.gamma = 1.0

scene.render.resolution_x = FRAME_SIZE
scene.render.resolution_y = FRAME_SIZE
scene.render.resolution_percentage = 100
scene.render.image_settings.file_format = "PNG"
scene.render.image_settings.color_mode = "RGBA"
scene.render.film_transparent = True

try:
    scene.eevee.taa_render_samples = 96
except AttributeError:
    pass


# ---------------------------------------------------------------------------
# Render two states
# ---------------------------------------------------------------------------
frames_dir = os.path.join(HERE, "_switch_frames")
if os.path.isdir(frames_dir):
    shutil.rmtree(frames_dir)
os.makedirs(frames_dir)

states = [
    ("up",   -TILT_DEG, mat_led_on),    # lever tilts away from camera, LED on
    ("down",  TILT_DEG, mat_led_off),   # lever tilts toward camera, LED off
]
for name, tilt, dome_mat in states:
    pivot.rotation_euler = (math.radians(tilt), 0, 0)
    dome.data.materials.clear()
    dome.data.materials.append(dome_mat)
    scene.render.filepath = os.path.join(frames_dir, f"{name}.png")
    bpy.ops.render.render(write_still=True)

print(f"[render_switch] frames ready in {frames_dir} — run stitch_switch.py")
