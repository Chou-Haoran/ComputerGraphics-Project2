#!/usr/bin/env python3

from __future__ import annotations

import json
import os
import re
from collections import OrderedDict
from dataclasses import dataclass, field
from pathlib import Path


SCENE_DIR = Path("assets/models/the_blender_scene")
SOURCE_OBJ = SCENE_DIR / "blender.obj"
SOURCE_MATERIALMAP = SCENE_DIR / "the_blender_scene_export.materialmap"
SOURCE_SCENES = [
    SCENE_DIR / "the_blender_scene.scene",
    SCENE_DIR / "the_blender_scene_table_close.scene",
]
OUT_OBJ_DIR = SCENE_DIR / "split_objs"
OUT_MATERIALMAP_DIR = SCENE_DIR / "split_materialmaps"
OUT_MANIFEST = SCENE_DIR / "split_manifest.json"


@dataclass
class FaceCommand:
    refs: list[str]


@dataclass
class ObjectRecord:
    name: str
    commands: list[tuple[str, object]] = field(default_factory=list)
    materials: list[str] = field(default_factory=list)
    used_materials: set[str] = field(default_factory=set)
    v_indices: set[int] = field(default_factory=set)
    vt_indices: set[int] = field(default_factory=set)
    vn_indices: set[int] = field(default_factory=set)
    face_count: int = 0

    def add_material(self, material: str) -> None:
        if material not in self.used_materials:
            self.used_materials.add(material)
            self.materials.append(material)
        self.commands.append(("usemtl", material))


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


def parse_face_ref(token: str) -> tuple[int | None, int | None, int | None]:
    parts = token.split("/")
    v = int(parts[0]) if parts and parts[0] else None
    vt = int(parts[1]) if len(parts) > 1 and parts[1] else None
    vn = int(parts[2]) if len(parts) > 2 and parts[2] else None
    return v, vt, vn


def parse_obj(path: Path) -> tuple[list[str], list[str], list[str], list[ObjectRecord]]:
    vertices: list[str] = []
    texcoords: list[str] = []
    normals: list[str] = []
    objects: list[ObjectRecord] = []
    current: ObjectRecord | None = None

    def ensure_current(name: str = "SceneRoot") -> ObjectRecord:
        nonlocal current
        if current is None:
            current = ObjectRecord(name=name)
            objects.append(current)
        return current

    for raw in path.read_text(encoding="utf-8").splitlines():
        line = raw.strip()
        if not line or line.startswith("#"):
            continue
        if raw.startswith("v "):
            vertices.append(raw)
            continue
        if raw.startswith("vt "):
            texcoords.append(raw)
            continue
        if raw.startswith("vn "):
            normals.append(raw)
            continue
        if raw.startswith("o "):
            current = ObjectRecord(name=raw[2:].strip())
            objects.append(current)
            continue
        if raw.startswith("g "):
            ensure_current().commands.append(("g", raw[2:].strip()))
            continue
        if raw.startswith("s "):
            ensure_current().commands.append(("s", raw[2:].strip()))
            continue
        if raw.startswith("usemtl "):
            ensure_current().add_material(raw[7:].strip())
            continue
        if raw.startswith("f "):
            obj = ensure_current()
            refs = raw[2:].split()
            obj.face_count += 1
            for ref in refs:
                v, vt, vn = parse_face_ref(ref)
                if v is not None:
                    obj.v_indices.add(v)
                if vt is not None:
                    obj.vt_indices.add(vt)
                if vn is not None:
                    obj.vn_indices.add(vn)
            obj.commands.append(("f", FaceCommand(refs=refs)))

    return vertices, texcoords, normals, [obj for obj in objects if obj.face_count > 0]


def remap_face_ref(token: str,
                   v_map: dict[int, int],
                   vt_map: dict[int, int],
                   vn_map: dict[int, int]) -> str:
    v, vt, vn = parse_face_ref(token)
    if vt is None and vn is None:
        return str(v_map[v])
    if vn is None:
        return f"{v_map[v]}/{vt_map[vt]}"
    if vt is None:
        return f"{v_map[v]}//{vn_map[vn]}"
    return f"{v_map[v]}/{vt_map[vt]}/{vn_map[vn]}"


def write_object_obj(path: Path,
                     obj: ObjectRecord,
                     vertices: list[str],
                     texcoords: list[str],
                     normals: list[str]) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    ordered_v = sorted(obj.v_indices)
    ordered_vt = sorted(obj.vt_indices)
    ordered_vn = sorted(obj.vn_indices)
    v_map = {idx: i + 1 for i, idx in enumerate(ordered_v)}
    vt_map = {idx: i + 1 for i, idx in enumerate(ordered_vt)}
    vn_map = {idx: i + 1 for i, idx in enumerate(ordered_vn)}

    with path.open("w", encoding="utf-8") as f:
        f.write("# Auto-generated from blender.obj by split_blender_scene_obj.py\n")
        f.write("mtllib ../blender.mtl\n")
        f.write(f"o {obj.name}\n")
        for idx in ordered_v:
            f.write(vertices[idx - 1] + "\n")
        for idx in ordered_vt:
            f.write(texcoords[idx - 1] + "\n")
        for idx in ordered_vn:
            f.write(normals[idx - 1] + "\n")
        for kind, payload in obj.commands:
            if kind in {"usemtl", "g", "s"}:
                f.write(f"{kind} {payload}\n")
                continue
            if kind == "f":
                face = payload
                remapped = [remap_face_ref(ref, v_map, vt_map, vn_map) for ref in face.refs]
                f.write("f " + " ".join(remapped) + "\n")


def write_materialmap_subset(path: Path,
                             header: list[str],
                             blocks: dict[str, list[str]],
                             materials: list[str]) -> list[str]:
    path_fields = {"diffuseTex", "metallicTex", "normalTex", "bumpTex", "roughnessTex"}

    def rewrite_line(line: str) -> str:
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
        rebased = os.path.relpath(original_abs, path.parent.resolve())
        indent = line[: len(line) - len(line.lstrip())]
        return f'{indent}{field} = "{rebased}"\n'

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
                f.write(rewrite_line(line))
            if block and block[-1].strip():
                f.write("\n")
    return missing


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
    return [
        line for line in mesh_block[1:-1]
        if not line.lstrip().startswith("materialMap")
    ]


def write_split_scene(scene_path: Path,
                      objects: list[ObjectRecord]) -> Path:
    lines = scene_path.read_text(encoding="utf-8").splitlines(keepends=True)
    mesh_block, start, end = extract_mesh_block(lines)
    mesh_body = extract_mesh_body_lines(mesh_block)
    new_mesh_lines: list[str] = []
    for obj in objects:
        new_mesh_lines.append(f'mesh "split_objs/{obj.name}.obj" {{\n')
        new_mesh_lines.append(
            f'    materialMap = "split_materialmaps/{obj.name}.materialmap"\n'
        )
        new_mesh_lines.extend(mesh_body)
        new_mesh_lines.append("}\n")
        new_mesh_lines.append("\n")

    out_lines = lines[:start] + new_mesh_lines + lines[end:]
    out_path = scene_path.with_name(scene_path.stem + "_split.scene")
    out_path.write_text("".join(out_lines), encoding="utf-8")
    return out_path


def main() -> None:
    header, material_blocks = parse_materialmap_blocks(SOURCE_MATERIALMAP)
    vertices, texcoords, normals, objects = parse_obj(SOURCE_OBJ)

    if not objects:
        raise RuntimeError("no object records found in blender.obj")

    OUT_OBJ_DIR.mkdir(parents=True, exist_ok=True)
    OUT_MATERIALMAP_DIR.mkdir(parents=True, exist_ok=True)

    manifest: dict[str, object] = {
        "source_obj": SOURCE_OBJ.name,
        "object_count": len(objects),
        "objects": [],
    }
    all_missing: dict[str, list[str]] = {}

    for obj in objects:
        out_obj_path = OUT_OBJ_DIR / f"{obj.name}.obj"
        out_materialmap_path = OUT_MATERIALMAP_DIR / f"{obj.name}.materialmap"
        write_object_obj(out_obj_path, obj, vertices, texcoords, normals)
        missing = write_materialmap_subset(out_materialmap_path,
                                           header,
                                           material_blocks,
                                           obj.materials)
        if missing:
            all_missing[obj.name] = missing

        manifest["objects"].append({
            "name": obj.name,
            "obj": out_obj_path.relative_to(SCENE_DIR).as_posix(),
            "materialmap": out_materialmap_path.relative_to(SCENE_DIR).as_posix(),
            "materials": obj.materials,
            "faces": obj.face_count,
        })

    split_scenes = [write_split_scene(scene_path, objects) for scene_path in SOURCE_SCENES]
    manifest["split_scenes"] = [path.name for path in split_scenes]
    manifest["missing_material_blocks"] = all_missing
    OUT_MANIFEST.write_text(json.dumps(manifest, indent=2, ensure_ascii=False) + "\n",
                            encoding="utf-8")

    print(f"Split {SOURCE_OBJ} into {len(objects)} object OBJ files.")
    print(f"Wrote material maps to {OUT_MATERIALMAP_DIR}")
    print(f"Wrote split scene files: {', '.join(path.name for path in split_scenes)}")
    if all_missing:
        print("Missing materialmap blocks:")
        for obj_name, missing in all_missing.items():
            print(f"  {obj_name}: {', '.join(missing)}")


if __name__ == "__main__":
    main()
