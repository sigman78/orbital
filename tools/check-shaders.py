"""Compile the shader library in isolation and report every failure.

    python tools/check-shaders.py                    helpers, entry points, SPIR-V validation
    python tools/check-shaders.py --compiler PATH --validator PATH

Every guarded helper (a .slang with #pragma once) compiles on its own, so an
include that happens to run first cannot hide a missing dependency. Every
[shader(...)] entry point compiles with the CMake flags to a temporary output,
then spirv-val checks it when a validator is found. The conventional backend's
descriptor count is compared with the shared slot definitions. All failures are
listed before the non-zero exit.
"""

import argparse
from concurrent.futures import ThreadPoolExecutor
import os
import re
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile

ROOT = Path(__file__).resolve().parent.parent
SHADERS = ROOT / "shaders"
EXE = ".exe" if os.name == "nt" else ""
ENTRY = re.compile(r'\[shader\("(\w+)"\)\]\s*(?:\[[^\]]*\]\s*)*[\w<>:]+\s+(\w+)\s*\(')
COMPILE_FLAGS = ["-target", "spirv", "-profile", "spirv_1_6", "-matrix-layout-column-major", "-fvk-use-entrypoint-name"]


def run(command):
    result = subprocess.run(command, capture_output=True, text=True)
    return result.returncode == 0, (result.stdout + result.stderr).strip()


def check_descriptor_count():
    slots = (SHADERS / "resource_slots.h").read_text()
    backend = (ROOT / "third_party/NoGraphicsAPI/src/NoGraphicsAPI.cpp").read_text()
    expected = int(re.search(r"ORBITAL_TEXTURE_COUNT (\d+)", slots)[1])
    actual = int(re.search(r"conventional_texture_descriptor_count = (\d+)", backend)[1])
    if actual != expected:
        return [f"descriptor count: backend {actual} differs from shader slots {expected}"]
    return []


def check_helper(compiler, helper):
    ok, output = run([str(compiler), str(helper), "-target", "spirv", "-no-codegen"])
    return None if ok else f"helper {helper.relative_to(ROOT)}:\n{output}"


def check_entry(compiler, validator, output_dir, source, stage, entry):
    output = output_dir / f"{source.stem}.{entry}.{stage}.spv"
    ok, text = run([str(compiler), str(source), *COMPILE_FLAGS, "-entry", entry, "-stage", stage, "-o", str(output)])
    if not ok:
        return f"entry {source.relative_to(ROOT)} {entry} ({stage}):\n{text}"
    if validator:
        ok, text = run([str(validator), "--target-env", "vulkan1.3", str(output)])
        if not ok:
            return f"spirv-val {source.relative_to(ROOT)} {entry} ({stage}):\n{text}"
    return None


def find_tool(explicit, name, hint):
    if explicit:
        return explicit
    candidate = ROOT / hint / f"{name}{EXE}"
    if candidate.exists():
        return candidate
    found = shutil.which(name)
    return Path(found) if found else None


def main():
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--compiler", type=Path, help="slangc (default: .tools/slang/bin, then PATH)")
    parser.add_argument("--validator", type=Path, help="spirv-val (default: .tools/Vulkan-ValidationLayers/bin, then PATH)")
    parser.add_argument("--jobs", type=int, default=os.cpu_count() or 4)
    args = parser.parse_args()
    compiler = find_tool(args.compiler, "slangc", ".tools/slang/bin")
    validator = find_tool(args.validator, "spirv-val", ".tools/Vulkan-ValidationLayers/bin")
    if compiler is None:
        sys.exit("slangc not found; pass --compiler")

    sources = sorted(SHADERS.rglob("*.slang"))
    helpers = [path for path in sources if "#pragma once" in path.read_text()]
    entries = [(path, stage, entry) for path in sources for stage, entry in ENTRY.findall(path.read_text())]
    failures = check_descriptor_count()
    with tempfile.TemporaryDirectory() as temporary, ThreadPoolExecutor(args.jobs) as pool:
        output_dir = Path(temporary)
        jobs = [pool.submit(check_helper, compiler, helper) for helper in helpers]
        jobs += [pool.submit(check_entry, compiler, validator, output_dir, *entry) for entry in entries]
        failures += [failure for failure in (job.result() for job in jobs) if failure]

    for failure in failures:
        print(failure, end="\n\n")
    validated = "validated" if validator else "not validated (no spirv-val)"
    summary = f"{len(helpers)} helpers, {len(entries)} entry points {validated}"
    if failures:
        sys.exit(f"{len(failures)} failure(s); {summary}")
    print(f"All shaders compile: {summary}.")


if __name__ == "__main__":
    main()
