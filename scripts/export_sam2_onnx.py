#!/usr/bin/env python3
"""Export SAM2 image encoder/decoder ONNX for EmbrSAM2 OFX.

This script is intentionally thin: it documents the expected artifact names and
tries common export entry points. Run on a machine with GPU/PyTorch as needed.

Expected outputs (default --out-dir models/sam2):
  image_encoder.onnx
  image_decoder.onnx

Compatible with the I/O name heuristics in plugins/sam2/src/Sam2Engine.cpp
(image / image_embed / high_res_feats_* / point_coords / point_labels / masks).
"""

from __future__ import annotations

import argparse
import sys
from pathlib import Path


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--out-dir", type=Path, default=Path("models/sam2"))
    parser.add_argument(
        "--via",
        choices=["hint", "sam2-onnx-cpp"],
        default="hint",
        help="hint prints instructions; sam2-onnx-cpp delegates if that repo is present",
    )
    parser.add_argument("--sam2-onnx-cpp", type=Path, default=None)
    parser.add_argument("--model-size", default="base_plus")
    args = parser.parse_args()

    args.out_dir.mkdir(parents=True, exist_ok=True)

    if args.via == "hint":
        print(
            """
EmbrSAM2 expects:
  {out}/image_encoder.onnx
  {out}/image_decoder.onnx

Recommended export (external repo, Apache-2.0):
  1) git clone https://github.com/pagarcia/sam2-onnx-cpp
  2) follow its README to export base_plus (or tiny)
  3) copy checkpoints/<size>/image_encoder.onnx
                 checkpoints/<size>/image_decoder.onnx
     into {out}/

Then rebuild is not required; set the OFX file params to those paths
(or place files at the defaults above).
""".format(out=args.out_dir)
        )
        return 0

    repo = args.sam2_onnx_cpp
    if repo is None or not repo.exists():
        print("Pass --sam2-onnx-cpp /path/to/sam2-onnx-cpp", file=sys.stderr)
        return 2

    export_py = repo / "export" / "onnx_export.py"
    if not export_py.exists():
        print(f"Missing {export_py}", file=sys.stderr)
        return 2

    import subprocess

    cmd = [sys.executable, str(export_py), "--model_size", args.model_size]
    print("Running:", " ".join(cmd))
    subprocess.check_call(cmd, cwd=str(repo))
    src = repo / "checkpoints" / args.model_size
    for name in ("image_encoder.onnx", "image_decoder.onnx"):
        s = src / name
        d = args.out_dir / name
        if not s.exists():
            print(f"Export did not produce {s}", file=sys.stderr)
            return 3
        d.write_bytes(s.read_bytes())
        print(f"Wrote {d}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
