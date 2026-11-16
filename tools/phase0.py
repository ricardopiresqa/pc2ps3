#!/usr/bin/env python3
"""Cross-platform Phase 0 frontend; no proprietary SDK is bundled."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import re
import shlex
import shutil
import stat
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[1]
LOCK = ROOT / "docker/toolchain.lock.json"
STATE = ROOT / "build/phase0/environment.json"
ELF = ROOT / "packaging/ps3/pc2ps3.elf"
DIST = ROOT / "dist/ps3"
PKG_MEMBERS = ("USRDIR/EBOOT.BIN", "PARAM.SFO", "ICON0.PNG", "PIC1.PNG")


def run(args, **kwargs):
    print("+ " + shlex.join(str(arg) for arg in args), flush=True)
    try:
        return subprocess.run([str(arg) for arg in args], check=True, **kwargs)
    except subprocess.CalledProcessError as error:
        if kwargs.get("capture_output"):
            print(error.stdout or "", end="", file=sys.stderr)
            print(error.stderr or "", end="", file=sys.stderr)
        raise


def config():
    if not LOCK.is_file():
        raise ValueError("Missing docker/toolchain.lock.json. See docs/PHASE0.md.")
    data = json.loads(LOCK.read_text(encoding="utf-8"))
    if not isinstance(data, dict) or not isinstance(data.get("components"), dict):
        raise ValueError("Toolchain lock must be an object with a components object.")
    if not (ROOT / str(data.get("dockerfile") or "")).is_file() or not data.get("image_tag"):
        raise ValueError("dockerfile and image_tag must name the pinned toolchain recipe.")
    for component in ("ps3toolchain", "PSL1GHT", "SDL2_PSL1GHT"):
        entry = data.get("components", {}).get(component, {})
        if not isinstance(entry, dict):
            raise ValueError(component + ": expected a metadata object.")
        if not re.fullmatch(r"[0-9a-f]{40}", entry.get("commit") or ""):
            raise ValueError(component + ": provide the verified full source commit.")
        for field in ("repository", "license", "evidence"):
            if not isinstance(entry.get(field), str) or not entry[field].strip():
                raise ValueError(component + ": missing " + field)
    for key in ("ps3dev", "psl1ght", "ppu_cc", "ppu_readelf", "sdl2_config"):
        value = data.get(key)
        if not isinstance(value, str) or not re.fullmatch(r"/[A-Za-z0-9_./+-]+", value):
            raise ValueError(key + ": provide an absolute Linux path from the verified image.")
    for key in ("compile_flags", "link_flags"):
        flags = data.get(key)
        if not isinstance(flags, list) or not all(isinstance(flag, str) and not any(c in flag for c in "$\n\r") for flag in flags):
            raise ValueError(key + ": expected an array of flags without make expansions or newlines.")
    return data


def lock_hash():
    return hashlib.sha256(LOCK.read_bytes()).hexdigest()


def container(data, image, args, **kwargs):
    return run(["docker", "run", "--rm", "--network", "none",
                "--mount", "type=bind,source=" + str(ROOT) + ",target=/workspace",
                "--workdir", "/workspace", "--env", "PS3DEV=" + data["ps3dev"],
                "--env", "PSL1GHT=" + data["psl1ght"], "--env", "LC_ALL=C", image, *args], **kwargs)


def setup():
    data = config()
    STATE.unlink(missing_ok=True)
    run(["docker", "version"])
    # The recipe pins every source commit; the lock commits are passed so both stay in step.
    parts = data["components"]
    run(["docker", "build",
         "--build-arg", "PS3TOOLCHAIN_COMMIT=" + parts["ps3toolchain"]["commit"],
         "--build-arg", "FALLOUT_PS3_COMMIT=" + parts["SDL2_PSL1GHT"]["commit"],
         "--tag", data["image_tag"], "--file", ROOT / data["dockerfile"], ROOT / "docker"])
    image = run(["docker", "image", "inspect", "--format", "{{.Id}}", data["image_tag"]],
                capture_output=True, text=True).stdout.strip()
    if not re.fullmatch(r"sha256:[0-9a-f]{64}", image):
        raise ValueError("Docker did not return a content-addressed image ID.")
    STATE.parent.mkdir(parents=True, exist_ok=True)
    versions = {}
    for key in ("ppu_cc", "ppu_readelf", "sdl2_config"):
        result = container(data, image, [data[key], "--version"], capture_output=True, text=True)
        print(result.stdout, end="")
        versions[key] = result.stdout
    target = container(data, image, [data["ppu_cc"], "-dumpmachine"], capture_output=True, text=True).stdout.strip()
    if "powerpc64" not in target.lower() and "ppu" not in target.lower():
        raise ValueError("Compiler target does not identify a PPU/PowerPC64 toolchain: " + target)
    # Link SDL: a variable or header alone cannot stand in for a working library.
    probe = ROOT / "build/phase0/sdl-probe.c"
    probe.write_text("#include <SDL.h>\nint main(void) { SDL_Quit(); return SDL_Init(0); }\n", encoding="utf-8")
    sdl = shlex.quote(data["sdl2_config"])
    command = ("set -eu; flags=$(" + sdl + " --cflags); libs=$(" + sdl + " --libs); exec "
               + shlex.join([data["ppu_cc"], *data["compile_flags"]])
               + " $flags build/phase0/sdl-probe.c $libs " + shlex.join(data["link_flags"])
               + " -o build/phase0/sdl-probe.elf")
    container(data, image, ["/bin/sh", "-c", command])
    inspect_elf(data, image, ROOT / "build/phase0/sdl-probe.elf")
    STATE.write_text(json.dumps({"image": image, "lock_sha256": lock_hash(), "target": target,
                                 "versions": versions}, indent=2) + "\n", encoding="utf-8")
    print("Setup completed; image identity and tool output: " + str(STATE))


def environment():
    data = config()
    if not STATE.is_file():
        raise ValueError("Run setup successfully before building.")
    state = json.loads(STATE.read_text(encoding="utf-8"))
    if state.get("lock_sha256") != lock_hash():
        raise ValueError("Toolchain lock changed; run setup again.")
    if not re.fullmatch(r"sha256:[0-9a-f]{64}", state.get("image", "")):
        raise ValueError("Invalid setup image identity; run setup again.")
    return data, state["image"]


def inspect_elf(data, image, path):
    relative = path.relative_to(ROOT).as_posix()
    result = container(data, image, [data["ppu_readelf"], "-h", relative], capture_output=True, text=True)
    print(result.stdout, end="")
    if not all(text in result.stdout for text in ("ELF64", "big endian", "PowerPC64")):
        raise ValueError("Expected an ELF64 big-endian PowerPC64 binary.")
    if not re.search(r"Type:\s+EXEC\b", result.stdout):
        raise ValueError("Expected a linked executable ELF, not an object file.")
    print(hashlib.sha256(path.read_bytes()).hexdigest() + "  " + str(path))


def build_ps3():
    data, image = environment()
    # Force rebuilding after setup so an old toolchain's output cannot be reused.
    container(data, image, ["make", "--always-make", "-C", "packaging/ps3", "pc2ps3.elf"])
    inspect_elf(data, image, ELF)


def verify_pkg():
    """Structural check of the produced PKG and its unpacked layout."""
    for name in ("pc2ps3.pkg", "pc2ps3.gnpdrm.pkg"):
        data = (DIST / name).read_bytes()
        if data[:4] != b"\x7fPKG":
            raise ValueError(name + ": missing \x7fPKG magic")
        if b"PCPS00001" not in data[:0x100]:
            raise ValueError(name + ": content ID UP0001-PCPS00001 not found in header")
        print(hashlib.sha256(data).hexdigest() + "  " + name + "  " + str(len(data)) + " bytes")
    for member in PKG_MEMBERS:
        path = DIST / "layout" / member
        if not path.is_file() or path.stat().st_size == 0:
            raise ValueError("PKG layout is missing " + member)
        print(hashlib.sha256(path.read_bytes()).hexdigest() + "  layout/" + member)
    if (DIST / "layout/USRDIR/EBOOT.BIN").read_bytes()[:4] != b"SCE\x00":
        raise ValueError("EBOOT.BIN is not an SCE (SELF) container")
    if (DIST / "layout/PARAM.SFO").read_bytes()[:4] != b"\x00PSF":
        raise ValueError("PARAM.SFO is not a PSF file")


def package_ps3():
    data, image = environment()
    container(data, image, ["make", "--always-make", "-C", "packaging/ps3"])
    inspect_elf(data, image, ELF)
    verify_pkg()
    print("PKG ready: " + str(DIST / "pc2ps3.pkg") + " (RPCS3) and pc2ps3.gnpdrm.pkg (PS3HEN)")


def test():
    data, image = environment()
    if shutil.which("cmake") and shutil.which("ctest"):
        run(["cmake", "-S", ROOT, "-B", ROOT / "build/host", "-DCMAKE_BUILD_TYPE=Debug"])
        run(["cmake", "--build", ROOT / "build/host", "--config", "Debug"])
        run(["ctest", "--test-dir", ROOT / "build/host", "-C", "Debug", "--output-on-failure", "--no-tests=error"])
    else:  # No host CMake: run the same suite with the image's Linux cmake/g++.
        container(data, image, ["/bin/sh", "-c",
                                "cmake -S . -B build/host-container -DCMAKE_BUILD_TYPE=Debug && "
                                "cmake --build build/host-container -j4 && "
                                "ctest --test-dir build/host-container --output-on-failure --no-tests=error"])
    package_ps3()


def validate_clean_path(path):
    info = path.lstat()
    if stat.S_ISLNK(info.st_mode) or getattr(info, "st_file_attributes", 0) & getattr(stat, "FILE_ATTRIBUTE_REPARSE_POINT", 0x400):
        raise ValueError("Refusing symlink, junction or reparse point: " + str(path))
    resolved = path.resolve(strict=True)
    if resolved == ROOT or ROOT not in resolved.parents:
        raise ValueError("Cleanup path escapes the project: " + str(path))
    return info


def clean(confirm=False):
    targets = []
    for name, directory in (("build", True), ("build-manual", True), ("dist", True),
                            ("asset-manifest.json", False), ("prototype.ppm", False)):
        path = ROOT / name
        try:
            info = validate_clean_path(path)
        except FileNotFoundError:
            continue
        if not (stat.S_ISDIR(info.st_mode) if directory else stat.S_ISREG(info.st_mode)):
            raise ValueError("Unexpected cleanup target type: " + str(path))
        if directory:
            def walk_error(error):
                raise error
            for parent, folders, files in os.walk(path, followlinks=False, onerror=walk_error):
                for child in folders + files:
                    validate_clean_path(Path(parent) / child)
        targets.append((path, directory))
    # Validate the entire allowlist and all descendants before the first removal.
    for path, _ in targets:
        print(("REMOVE " if confirm else "WOULD REMOVE ") + str(path))
    if not confirm:
        print("Dry run; nothing removed. The person may run clean --confirm after approval.")
        return
    for path, directory in targets:
        if directory:
            shutil.rmtree(path)
        else:
            path.unlink()
    print("Cleanup completed; only the listed generated outputs were removed.")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("command", choices=("setup", "build-ps3", "package-ps3", "test", "clean"))
    parser.add_argument("--confirm", action="store_true", help="clean only: remove the validated generated outputs (default: dry run)")
    args = parser.parse_args()
    if args.confirm and args.command != "clean":
        parser.error("--confirm is supported only by clean")
    try:
        if args.command == "clean":
            clean(args.confirm)
        else:
            {"setup": setup, "build-ps3": build_ps3, "package-ps3": package_ps3,
             "test": test}[args.command]()
    except (ValueError, TypeError, OSError, subprocess.CalledProcessError) as error:
        print("ERROR: " + str(error), file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    sys.exit(main())
