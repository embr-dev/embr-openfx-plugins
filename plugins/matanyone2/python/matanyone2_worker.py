#!/usr/bin/env python3
"""MatAnyone2 OFX neural worker (stdin/stdout binary protocol).

Protocol (little-endian):
  Request header: magic u32(=0x4D413201), cmd u32
    CMD_INIT=1: path_len u32, path utf8, device_len u32, device utf8,
                erode i32, dilate i32, warmup i32, max_size i32
    CMD_RESET=2
    CMD_STEP=3: w i32, h i32, has_mask i32,
                rgb float32[w*h*3] (RGB 0..1),
                mask float32[w*h] if has_mask
    CMD_QUIT=4
  Response: status u32 (0=ok, else error),
    if error: msg_len u32, msg utf8
    if STEP ok: w i32, h i32, alpha float32[w*h]
"""

from __future__ import annotations

import os
import struct
import sys
import traceback
from typing import Optional

MAGIC = 0x4D413201
CMD_INIT, CMD_RESET, CMD_STEP, CMD_QUIT = 1, 2, 3, 4

_ROOT = os.environ.get("EMBR_MATANYONE2_SRC")
if _ROOT:
    sys.path.insert(0, _ROOT)


def _read_exact(n: int) -> bytes:
    buf = bytearray()
    while len(buf) < n:
        chunk = sys.stdin.buffer.read(n - len(buf))
        if not chunk:
            raise EOFError("stdin closed")
        buf.extend(chunk)
    return bytes(buf)


def _read_u32() -> int:
    return struct.unpack("<I", _read_exact(4))[0]


def _read_i32() -> int:
    return struct.unpack("<i", _read_exact(4))[0]


def _read_str() -> str:
    n = _read_u32()
    return _read_exact(n).decode("utf-8")


def _write_u32(v: int) -> None:
    sys.stdout.buffer.write(struct.pack("<I", v))


def _write_i32(v: int) -> None:
    sys.stdout.buffer.write(struct.pack("<i", v))


def _write_ok() -> None:
    _write_u32(0)
    sys.stdout.buffer.flush()


def _write_err(msg: str) -> None:
    data = msg.encode("utf-8", errors="replace")
    _write_u32(1)
    _write_u32(len(data))
    sys.stdout.buffer.write(data)
    sys.stdout.buffer.flush()


def _patch_topk_safe() -> None:
    """Clamp top_k to memory length so short OFX clips do not crash torch.topk."""
    from matanyone2.model.utils import memory_utils

    _orig = memory_utils.do_softmax

    def _safe_do_softmax(similarity, top_k=None, inplace=False, return_usage=False):
        if top_k is not None and top_k > 0:
            n = int(similarity.shape[1])
            if n <= 0:
                top_k = None
            elif top_k > n:
                top_k = n
        return _orig(similarity, top_k=top_k, inplace=inplace, return_usage=return_usage)

    memory_utils.do_softmax = _safe_do_softmax


class Session:
    def __init__(self) -> None:
        self.processor = None
        self.device = "cpu"
        self.erode = 10
        self.dilate = 10
        self.warmup = 10
        self.max_size = -1
        self.frame_index = 0
        self.objects = [1]
        self._torch = None
        self._F = None
        self._np = None
        self._gen_dilate = None
        self._gen_erosion = None

    def init(self, ckpt: str, device: str, erode: int, dilate: int, warmup: int, max_size: int) -> None:
        import numpy as np
        import torch
        import torch.nn.functional as F
        from hydra.core.global_hydra import GlobalHydra
        from matanyone2.utils.get_default_model import get_matanyone2_model
        from matanyone2.inference.inference_core import InferenceCore
        from matanyone2.utils.inference_utils import gen_dilate, gen_erosion

        _patch_topk_safe()

        if device in ("auto", ""):
            device = "cuda" if torch.cuda.is_available() else "cpu"
        if device.startswith("cuda") and not torch.cuda.is_available():
            device = "cpu"

        self._torch = torch
        self._F = F
        self._np = np
        self._gen_dilate = gen_dilate
        self._gen_erosion = gen_erosion
        self.device = device
        self.erode = int(erode)
        self.dilate = int(dilate)
        self.warmup = max(0, int(warmup))
        self.max_size = int(max_size)
        self.frame_index = 0

        # get_matanyone2_model calls hydra.initialize once; clear for worker restarts.
        if GlobalHydra.instance().is_initialized():
            GlobalHydra.instance().clear()

        model = get_matanyone2_model(ckpt, device=device)
        self.processor = InferenceCore(model, cfg=model.cfg, device=device)

    def reset(self) -> None:
        if self.processor is not None:
            self.processor.clear_memory()
        self.frame_index = 0

    def _prep_mask(self, mask_opt, h: int, w: int):
        np = self._np
        mask = np.clip(mask_opt.reshape(h, w) * 255.0, 0, 255).astype(np.uint8)
        if self.dilate > 0:
            mask = self._gen_dilate(mask, self.dilate, self.dilate)
        if self.erode > 0:
            mask = self._gen_erosion(mask, self.erode, self.erode)
        return self._torch.from_numpy(mask).float().to(self.device)

    def step(self, w: int, h: int, rgb, mask_opt: Optional[object]):
        torch = self._torch
        F = self._F
        np = self._np
        assert self.processor is not None

        image = torch.from_numpy(rgb.reshape(h, w, 3).transpose(2, 0, 1)).float()
        if self.max_size > 0:
            min_side = min(h, w)
            if min_side > self.max_size:
                new_h = int(h / min_side * self.max_size)
                new_w = int(w / min_side * self.max_size)
                image = F.interpolate(
                    image.unsqueeze(0), size=(new_h, new_w), mode="bilinear", align_corners=False
                )[0]
                if mask_opt is not None:
                    mask_opt = F.interpolate(
                        torch.from_numpy(mask_opt.reshape(1, 1, h, w)).float(),
                        size=(new_h, new_w),
                        mode="nearest",
                    )[0, 0].numpy()
                h, w = new_h, new_w

        image = image.to(self.device)

        # Match InferenceCore.process_video:
        #   ti==0: encode mask + first_frame_pred
        #   1..warmup: first_frame_pred
        #   else: temporal step
        # Host/engine should send mask only to seed (or after Reset). Extra masks mid-sequence
        # are ignored so OFX hosts that keep Mask connected do not reseed every frame.
        if self.frame_index == 0:
            if mask_opt is None:
                raise RuntimeError("first frame requires a mask")
            mask_t = self._prep_mask(mask_opt, h, w)
            self.processor.step(image, mask_t, objects=self.objects)
            output_prob = self.processor.step(image, first_frame_pred=True)
            self.frame_index = 1
        elif self.frame_index <= self.warmup:
            output_prob = self.processor.step(image, first_frame_pred=True)
            self.frame_index += 1
        else:
            output_prob = self.processor.step(image)
            self.frame_index += 1

        alpha = self.processor.output_prob_to_mask(output_prob)
        alpha_np = alpha.detach().float().cpu().numpy().reshape(-1)
        alpha_np = np.nan_to_num(alpha_np, nan=0.0, posinf=1.0, neginf=0.0)
        alpha_np = np.clip(alpha_np, 0.0, 1.0).astype(np.float32)
        return w, h, alpha_np


def main() -> int:
    session = Session()
    try:
        while True:
            magic = _read_u32()
            if magic != MAGIC:
                _write_err(f"bad magic {magic:#x}")
                continue
            cmd = _read_u32()
            try:
                if cmd == CMD_INIT:
                    ckpt = _read_str()
                    device = _read_str()
                    erode = _read_i32()
                    dilate = _read_i32()
                    warmup = _read_i32()
                    max_size = _read_i32()
                    session.init(ckpt, device, erode, dilate, warmup, max_size)
                    _write_ok()
                elif cmd == CMD_RESET:
                    session.reset()
                    _write_ok()
                elif cmd == CMD_STEP:
                    import numpy as np

                    w = _read_i32()
                    h = _read_i32()
                    has_mask = _read_i32()
                    rgb = np.frombuffer(_read_exact(w * h * 3 * 4), dtype=np.float32).copy()
                    mask = None
                    if has_mask:
                        mask = np.frombuffer(_read_exact(w * h * 4), dtype=np.float32).copy()
                    out_w, out_h, alpha = session.step(w, h, rgb, mask)
                    _write_u32(0)
                    _write_i32(out_w)
                    _write_i32(out_h)
                    sys.stdout.buffer.write(alpha.tobytes())
                    sys.stdout.buffer.flush()
                elif cmd == CMD_QUIT:
                    _write_ok()
                    return 0
                else:
                    _write_err(f"unknown cmd {cmd}")
            except Exception as e:
                _write_err(f"{e}\n{traceback.format_exc()}")
    except EOFError:
        return 0


if __name__ == "__main__":
    raise SystemExit(main())
