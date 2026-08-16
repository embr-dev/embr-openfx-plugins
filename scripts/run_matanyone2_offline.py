#!/usr/bin/env python3
"""Offline MatAnyone2 runner (real neural model) until OFX neural backend lands.

Requires:
  pip install 'matanyone2 @ git+https://github.com/pq-yang/MatAnyone2.git'

Example:
  python3 scripts/run_matanyone2_offline.py \\
    --video in.mp4 --mask mask.png --out results/
"""

from __future__ import annotations

import argparse
from pathlib import Path


def main() -> int:
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument("--video", required=True)
    p.add_argument("--mask", required=True)
    p.add_argument("--out", default="results")
    p.add_argument("--device", default="cuda:0")
    p.add_argument("--model", default="PeiqingYang/MatAnyone2")
    args = p.parse_args()

    try:
        from matanyone2 import MatAnyone2, InferenceCore
    except ImportError:
        print("Install: pip install 'matanyone2 @ git+https://github.com/pq-yang/MatAnyone2.git'")
        return 2

    Path(args.out).mkdir(parents=True, exist_ok=True)
    model = MatAnyone2.from_pretrained(args.model)
    core = InferenceCore(model, device=args.device)
    fgr, pha = core.process_video(
        input_path=args.video,
        mask_path=args.mask,
        output_path=args.out,
    )
    print("foreground:", fgr)
    print("alpha:", pha)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
