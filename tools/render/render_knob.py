"""Render Marshall-style chickenhead knob filmstrip.

Output: knob_filmstrip.png — vertical strip of N frames, each FRAME_SIZE px square.
Each frame rotates the knob by SWEEP_DEG / (N-1) so frame 0 = min, frame N-1 = max.

Geometry:
- Cylindrical body, white plastic
- "Beak" wedge protruding from one side (the chickenhead pointer)
- Black indicator stripe on top surface aligned with the beak
- Small brass screw at the centre

Run: blender --background --python render_knob.py
"""

import os
import math
import tempfile
import shutil
import bpy

# ---------------------------------------------------------------------------
# Configuration
# ---------------------------------------------------------------------------
HERE = os.path.dirname(os.path.abspath(__file__))
OUT_PATH = os.path.abspath(os.path.join(HERE, "..", "..", "resources", "ui", "knob_filmstrip.png"))

FRAME_COUNT = int(os.environ.get("KNOB_FRAMES", "128"))
FRAME_SIZE = 192             # square frame, 192px (retina-ready)
# Sweep is 270° "the long way" through 12 o'clock. In Blender's CCW Z-rotation
# convention (with screen Y flipped), the long path from 7:30 (lower-left) to
# 4:30 (lower-right) via 12 o'clock requires rotating by -270° total.
SWEEP_DEG = -270.0
START_DEG = -135.0           # frame 0: beak at 7:30 (min position)

# ---------------------------------------------------------------------------
# Reset scene
# ---------------------------------------------------------------------------
bpy.ops.wm.read_factory_settings(use_empty=True)
scene = bpy.context.scene


# ---------------------------------------------------------------------------
# Materials
# ---------------------------------------------------------------------------
def new_pbr_material(name, base_color, metallic=0.0, roughness=0.5,
                     coat=0.0, coat_roughness=0.08):
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
    if coat > 0:
        for key, val in (("Coat Weight", coat), ("Coat Roughness", coat_roughness)):
            try:
                bsdf.inputs[key].default_value = val
            except KeyError:
                pass
    nt.links.new(bsdf.outputs["BSDF"], out.inputs["Surface"])
    return mat


# Boutique knob palette — moulded plastic with satin clearcoat varnish.
# Body is dark, but the coat catches HDRI reflections like a real knob.
mat_body = new_pbr_material("KnobBody", (0.014, 0.012, 0.010),
                              metallic=0.0, roughness=0.45,
                              coat=0.8, coat_roughness=0.10)
mat_indicator = new_pbr_material("KnobIndicator", (0.88, 0.82, 0.70),
                                   metallic=0.0, roughness=0.40,
                                   coat=0.6, coat_roughness=0.10)
# Cap: polished gold (matches the screws on the panel)
mat_cap = new_pbr_material("KnobCap", (0.92, 0.68, 0.22),
                             metallic=1.0, roughness=0.22)


# ---------------------------------------------------------------------------
# Build chickenhead knob — all geometry parented to a pivot empty
# ---------------------------------------------------------------------------
bpy.ops.object.empty_add(type="PLAIN_AXES", location=(0, 0, 0))
pivot = bpy.context.active_object
pivot.name = "KnobPivot"


def parent_to_pivot(obj):
    obj.parent = pivot


# --- Body: tapered cylinder, smooth, 48-sided (boutique skirted knob)
bpy.ops.mesh.primitive_cylinder_add(vertices=48, radius=0.55, depth=0.36,
                                     location=(0, 0, 0.18))
body = bpy.context.active_object
body.name = "Body"

# Squeeze the top vertices inward so the cylinder tapers slightly to a smaller
# top — classic boutique skirted-knob silhouette.
import bmesh
bm = bmesh.new()
bm.from_mesh(body.data)
for v in bm.verts:
    if v.co.z > 0.05:
        v.co.x *= 0.86
        v.co.y *= 0.86
bm.to_mesh(body.data)
bm.free()
body.data.update()

bpy.ops.object.modifier_add(type="BEVEL")
body.modifiers["Bevel"].width = 0.022
body.modifiers["Bevel"].segments = 5
bpy.ops.object.shade_smooth()
body.data.materials.append(mat_body)
parent_to_pivot(body)

# --- Cream indicator stripe on the top, +X direction (no chickenhead beak)
bpy.ops.mesh.primitive_cube_add(size=1, location=(0.24, 0, 0.365))
stripe = bpy.context.active_object
stripe.name = "Stripe"
stripe.scale = (0.32, 0.05, 0.014)
bpy.ops.object.transform_apply(scale=True)
bpy.ops.object.modifier_add(type="BEVEL")
stripe.modifiers["Bevel"].width = 0.005
stripe.modifiers["Bevel"].segments = 2
bpy.ops.object.shade_smooth()
stripe.data.materials.append(mat_indicator)
parent_to_pivot(stripe)

# --- Antique-bronze cap at the centre (echoes the panel trim)
bpy.ops.mesh.primitive_cylinder_add(vertices=28, radius=0.13, depth=0.055,
                                     location=(0, 0, 0.36 + 0.025))
cap = bpy.context.active_object
cap.name = "Cap"
bpy.ops.object.modifier_add(type="BEVEL")
cap.modifiers["Bevel"].width = 0.010
cap.modifiers["Bevel"].segments = 3
bpy.ops.object.shade_smooth()
cap.data.materials.append(mat_cap)
parent_to_pivot(cap)


# NOTE: drop shadow is composited later in Pillow rather than rendered here —
# Eevee's transparent shader on a backing plane produced a flat grey square
# instead of a soft circular falloff.


# ---------------------------------------------------------------------------
# Lighting — HDRI environment (same studio map as the panel render)
# Camera ray sees transparent so the strip stays alpha-masked.
# ---------------------------------------------------------------------------
HDRI_PATH = os.path.abspath(
    os.path.join(HERE, "_hdri", "studio_small_03_1k.hdr"))

world = bpy.data.worlds.new("W")
world.use_nodes = True
nt = world.node_tree
for n in list(nt.nodes):
    nt.nodes.remove(n)

out_node = nt.nodes.new("ShaderNodeOutputWorld")
bg_env = nt.nodes.new("ShaderNodeBackground")
bg_env.inputs["Strength"].default_value = 1.0

env_tex = nt.nodes.new("ShaderNodeTexEnvironment")
env_tex.image = bpy.data.images.load(HDRI_PATH)

env_mapping = nt.nodes.new("ShaderNodeMapping")
env_mapping.inputs["Rotation"].default_value = (0, 0, math.radians(60))

env_texcoord = nt.nodes.new("ShaderNodeTexCoord")

nt.links.new(env_texcoord.outputs["Generated"], env_mapping.inputs["Vector"])
nt.links.new(env_mapping.outputs["Vector"], env_tex.inputs["Vector"])
nt.links.new(env_tex.outputs["Color"], bg_env.inputs["Color"])
nt.links.new(bg_env.outputs["Background"], out_node.inputs["Surface"])

scene.world = world


# ---------------------------------------------------------------------------
# Camera — ortho top-down with slight tilt to expose the beak's depth
# ---------------------------------------------------------------------------
bpy.ops.object.camera_add(location=(0, -0.3, 1.4),
                          rotation=(math.radians(12), 0, 0))
cam = bpy.context.active_object
cam.data.type = "ORTHO"
cam.data.ortho_scale = 2.4   # leave margin for shadow + ensure beak never clips
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
scene.render.film_transparent = True   # transparent background

try:
    scene.eevee.taa_render_samples = 64
except AttributeError:
    pass


# ---------------------------------------------------------------------------
# Render N frames into a stable temp dir. Stitching is done by a separate
# system-Python step (Blender's bundled Python lacks PIL).
# ---------------------------------------------------------------------------
frames_dir = os.path.join(HERE, "_knob_frames")
if os.path.isdir(frames_dir):
    shutil.rmtree(frames_dir)
os.makedirs(frames_dir)
print(f"[render_knob] rendering {FRAME_COUNT} frames into {frames_dir}")

for i in range(FRAME_COUNT):
    t = i / (FRAME_COUNT - 1) if FRAME_COUNT > 1 else 0.5
    angle_deg = START_DEG + t * SWEEP_DEG
    pivot.rotation_euler = (0, 0, math.radians(angle_deg))
    scene.render.filepath = os.path.join(frames_dir, f"frame_{i:03d}.png")
    bpy.ops.render.render(write_still=True)

print(f"[render_knob] frames ready in {frames_dir} — run stitch_knob.py to assemble")
