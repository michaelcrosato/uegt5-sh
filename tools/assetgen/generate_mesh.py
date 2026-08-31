"""FOOTCANDLE parametric mesh generator (Blender headless).

Usage:
    blender -b --factory-startup -P generate_mesh.py -- <spec.json> <out.glb>

Reads a JSON spec (tools/assetgen/specs/) and emits a glTF binary. The script
is the reviewed artifact; the mesh is a build product (AGENTS.md rule 10).
Grid conventions: docs/ROADMAP.md 5.3. Units: spec is cm, Blender works in
meters, the UE importer converts back to cm.
"""

import json
import sys

import bpy


def parse_args():
    argv = sys.argv
    if "--" not in argv:
        raise SystemExit("usage: blender -b -P generate_mesh.py -- <spec.json> <out.glb>")
    tail = argv[argv.index("--") + 1:]
    if len(tail) != 2:
        raise SystemExit("expected <spec.json> <out.glb>")
    return tail[0], tail[1]


def clear_scene():
    bpy.ops.wm.read_factory_settings(use_empty=True)


def build_box(spec):
    # Spec sizes are cm; Blender meters.
    sx, sy, sz = (c / 100.0 for c in spec["size_cm"])

    mesh = bpy.data.meshes.new(spec["name"])
    obj = bpy.data.objects.new(spec["name"], mesh)
    bpy.context.scene.collection.objects.link(obj)

    if spec.get("pivot", "corner") == "corner":
        x0, y0, z0 = 0.0, 0.0, 0.0
    else:  # floor-center pivot for props
        x0, y0, z0 = -sx / 2.0, -sy / 2.0, 0.0

    verts = [
        (x0, y0, z0), (x0 + sx, y0, z0), (x0 + sx, y0 + sy, z0), (x0, y0 + sy, z0),
        (x0, y0, z0 + sz), (x0 + sx, y0, z0 + sz), (x0 + sx, y0 + sy, z0 + sz), (x0, y0 + sy, z0 + sz),
    ]
    # Outward-facing quads (CCW seen from outside).
    faces = [
        (0, 3, 2, 1),  # bottom (-Z)
        (4, 5, 6, 7),  # top (+Z)
        (0, 1, 5, 4),  # front (-Y)
        (2, 3, 7, 6),  # back (+Y)
        (0, 4, 7, 3),  # left (-X)
        (1, 2, 6, 5),  # right (+X)
    ]
    mesh.from_pydata(verts, [], faces)
    mesh.update()

    # Hard edges by default (art-direction rule): flat shading everywhere.
    for poly in mesh.polygons:
        poly.use_smooth = False

    # All UVs point at one palette cell center (palette atlas is 16 cells wide,
    # docs/ROADMAP.md 6.6). Placeholder single-cell mapping until M2's atlas.
    cell = int(spec.get("palette_cell", 0))
    u = (cell % 16 + 0.5) / 16.0
    v = 0.5
    uv_layer = mesh.uv_layers.new(name="UVMap")
    for loop_index in range(len(mesh.loops)):
        uv_layer.data[loop_index].uv = (u, v)

    return obj


BUILDERS = {"box": build_box}


def main():
    spec_path, out_path = parse_args()
    with open(spec_path, "r", encoding="utf-8") as handle:
        spec = json.load(handle)

    if not spec["name"].startswith("SM_"):
        raise SystemExit(f"naming: {spec['name']} must start with SM_")

    clear_scene()
    builder = BUILDERS.get(spec["kind"])
    if builder is None:
        raise SystemExit(f"unknown kind '{spec['kind']}'")
    obj = builder(spec)

    bpy.ops.object.select_all(action="DESELECT")
    obj.select_set(True)
    bpy.context.view_layer.objects.active = obj

    bpy.ops.export_scene.gltf(
        filepath=out_path,
        export_format="GLB",
        use_selection=True,
        export_yup=True,
        export_apply=True,
        export_materials="NONE",
    )
    print(f"[assetgen] wrote {out_path}")


if __name__ == "__main__":
    main()
