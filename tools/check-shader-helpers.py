"""Check that shader helpers declare their own dependencies and compile in isolation."""

import argparse
import os
import re
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
    slots = (shaders / "resource_slots.h").read_text()
    backend = (root / "third_party/NoGraphicsAPI/src/NoGraphicsAPI.cpp").read_text()
    expected = int(re.search(r"ORBITAL_TEXTURE_COUNT (\d+)", slots)[1])
    actual = int(re.search(r"conventional_texture_descriptor_count = (\d+)", backend)[1])
    if actual != expected:
        raise ValueError(f"Conventional descriptor count {actual} differs from shader slots {expected}")
    # Helpers declare their role with an include guard; feature folders also contain entry points.
    helpers = sorted(path for path in shaders.rglob("*.slang") if "#pragma once" in path.read_text())
    for helper in helpers:
        subprocess.run([str(args.compiler), str(helper), "-target", "spirv", "-no-codegen"], check=True)
    print(f"All {len(helpers)} shader helpers compile independently.")


if __name__ == "__main__":
    main()
