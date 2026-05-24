#!/usr/bin/env python3

from __future__ import annotations

import json
import os
import re
from collections import OrderedDict
from pathlib import Path


SCENE_DIR = Path("assets/models/the_blender_scene")
SPLIT_OBJ_DIR = SCENE_DIR / "split_objs"
SOURCE_MATERIALMAP = SCENE_DIR / "the_blender_scene_export.materialmap"
SOURCE_SCENES = {
    "wide_full": SCENE_DIR / "the_blender_scene.scene",
    "closeup_full": SCENE_DIR / "the_blender_scene_table_close.scene",
}
OUT_OBJ_DIR = SCENE_DIR / "grouped_objs"
OUT_MATERIALMAP_DIR = SCENE_DIR / "grouped_materialmaps"
OUT_MANIFEST = SCENE_DIR / "grouped_manifest.json"

GROUPS: "OrderedDict[str, list[str]]" = OrderedDict([
    ("room_shell", [
        "Floor01", "Floor02", "Floor03", "Floor04", "Floor05", "Floor06", "Floor07",
        "Floor08", "Floor09", "Floor10", "Floor11", "Floor12", "Floor13", "Floor14",
        "Floor15", "Floor16", "Floor17", "Floor18",
        "Wall01", "Wall02", "Wall03", "Wall04", "Wall05", "Wall06", "Wall07",
        "Wall08", "Wall09",
    ]),
    ("curtains_and_poles", [
        "Curtain01", "Curtain02", "Pole01", "Pole02",
    ]),
    ("desk_structure", [
        "Desk", "Carbinet01", "Carbinet02", "Carbinet03",
    ]),
    ("chairs_and_carpet", [
        "Chair01", "Chair02", "Carpet01",
    ]),
    ("books_and_frames", [
        "Book1", "Book2", "Book3", "Frame01", "Frame02", "Frame03",
    ]),
    ("tabletop_devices", [
        "Player01", "Clock", "Globe01", "Phone01", "Radio01", "TV01",
    ]),
    ("lamps", [
        "Lamp01", "Lamp02", "Lampshade01",
    ]),
    ("accent_machine", [
        "Neri", "Targa", "Logo",
    ]),
])

SCENE_VARIANTS = OrderedDict([
    ("the_blender_scene_grouped.scene", {
        "source": "wide_full",
        "groups": list(GROUPS.keys()),
    }),
    ("the_blender_scene_table_close_grouped.scene", {
        "source": "closeup_full",
        "groups": list(GROUPS.keys()),
    }),
    ("the_blender_scene_table_close_essential.scene", {
        "source": "closeup_full",
        "groups": [
            "room_shell",
            "curtains_and_poles",
            "desk_structure",
            "books_and_frames",
            "tabletop_devices",
            "lamps",
        ],
    }),
])


def parse_materialmap_blocks(path: Path) -> tuple[list[str], dict[str, list[str]]]:
    lines = path.read_text(encoding="utf-8").splitlines(keepends=True)
    header: list[str] = []
    blocks: dict[str, list[str]] = OrderedDict()
    i = 0
    mtl_re = re.compile(r'^mtl\s+"([^"]+)"\s*\{\s*$')
    while i < len(lines):
        line = lines[i]
        if line.lstrip().startswith("materialMap"):
            block = [line]
            depth = line.count("{") - line.count("}")
            i += 1
            while i < len(lines) and depth > 0:
                block.append(lines[i])
                depth += lines[i].count("{") - lines[i].count("}")
                i += 1
            header.extend(block)
            continue
        match = mtl_re.match(line.strip())
        if match:
            name = match.group(1)
            block = [line]
            depth = line.count("{") - line.count("}")
            i += 1
            while i < len(lines) and depth > 0:
                block.append(lines[i])
                depth += lines[i].count("{") - lines[i].count("}")
                i += 1
            blocks[name] = block
            continue
        header.append(line)
        i += 1
    return header, blocks


def rewrite_materialmap_line(line: str, out_path: Path) -> str:
    path_fields = {"diffuseTex", "metallicTex", "normalTex", "bumpTex", "roughnessTex"}
    stripped = line.strip()
    if "=" not in stripped or '"' not in stripped:
        return line
    field, raw_value = [part.strip() for part in stripped.split("=", 1)]
    if field not in path_fields:
        return line
    match = re.match(r'^"([^"]+)"$', raw_value)
    if not match:
        return line
    original_rel = match.group(1)
    original_abs = (SCENE_DIR / original_rel).resolve(strict=False)
    rebased = os.path.relpath(original_abs, out_path.parent.resolve())
    indent = line[: len(line) - len(line.lstrip())]
    return f'{indent}{field} = "{rebased}"\n'


def write_materialmap_subset(path: Path,
                             header: list[str],
                             blocks: dict[str, list[str]],
                             materials: list[str]) -> list[str]:
    path.parent.mkdir(parents=True, exist_ok=True)
    missing: list[str] = []
    with path.open("w", encoding="utf-8") as f:
        for line in header:
            f.write(line)
        if header and not header[-1].endswith("\n"):
            f.write("\n")
        if header and header[-1].strip():
            f.write("\n")
        for material in materials:
            block = blocks.get(material)
            if block is None:
                missing.append(material)
                continue
            for line in block:
                f.write(rewrite_materialmap_line(line, path))
            if block and block[-1].strip():
                f.write("\n")
    return missing


def parse_face_ref(token: str) -> tuple[int | None, int | None, int | None]:
    parts = token.split("/")
    v = int(parts[0]) if parts and parts[0] else None
    vt = int(parts[1]) if len(parts) > 1 and parts[1] else None
    vn = int(parts[2]) if len(parts) > 2 and parts[2] else None
    return v, vt, vn


def offset_face_ref(token: str, v_off: int, vt_off: int, vn_off: int) -> str:
    v, vt, vn = parse_face_ref(token)
    v_out = v + v_off if v is not None else None
    vt_out = vt + vt_off if vt is not None else None
    vn_out = vn + vn_off if vn is not None else None
    if vt is None and vn is None:
        return str(v_out)
    if vn is None:
        return f"{v_out}/{vt_out}"
    if vt is None:
        return f"{v_out}//{vn_out}"
    return f"{v_out}/{vt_out}/{vn_out}"


def parse_split_obj(path: Path) -> dict[str, object]:
    vertices: list[str] = []
    texcoords: list[str] = []
    normals: list[str] = []
    commands: list[tuple[str, str]] = []
    materials: list[str] = []
    seen_materials: set[str] = set()

    for raw in path.read_text(encoding="utf-8").splitlines():
        if not raw or raw.startswith("#") or raw.startswith("mtllib "):
            continue
        if raw.startswith("o "):
            commands.append(("o", raw[2:].strip()))
        elif raw.startswith("g "):
            commands.append(("g", raw[2:].strip()))
        elif raw.startswith("s "):
            commands.append(("s", raw[2:].strip()))
        elif raw.startswith("usemtl "):
            material = raw[7:].strip()
            commands.append(("usemtl", material))
            if material not in seen_materials:
                seen_materials.add(material)
                materials.append(material)
        elif raw.startswith("v "):
            vertices.append(raw)
        elif raw.startswith("vt "):
            texcoords.append(raw)
        elif raw.startswith("vn "):
            normals.append(raw)
        elif raw.startswith("f "):
            commands.append(("f", raw[2:].strip()))

    return {
        "vertices": vertices,
        "texcoords": texcoords,
        "normals": normals,
        "commands": commands,
        "materials": materials,
    }


def write_group_obj(group_name: str, members: list[str]) -> tuple[list[str], dict[str, int]]:
    out_path = OUT_OBJ_DIR / f"{group_name}.obj"
    out_path.parent.mkdir(parents=True, exist_ok=True)

    group_materials: list[str] = []
    seen_materials: set[str] = set()
    stats = {"objects": 0, "faces": 0}
    v_offset = 0
    vt_offset = 0
    vn_offset = 0

    with out_path.open("w", encoding="utf-8") as f:
        f.write("# Auto-generated grouped OBJ from split_objs\n")
        f.write("mtllib ../blender.mtl\n")
        f.write(f"o {group_name}\n")

        for member in members:
            parsed = parse_split_obj(SPLIT_OBJ_DIR / f"{member}.obj")
            stats["objects"] += 1

            f.write(f"g {member}\n")
            for line in parsed["vertices"]:
                f.write(line + "\n")
            for line in parsed["texcoords"]:
                f.write(line + "\n")
            for line in parsed["normals"]:
                f.write(line + "\n")

            for kind, payload in parsed["commands"]:
                if kind == "f":
                    refs = payload.split()
                    mapped = [offset_face_ref(ref, v_offset, vt_offset, vn_offset) for ref in refs]
                    f.write("f " + " ".join(mapped) + "\n")
                    stats["faces"] += 1
                elif kind == "usemtl":
                    f.write(f"usemtl {payload}\n")
                elif kind == "s":
                    f.write(f"s {payload}\n")

            for material in parsed["materials"]:
                if material not in seen_materials:
                    seen_materials.add(material)
                    group_materials.append(material)

            v_offset += len(parsed["vertices"])
            vt_offset += len(parsed["texcoords"])
            vn_offset += len(parsed["normals"])

    return group_materials, stats


def extract_mesh_block(lines: list[str]) -> tuple[list[str], int, int]:
    start = -1
    depth = 0
    for i, line in enumerate(lines):
        if line.lstrip().startswith('mesh "'):
            start = i
            depth = line.count("{") - line.count("}")
            j = i + 1
            while j < len(lines) and depth > 0:
                depth += lines[j].count("{") - lines[j].count("}")
                j += 1
            return lines[i:j], i, j
    raise RuntimeError("mesh block not found")


def extract_mesh_body_lines(mesh_block: list[str]) -> list[str]:
    return [line for line in mesh_block[1:-1] if not line.lstrip().startswith("materialMap")]


def write_grouped_scene(out_name: str, source_scene: Path, groups: list[str]) -> None:
    lines = source_scene.read_text(encoding="utf-8").splitlines(keepends=True)
    mesh_block, start, end = extract_mesh_block(lines)
    mesh_body = extract_mesh_body_lines(mesh_block)
    new_mesh_lines: list[str] = []
    for group in groups:
        new_mesh_lines.append(f'mesh "grouped_objs/{group}.obj" {{\n')
        new_mesh_lines.append(f'    materialMap = "grouped_materialmaps/{group}.materialmap"\n')
        new_mesh_lines.extend(mesh_body)
        new_mesh_lines.append("}\n")
        new_mesh_lines.append("\n")
    out_lines = lines[:start] + new_mesh_lines + lines[end:]
    (SCENE_DIR / out_name).write_text("".join(out_lines), encoding="utf-8")


def main() -> None:
    header, material_blocks = parse_materialmap_blocks(SOURCE_MATERIALMAP)
    OUT_OBJ_DIR.mkdir(parents=True, exist_ok=True)
    OUT_MATERIALMAP_DIR.mkdir(parents=True, exist_ok=True)

    manifest: dict[str, object] = {"groups": {}, "scene_variants": SCENE_VARIANTS}
    missing_map: dict[str, list[str]] = {}

    for group_name, members in GROUPS.items():
        materials, stats = write_group_obj(group_name, members)
        missing = write_materialmap_subset(
            OUT_MATERIALMAP_DIR / f"{group_name}.materialmap",
            header,
            material_blocks,
            materials,
        )
        if missing:
            missing_map[group_name] = missing
        manifest["groups"][group_name] = {
            "members": members,
            "materials": materials,
            "faces": stats["faces"],
            "object_count": stats["objects"],
            "obj": f"grouped_objs/{group_name}.obj",
            "materialmap": f"grouped_materialmaps/{group_name}.materialmap",
        }

    for out_name, spec in SCENE_VARIANTS.items():
        write_grouped_scene(out_name, SOURCE_SCENES[spec["source"]], spec["groups"])

    manifest["missing_material_blocks"] = missing_map
    OUT_MANIFEST.write_text(json.dumps(manifest, indent=2, ensure_ascii=False) + "\n",
                            encoding="utf-8")

    print(f"Generated {len(GROUPS)} grouped OBJ files.")
    print("Scene variants:")
    for name in SCENE_VARIANTS:
        print(f"  {name}")
    if missing_map:
        print("Missing material blocks:")
        for group_name, missing in missing_map.items():
            print(f"  {group_name}: {', '.join(missing)}")


if __name__ == "__main__":
    main()
