#!/usr/bin/env python3
"""Compare two flight-matrix runs. Reports per-shot RMS pixel error,
worst first; exits nonzero if any shot drifts past the threshold.

Usage: scripts/flight_diff.py <baseline_dir> <candidate_dir> [--thresh 1.0]
"""
import sys, os
from PIL import Image, ImageChops

def rms(a, b):
    diff = ImageChops.difference(a.convert("RGB"), b.convert("RGB"))
    h = diff.histogram()
    total = 0.0
    n = a.size[0] * a.size[1] * 3
    for band in range(3):
        for v, count in enumerate(h[band * 256:(band + 1) * 256]):
            total += count * (v * v)
    return (total / n) ** 0.5

def main():
    args = [a for a in sys.argv[1:] if not a.startswith("--")]
    thresh = 1.0
    if "--thresh" in sys.argv:
        thresh = float(sys.argv[sys.argv.index("--thresh") + 1])
    base_dir, cand_dir = args[0], args[1]
    base = {f for f in os.listdir(base_dir) if f.endswith(".png")}
    cand = {f for f in os.listdir(cand_dir) if f.endswith(".png")}
    results, missing = [], sorted((base ^ cand))
    for f in sorted(base & cand):
        a = Image.open(os.path.join(base_dir, f))
        b = Image.open(os.path.join(cand_dir, f))
        if a.size != b.size:
            results.append((float("inf"), f))
            continue
        results.append((rms(a, b), f))
    results.sort(reverse=True)
    bad = [(e, f) for e, f in results if e > thresh]
    # EVERY drifting shot is printed. This used to show results[:20]
    # flat, which quietly hid drift #21 onward — and the summary line
    # still said "36 past threshold", so the list looked complete while
    # being a third of the story. Reading which shots drifted is the
    # whole point: that is how you tell a contained change from a leak.
    shown = bad if bad else results[:20]
    for e, f in shown:
        mark = "DRIFT" if e > thresh else "  ok "
        print(f"{mark}  rms={e:8.3f}  {f}")
    if bad:
        exact = sum(1 for e, _ in results if e == 0.0)
        print(f"\n{exact} of {len(results)} shots bit-identical")
    if missing:
        print(f"missing from one side: {len(missing)}")
        for f in missing[:10]:
            print(f"  {f}")
    print(f"\n{len(results)} compared, {len(bad)} past threshold {thresh}")
    sys.exit(1 if (bad or missing) else 0)

if __name__ == "__main__":
    main()
