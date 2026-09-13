"""Check that shader helpers declare their own dependencies and compile in isolation."""

import argparse
import os
from pathlib import Path
import subprocess


def main():
    root = Path(__file__).resolve().parent.parent
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        "--compiler",
        type=Path,
        default=root / ".tools/slang/bin" / ("slangc.exe" if os.name == "nt" else "slangc"),
    )
    args = parser.parse_args()
    shaders = root / "shaders"
    helpers = sorted(
        path
        for directory in ("lib", "scene", "surface", "post")
        for path in (shaders / directory).rglob("*.slang")
    )
    helpers += [shaders / name for name in ("belt.slang", "beltfar.slang", "clouds.slang", "rockclass.slang")]
    for helper in helpers:
        subprocess.run([str(args.compiler), str(helper), "-target", "spirv", "-no-codegen"], check=True)
    print(f"All {len(helpers)} shader helpers compile independently.")


if __name__ == "__main__":
    main()
