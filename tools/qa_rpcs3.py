#!/usr/bin/env python3
"""Automated RPCS3 smoke for the PC2PS3 PKG layout (QA autoplay).

Copies dist/ps3/layout into <rpcs3>/dev_hdd0/game/PCPS00001, creates
USRDIR/qa_autoplay, boots EBOOT.BIN with --no-gui, waits for the game to quit
on its own and prints the [pc2ps3] TTY lines plus which expected markers were
seen. Observations only; exit 1 just means a marker was missing or a timeout.
"""
import argparse
import shutil
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
LAYOUT = ROOT / "dist/ps3/layout"
TITLE = "PCPS00001"
EXPECTED = ("[pc2ps3] boot", "[pc2ps3] audio", "[pc2ps3] qa autoplay on", "[pc2ps3] checkpoint",
            "[pc2ps3] victory", "[pc2ps3] save write status=0", "[pc2ps3] exit status 0")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("rpcs3_dir", type=Path, help="folder containing rpcs3.exe (firmware installed)")
    parser.add_argument("--timeout", type=int, default=300)
    parser.add_argument("--keep-flag", action="store_true", help="leave USRDIR/qa_autoplay in place")
    args = parser.parse_args()

    exe = args.rpcs3_dir / ("rpcs3.exe" if sys.platform == "win32" else "rpcs3")
    if not exe.is_file() or not (LAYOUT / "USRDIR/EBOOT.BIN").is_file():
        print("ERROR: need " + str(exe) + " and a packaged layout (run package-ps3 first)", file=sys.stderr)
        return 1
    game = args.rpcs3_dir / "dev_hdd0/game" / TITLE
    shutil.copytree(LAYOUT, game, dirs_exist_ok=True)  # keeps user data such as USRDIR/player and saves
    flag = game / "USRDIR/qa_autoplay"
    flag.write_text("1\n", encoding="ascii")
    try:
        result = subprocess.run([str(exe), "--no-gui", str(game / "USRDIR/EBOOT.BIN")],
                                timeout=args.timeout)
        code = result.returncode
    except subprocess.TimeoutExpired:
        code = None
    finally:
        if not args.keep_flag:
            flag.unlink(missing_ok=True)
    tty = (args.rpcs3_dir / "log/TTY.log").read_text(encoding="utf-8", errors="replace")
    lines = [line for line in tty.splitlines() if line.startswith("[pc2ps3]") and "pad buttons" not in line]
    print("\n".join(lines))
    missing = [marker for marker in EXPECTED if not any(line.startswith(marker) for line in lines)]
    print("rpcs3 exit code: " + ("timeout" if code is None else str(code)))
    print("missing markers: " + (", ".join(missing) if missing else "none"))
    return 0 if code == 0 and not missing else 1


if __name__ == "__main__":
    sys.exit(main())
