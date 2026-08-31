"""FOOTCANDLE UE import step - runs inside UnrealEditor-Cmd.

Usage:
    UnrealEditor-Cmd.exe <project> -run=pythonscript
        -script="import_mesh.py <mesh.glb> <name> <dest_path>"

Imports the validated GLB as a StaticMesh, enables Nanite (kit policy,
docs/ROADMAP.md 6.5), adds simple box collision, saves the asset.
Generated assets land under /Game/Generated/ (gitignored - regenerable).
"""

import sys

import unreal


def main():
    glb_path, name, dest = sys.argv[1], sys.argv[2], sys.argv[3]

    task = unreal.AssetImportTask()
    task.filename = glb_path
    task.destination_path = dest
    task.destination_name = name
    task.automated = True
    task.replace_existing = True
    task.save = False

    unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])

    asset_path = f"{dest}/{name}"
    mesh = unreal.load_asset(asset_path)
    if mesh is None:
        # Interchange may nest the asset under a folder named after the source.
        found = unreal.EditorAssetLibrary.list_assets(dest, recursive=True)
        candidates = [p for p in found if p.split(".")[-1] == name]
        if candidates:
            asset_path = candidates[0]
            mesh = unreal.load_asset(asset_path)
    if mesh is None:
        unreal.log_error(f"[import] asset not found after import: {asset_path}")
        sys.exit(1)

    if isinstance(mesh, unreal.StaticMesh):
        nanite = mesh.get_editor_property("nanite_settings")
        nanite.enabled = True
        mesh.set_editor_property("nanite_settings", nanite)

        # Commandlet mode: editor subsystems may be absent - fall back through
        # the older function library, then direct BodySetup authoring.
        sm_subsystem = unreal.get_editor_subsystem(unreal.StaticMeshEditorSubsystem)
        if sm_subsystem is not None:
            sm_subsystem.remove_collisions(mesh)
            sm_subsystem.add_simple_collisions(mesh, unreal.ScriptCollisionShapeType.BOX)
        elif hasattr(unreal, "EditorStaticMeshLibrary"):
            unreal.EditorStaticMeshLibrary.remove_collisions(mesh)
            unreal.EditorStaticMeshLibrary.add_simple_collisions(
                mesh, unreal.ScriptingCollisionShapeType.BOX)
        else:
            bounds = mesh.get_bounding_box()
            extent = (bounds.max - bounds.min) * 0.5
            center = (bounds.max + bounds.min) * 0.5
            box = unreal.BoxElem()
            box.set_editor_property("center", center)
            box.set_editor_property("x", extent.x * 2.0)
            box.set_editor_property("y", extent.y * 2.0)
            box.set_editor_property("z", extent.z * 2.0)
            body = mesh.get_editor_property("body_setup")
            agg = body.get_editor_property("agg_geom")
            agg.set_editor_property("box_elems", [box])
            body.set_editor_property("agg_geom", agg)
            mesh.set_editor_property("body_setup", body)

    saved = unreal.EditorAssetLibrary.save_asset(asset_path, only_if_is_dirty=False)
    unreal.log(f"[import] {'OK' if saved else 'SAVE FAILED'}: {asset_path}")
    if not saved:
        sys.exit(1)


if __name__ == "__main__":
    main()
