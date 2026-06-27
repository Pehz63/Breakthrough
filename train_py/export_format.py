#!/usr/bin/env python3
"""Write a Breakthrough model file that the C++ engine can load.

This is the Python -> C++ contract: heavy models (MLP / NNUE / transformer) train
in PyTorch here, then export their weights into the same plain-text format the
engine's loadModel() reads (see ML.md "Model file format"). The C++ engine always
does the in-search inference; Python only produces weights.

Phase 1 ships the `linear` format. Higher-capacity formats add their own `type=`
and the matching loader case in src/ml_model.cpp.

Usage (demo: export a random linear value model the engine can load):
    python train_py/export_format.py --out models/py_linear.txt --features 30
Then point a side's evaluator at it in C++:
    train.exe tournament   # if you copy it to models/lin_value.txt, or load a slot
"""
import argparse


def write_linear_model(path, weights, bias, head="value", feature_version=1, out_scale=900.0):
    """Write a linear model in the engine's text format.

    weights: 1-D sequence of floats (len == feature_count).
    head: "value" (board features) or "policy" (move features).
    """
    n = len(weights)
    with open(path, "w") as f:
        f.write("# Breakthrough ML model\n")
        f.write("type=linear\n")
        f.write(f"head={head}\n")
        f.write(f"feature_version={feature_version}\n")
        f.write(f"feature_count={n}\n")
        f.write(f"out_scale={out_scale}\n")
        f.write(f"bias={float(bias)}\n")
        for i, w in enumerate(weights):
            f.write(f"w{i}={float(w)}\n")
    print(f"Wrote {path}  (type=linear head={head} features={n})")


def _demo(args):
    # Prefer torch if available (shows the real export path); fall back to numpy.
    try:
        import torch
        lin = torch.nn.Linear(args.features, 1)
        weights = lin.weight.detach().numpy().ravel().tolist()
        bias = float(lin.bias.detach().item())
        print("(using torch.nn.Linear weights)")
    except Exception:  # noqa: BLE001
        import random
        weights = [random.uniform(-0.05, 0.05) for _ in range(args.features)]
        bias = 0.0
        print("(torch not installed; using random weights)")
    write_linear_model(args.out, weights, bias, head=args.head, out_scale=args.out_scale)


if __name__ == "__main__":
    ap = argparse.ArgumentParser(description="Export a linear model in the C++ engine format.")
    ap.add_argument("--out", default="models/py_linear.txt")
    ap.add_argument("--features", type=int, default=30, help="30=value head, 9=policy head")
    ap.add_argument("--head", default="value", choices=["value", "policy"])
    ap.add_argument("--out-scale", dest="out_scale", type=float, default=900.0)
    _demo(ap.parse_args())
