# SAM2 ONNX models (not committed)

Place exported files here:

- `image_encoder.onnx`
- `image_decoder.onnx`

Generate them with:

```bash
python3 scripts/export_sam2_onnx.py --via hint
```

Or copy from a `sam2-onnx-cpp` export (`checkpoints/<size>/`).

Without these files the OFX plugin still loads in **demo mode**
(elliptical soft mask from the box/point controls) so host wiring can be tested.
