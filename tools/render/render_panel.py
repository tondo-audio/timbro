"""Render Marshall-style faceplate base (no typography).

Output: panel_base.png at 920x1200 (2x retina for 460x600 window).
- Black tolex border around the edges
- Brass/gold brushed faceplate in the centre
- Subtle bevel between tolex and faceplate (gold piping)
- Realistic lighting via Eevee Next

Run: blender --background --python render_panel.py
"""

import os
import sys
import math
import bpy
import bmesh

# ---------------------------------------------------------------------------
# Configuration
# ---------------------------------------------------------------------------
OUT_PATH = os.path.join(
    os.path.dirname(os.path.abspath(__file__)),
    "..", "..", "resources", "ui", "panel_base.png",
)
OUT_PATH = os.path.abspath(OUT_PATH)

# Plugin window is 720x280 (horizontal amp-head form factor). Render at 2x.
RES_X, RES_Y = 1440, 560

# Panel real-world dimensions (Blender units = meters).
# Aspect ratio matches 720:280 ≈ 2.57:1 (typical boutique amp head face)
PANEL_W = 0.72
PANEL_H = 0.28

# Walnut wood border around the matte-black faceplate
TOLEX_BORDER = 0.025

# ---------------------------------------------------------------------------
# Reset scene
# ---------------------------------------------------------------------------
bpy.ops.wm.read_factory_settings(use_empty=True)
scene = bpy.context.scene
scene.unit_settings.system = "METRIC"


# ---------------------------------------------------------------------------
# Helpers
# ---------------------------------------------------------------------------
def add_plane(name, w, h, location=(0, 0, 0)):
    bpy.ops.mesh.primitive_plane_add(size=1, location=location)
    obj = bpy.context.active_object
    obj.name = name
    obj.scale = (w, h, 1)
    bpy.ops.object.transform_apply(location=False, rotation=False, scale=True)
    return obj


def new_material(name):
    mat = bpy.data.materials.new(name)
    mat.use_nodes = True
    return mat


def clear_nodes(mat):
    nt = mat.node_tree
    for n in list(nt.nodes):
        nt.nodes.remove(n)
    return nt


# ---------------------------------------------------------------------------
# Walnut wood backplate — Wood048 PBR maps from ambientCG (CC0), tinted dark
# walnut so the lighter ash texture reads as dark stained boutique-amp wood.
# ---------------------------------------------------------------------------
wood = add_plane("Wood", PANEL_W, PANEL_H, location=(0, 0, 0))

WOOD_DIR = os.path.abspath(
    os.path.join(os.path.dirname(os.path.abspath(__file__)),
                 "_textures", "wood"))
WOOD_COLOR = os.path.join(WOOD_DIR, "Wood048_2K-PNG_Color.png")
WOOD_ROUGH = os.path.join(WOOD_DIR, "Wood048_2K-PNG_Roughness.png")
WOOD_NORMAL = os.path.join(WOOD_DIR, "Wood048_2K-PNG_NormalGL.png")

mat_wood = new_material("Wood")
nt = clear_nodes(mat_wood)
out = nt.nodes.new("ShaderNodeOutputMaterial")
bsdf = nt.nodes.new("ShaderNodeBsdfPrincipled")

tex_coord = nt.nodes.new("ShaderNodeTexCoord")
mapping = nt.nodes.new("ShaderNodeMapping")
mapping.inputs["Scale"].default_value = (1.4, 1.4, 1.0)
mapping.inputs["Rotation"].default_value = (0, 0, math.radians(90))   # grain runs vertical

# BaseColor — texture × dark-walnut tint
color_tex = nt.nodes.new("ShaderNodeTexImage")
color_tex.image = bpy.data.images.load(WOOD_COLOR)

tint_mix = nt.nodes.new("ShaderNodeMixRGB")
tint_mix.blend_type = "MULTIPLY"
tint_mix.inputs["Fac"].default_value = 1.0
tint_rgb = nt.nodes.new("ShaderNodeRGB")
tint_rgb.outputs[0].default_value = (0.28, 0.16, 0.075, 1.0)

# Roughness map (Non-Color)
rough_tex = nt.nodes.new("ShaderNodeTexImage")
rough_tex.image = bpy.data.images.load(WOOD_ROUGH)
rough_tex.image.colorspace_settings.name = "Non-Color"

# Normal map (Non-Color, GL convention)
normal_tex = nt.nodes.new("ShaderNodeTexImage")
normal_tex.image = bpy.data.images.load(WOOD_NORMAL)
normal_tex.image.colorspace_settings.name = "Non-Color"
normal_map = nt.nodes.new("ShaderNodeNormalMap")
normal_map.inputs["Strength"].default_value = 0.8

nt.links.new(tex_coord.outputs["UV"], mapping.inputs["Vector"])
for t in (color_tex, rough_tex, normal_tex):
    nt.links.new(mapping.outputs["Vector"], t.inputs["Vector"])

nt.links.new(color_tex.outputs["Color"], tint_mix.inputs[1])
nt.links.new(tint_rgb.outputs["Color"], tint_mix.inputs[2])
nt.links.new(tint_mix.outputs["Color"], bsdf.inputs["Base Color"])
nt.links.new(rough_tex.outputs["Color"], bsdf.inputs["Roughness"])
nt.links.new(normal_tex.outputs["Color"], normal_map.inputs["Color"])
nt.links.new(normal_map.outputs["Normal"], bsdf.inputs["Normal"])
nt.links.new(bsdf.outputs["BSDF"], out.inputs["Surface"])
wood.data.materials.append(mat_wood)


# ---------------------------------------------------------------------------
# Black anodized faceplate — matte, slight micro-texture
# ---------------------------------------------------------------------------
fw = PANEL_W - 2 * TOLEX_BORDER
fh = PANEL_H - 2 * TOLEX_BORDER
faceplate = add_plane("Faceplate", fw, fh, location=(0, 0, 0.0015))

mat_black = new_material("BlackPlate")
nt = clear_nodes(mat_black)
out = nt.nodes.new("ShaderNodeOutputMaterial")
bsdf = nt.nodes.new("ShaderNodeBsdfPrincipled")
# Deep matte black with the faintest brown undertone (boutique anodized)
bsdf.inputs["Base Color"].default_value = (0.012, 0.010, 0.008, 1.0)
bsdf.inputs["Metallic"].default_value = 0.0
bsdf.inputs["Roughness"].default_value = 0.92

# Fine micro-noise so the surface isn't dead flat under the key light
tex_coord = nt.nodes.new("ShaderNodeTexCoord")
mapping = nt.nodes.new("ShaderNodeMapping")
mapping.inputs["Scale"].default_value = (250.0, 250.0, 1.0)
noise = nt.nodes.new("ShaderNodeTexNoise")
noise.inputs["Scale"].default_value = 6.0
noise.inputs["Detail"].default_value = 5.0
noise.inputs["Roughness"].default_value = 0.5
bump = nt.nodes.new("ShaderNodeBump")
bump.inputs["Strength"].default_value = 0.10
bump.inputs["Distance"].default_value = 0.0004

nt.links.new(tex_coord.outputs["Generated"], mapping.inputs["Vector"])
nt.links.new(mapping.outputs["Vector"], noise.inputs["Vector"])
nt.links.new(noise.outputs["Fac"], bump.inputs["Height"])
nt.links.new(bump.outputs["Normal"], bsdf.inputs["Normal"])
nt.links.new(bsdf.outputs["BSDF"], out.inputs["Surface"])

faceplate.data.materials.append(mat_black)


# ---------------------------------------------------------------------------
# Antique-bronze trim (thin rim where wood meets the black faceplate)
# ---------------------------------------------------------------------------
piping = add_plane("Trim", fw + 0.004, fh + 0.004, location=(0, 0, 0.0008))
mat_piping = new_material("Trim")
nt = clear_nodes(mat_piping)
out = nt.nodes.new("ShaderNodeOutputMaterial")
bsdf = nt.nodes.new("ShaderNodeBsdfPrincipled")
# Aged bronze — muted, slightly desaturated
bsdf.inputs["Base Color"].default_value = (0.42, 0.28, 0.13, 1.0)
bsdf.inputs["Metallic"].default_value = 1.0
bsdf.inputs["Roughness"].default_value = 0.55
nt.links.new(bsdf.outputs["BSDF"], out.inputs["Surface"])
piping.data.materials.append(mat_piping)


# ---------------------------------------------------------------------------
# Phillips screws — real 3D so they share the panel's lighting
# ---------------------------------------------------------------------------
mat_screw = new_material("Screw")
nt = clear_nodes(mat_screw)
out = nt.nodes.new("ShaderNodeOutputMaterial")
bsdf = nt.nodes.new("ShaderNodeBsdfPrincipled")
# Polished gold — warm, saturated, only mildly worn. Roughness intentionally
# higher than a mirror finish so the diffuse body colour reads as gold instead
# of being washed out by the bright HDRI specular reflection.
bsdf.inputs["Base Color"].default_value = (0.95, 0.55, 0.10, 1.0)
bsdf.inputs["Metallic"].default_value = 1.0
bsdf.inputs["Roughness"].default_value = 0.45
nt.links.new(bsdf.outputs["BSDF"], out.inputs["Surface"])


def add_screw(x, y, head_radius=0.0080, head_depth=0.0022, slot_angle_deg=0.0):
    """Phillips-head screw: domed cap with a real cross slot cut by booleans."""
    head_z = head_depth / 2 + 0.0005   # rest just above the wood
    bpy.ops.mesh.primitive_cylinder_add(vertices=48, radius=head_radius,
                                         depth=head_depth, location=(x, y, head_z))
    head = bpy.context.active_object
    head.name = f"Screw_{x:.3f}_{y:.3f}"

    # Dome the top: inset the top face and raise the inset face along its
    # normal. Combined with smooth shading + subsurf this turns the flat cap
    # into a convex Phillips button — without this the head reads as concave
    # because the cross slot cut into a flat disc looks like a depression.
    DOME_RISE = head_depth * 0.55
    bm = bmesh.new()
    bm.from_mesh(head.data)
    top_z = max(f.calc_center_median().z for f in bm.faces)
    top_faces = [f for f in bm.faces if abs(f.calc_center_median().z - top_z) < 1e-5]
    bmesh.ops.inset_region(bm, faces=top_faces,
                            thickness=head_radius * 0.55, depth=DOME_RISE)
    bm.to_mesh(head.data)
    bm.free()
    head.data.update()

    # Boolean cutters for the cross slot — placed above the (now raised) dome
    # centre so the difference produces a recessed groove that catches shadow.
    slot_l = head_radius * 1.6
    slot_w = head_radius * 0.20
    slot_h = head_depth * 0.55
    z_cut = head_z + head_depth / 2 + DOME_RISE - slot_h * 0.30
    cutters = []
    for sx, sy in [(slot_l, slot_w), (slot_w, slot_l)]:
        bpy.ops.mesh.primitive_cube_add(size=1, location=(x, y, z_cut))
        c = bpy.context.active_object
        c.scale = (sx, sy, slot_h)
        bpy.ops.object.transform_apply(scale=True)
        cutters.append(c)

    # Apply booleans to the head
    bpy.ops.object.select_all(action="DESELECT")
    head.select_set(True)
    bpy.context.view_layer.objects.active = head
    for i, c in enumerate(cutters):
        m = head.modifiers.new(name=f"Cut{i}", type="BOOLEAN")
        m.operation = "DIFFERENCE"
        m.object = c
        bpy.ops.object.modifier_apply(modifier=m.name)
    for c in cutters:
        bpy.data.objects.remove(c)

    # Round the head's outer rim + subdivision surface to smooth the dome step
    # into a continuous curve.
    bpy.ops.object.select_all(action="DESELECT")
    head.select_set(True)
    bpy.context.view_layer.objects.active = head
    bpy.ops.object.modifier_add(type="BEVEL")
    head.modifiers["Bevel"].width = 0.0005
    head.modifiers["Bevel"].segments = 3
    bpy.ops.object.modifier_add(type="SUBSURF")
    head.modifiers["Subdivision"].levels = 2
    head.modifiers["Subdivision"].render_levels = 2
    bpy.ops.object.shade_smooth()

    # Random slot rotation per screw so they don't look identical
    head.rotation_euler.z = math.radians(slot_angle_deg)

    # Boolean operations can leave behind empty material slots inherited from
    # the (untextured) cutter cubes — without this cleanup, the screw faces
    # default to slot 0 (None / default white) and our gold material is never
    # actually applied.
    head.data.materials.clear()
    head.data.materials.append(mat_screw)
    for poly in head.data.polygons:
        poly.material_index = 0


# Inset matches compose_panel.py (~25 logical px from edge, on the wood)
SCREW_INSET = 0.022
sx = PANEL_W / 2 - SCREW_INSET
sy = PANEL_H / 2 - SCREW_INSET
add_screw(-sx,  sy, slot_angle_deg=22.0)
add_screw( sx,  sy, slot_angle_deg=-15.0)
add_screw(-sx, -sy, slot_angle_deg=-32.0)
add_screw( sx, -sy, slot_angle_deg=8.0)


# Bypass lamp removed — replaced by a separate 3D toggle-switch asset
# (render_switch.py) that JUCE composites on top of the panel.


# ---------------------------------------------------------------------------
# Lighting — HDRI environment (studio_small_03 from Poly Haven, CC0)
# Replaces the previous three area lights so metals reflect a real photo studio
# instead of three artificial "spots". Camera ray still sees solid black behind
# the panel — the HDRI is used purely for illumination/reflection.
# ---------------------------------------------------------------------------
HDRI_PATH = os.path.abspath(
    os.path.join(os.path.dirname(os.path.abspath(__file__)),
                 "_hdri", "studio_small_03_1k.hdr"))

world = bpy.data.worlds.new("W")
world.use_nodes = True
nt = world.node_tree
for n in list(nt.nodes):
    nt.nodes.remove(n)

out_node = nt.nodes.new("ShaderNodeOutputWorld")
mix = nt.nodes.new("ShaderNodeMixShader")

bg_camera = nt.nodes.new("ShaderNodeBackground")
bg_camera.inputs["Color"].default_value = (0.012, 0.010, 0.008, 1.0)
bg_camera.inputs["Strength"].default_value = 1.0

bg_env = nt.nodes.new("ShaderNodeBackground")
bg_env.inputs["Strength"].default_value = 0.55

env_tex = nt.nodes.new("ShaderNodeTexEnvironment")
env_tex.image = bpy.data.images.load(HDRI_PATH)

env_mapping = nt.nodes.new("ShaderNodeMapping")
env_mapping.inputs["Rotation"].default_value = (0, 0, math.radians(60))

env_texcoord = nt.nodes.new("ShaderNodeTexCoord")
light_path = nt.nodes.new("ShaderNodeLightPath")

nt.links.new(env_texcoord.outputs["Generated"], env_mapping.inputs["Vector"])
nt.links.new(env_mapping.outputs["Vector"], env_tex.inputs["Vector"])
nt.links.new(env_tex.outputs["Color"], bg_env.inputs["Color"])
# Mix factor = 1 means camera ray; we want HDRI for reflections (non-camera)
# and solid colour for camera. So input1 = HDRI, input2 = solid, fac=isCamera.
nt.links.new(light_path.outputs["Is Camera Ray"], mix.inputs["Fac"])
nt.links.new(bg_env.outputs["Background"], mix.inputs[1])
nt.links.new(bg_camera.outputs["Background"], mix.inputs[2])
nt.links.new(mix.outputs["Shader"], out_node.inputs["Surface"])

scene.world = world


# ---------------------------------------------------------------------------
# Camera — orthographic, looking straight down
# ---------------------------------------------------------------------------
bpy.ops.object.camera_add(location=(0, 0, 1.0), rotation=(0, 0, 0))
cam = bpy.context.active_object
cam.data.type = "ORTHO"
cam.data.ortho_scale = PANEL_W  # frame the full panel width (now > height)
scene.camera = cam


# ---------------------------------------------------------------------------
# Render settings
# ---------------------------------------------------------------------------
scene.render.engine = "BLENDER_EEVEE"

# Color management — Standard view, slight pulldown so the matte black holds
# its depth while the wood grain still reads.
scene.view_settings.view_transform = "Standard"
scene.view_settings.exposure = -1.3
scene.view_settings.gamma = 1.0

scene.render.resolution_x = RES_X
scene.render.resolution_y = RES_Y
scene.render.resolution_percentage = 100
scene.render.image_settings.file_format = "PNG"
scene.render.image_settings.color_mode = "RGBA"
scene.render.image_settings.compression = 15
scene.render.film_transparent = False
scene.render.filepath = OUT_PATH

# Eevee quality boosts available in 5.x
try:
    scene.eevee.taa_render_samples = 64
    scene.eevee.use_raytracing = True
except AttributeError:
    pass

print(f"[render_panel] writing {OUT_PATH}")
bpy.ops.render.render(write_still=True)
print(f"[render_panel] done")
