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
    # Helpers declare their role with an include guard; feature folders also contain entry points.
    helpers = sorted(path for path in shaders.rglob("*.slang") if "#pragma once" in path.read_text())
    for helper in helpers:
        subprocess.run([str(args.compiler), str(helper), "-target", "spirv", "-no-codegen"], check=True)
    print(f"All {len(helpers)} shader helpers compile independently.")


if __name__ == "__main__":
    main()
