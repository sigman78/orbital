"""Checks the optional layer fitter, composition constraints and encoder/preparer agreement."""
import importlib.util
import json
from pathlib import Path
import struct
import subprocess
import tempfile

import numpy as np
from PIL import Image
import torch

spec = importlib.util.spec_from_file_location("galaxy", Path(__file__).with_name("bake-galaxy-layers.py"))
galaxy = importlib.util.module_from_spec(spec)
spec.loader.exec_module(galaxy)


def main():
    torch.set_num_threads(2)
    low = torch.full((1, 3, 4, 8), .2)
    cloud = torch.full((1, 3, 4, 16), .3)
    dark = torch.full((1, 1, 8, 32), .7)
    layers = [low, cloud, dark]
    base = galaxy.compose(layers, 64, 32, 0, 0)
    light = galaxy.compose(layers, 64, 32, 1, 0)
    composite = galaxy.compose(layers, 64, 32)
    assert torch.allclose(base, low.new_full(base.shape, .2))
    assert torch.all(light >= base)  # additive cloud layer
    assert torch.all(composite <= light)  # filaments can only darken
    assert torch.allclose(composite[..., :8, :], base[..., :8, :])  # strips cannot affect high latitudes
    assert torch.allclose(composite[..., -8:, :], base[..., -8:, :])

    # A wrapped alternating row has the same interpolation slope through the seam.
    row = torch.tensor([[[[0., 1., 0., 1.]]]])
    samples = galaxy.sample(row, 8, 1).flatten()
    assert torch.allclose(samples, torch.tensor([.25, .25, .75, .75, .25, .25, .75, .75]))
    target = np.full((32, 64, 3), .25, dtype=np.float32)
    _, history = galaxy.fit(target, (8, 16, 32), 60, "cpu")
    assert history[-1]["loss"] < history[0]["loss"] * .5

    # Independent C++ preparation checks byte-for-byte mip agreement, including 3->1 extents.
    with tempfile.TemporaryDirectory(prefix="galaxy-check-") as temporary:
        folder = Path(temporary)
        rng = np.random.default_rng(7)
        pixels = rng.integers(0, 256, (11, 17, 4), dtype=np.uint8)
        source = folder / "galaxy_low.png"
        Image.fromarray(pixels).save(source)
        executable = galaxy.ROOT / "build/release" / ("texture_prepare.exe" if galaxy.os.name == "nt" else "texture_prepare")
        subprocess.run([str(executable), str(source), str(folder / "prepared"), "--raw"], check=True)
        meta = json.loads((folder / "prepared/metadata.json").read_text())
        image = Image.fromarray(pixels)
        for level in range(meta["levels"]):
            assert image.tobytes() == (folder / f"prepared/{level}.rgba").read_bytes()
            image = galaxy.downsample(image)
        encoder = galaxy.ROOT / "build/release" / ("bc7_compress.exe" if galaxy.os.name == "nt" else "bc7_compress")
        constant = np.full((11, 17, 1), .4, dtype=np.float32)
        decoded, size = galaxy.encode(source, constant, encoder)
        assert np.max(abs(decoded - .4)) < 2 / 255
        data = source.with_suffix(".bc7.otex").read_bytes()
        header = struct.unpack("<16I", data[:64])
        assert size == len(data) and header[2:5] == (17, 11, 5)
        assert header[7:9] == (2, 0)  # BC7, raw linear data rather than sRGB conversion
        assert header[9] | (header[10] << 32) == galaxy.fnv(source.read_bytes())
        assert header[11] | (header[12] << 32) == galaxy.fnv(data[64:])
    print("Galaxy composition, wrap sampling, Adam convergence, mip and BC7 checks passed.")


if __name__ == "__main__":
    main()
