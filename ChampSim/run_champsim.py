#!/usr/bin/python3 python3
"""
Python script to run ChampSim simulations for all traces in a directory.

Usage:
    python3 run_champsim.py <branch_predictor> <N-WARM> <N-SIM> <traces_directory>

- <branch_predictor>: Name of the branch predictor (used ONLY to name the output directory, e.g., "bimodal").
- <N-WARM>: Number of warm-up instructions (e.g., 200000000).
- <N-SIM>: Number of simulation instructions (e.g., 500000000).
- <traces_directory>: Path to the directory containing *.xz trace files.

Prerequisites:
1. ChampSim must be built (run ./config.sh champsim_config.json && make).
2. The binary must exist at ./bin/champsim (the branch predictor must already be configured in champsim_config.json).
3. Run this script from the root of the ChampSim repository.

The script will:
- Create an output directory named exactly after the branch predictor (e.g., "bimodal/").
- Run one simulation per .xz trace.
- Save the full stdout (including all statistics) to <output_dir>/<trace_stem>.txt.
"""

import argparse
import subprocess
import sys
from pathlib import Path


def main():
    parser = argparse.ArgumentParser(
        description="Batch runner for ChampSim simulations across multiple traces."
    )
    parser.add_argument(
        "branch_predictor",
        type=str,
        help="Branch predictor name (used to name the output directory)",
    )
    parser.add_argument(
        "n_warm",
        type=int,
        help="Number of warm-up instructions (full integer, e.g. 200000000)",
    )
    parser.add_argument(
        "n_sim",
        type=int,
        help="Number of simulation instructions (full integer, e.g. 500000000)",
    )
    parser.add_argument(
        "traces_dir",
        type=str,
        help="Path to directory containing .xz trace files",
    )

    args = parser.parse_args()

    # Output directory named exactly using the branch predictor (as requested)
    output_dir = Path(args.branch_predictor)
    output_dir.mkdir(exist_ok=True, parents=True)

    # ChampSim binary path (relative to repository root)
    binary_path = Path("bin/champsim")
    if not binary_path.exists():
        print(f"ERROR: ChampSim binary not found at {binary_path.resolve()}")
        print("       Please build ChampSim first: ./config.sh champsim_config.json && make")
        sys.exit(1)

    traces_path = Path(args.traces_dir)
    if not traces_path.is_dir():
        print(f"ERROR: Traces directory not found: {traces_path}")
        sys.exit(1)

    # Find all .xz files (non-recursive; assumes traces are directly in the directory)
    trace_files = sorted(traces_path.glob("*.xz"))
    if not trace_files:
        print(f"ERROR: No .xz files found in {traces_path}")
        sys.exit(1)

    print(f"Found {len(trace_files)} trace(s). Output directory: {output_dir}")
    print(f"Using binary: {binary_path.resolve()}")
    print(f"Warm-up instructions : {args.n_warm:,}")
    print(f"Simulation instructions: {args.n_sim:,}\n")

    for i, trace in enumerate(trace_files, start=1):
        trace_name = trace.stem  # e.g. "600.perlbench_s-210B.champsimtrace"
        output_file = output_dir / f"{trace_name}.txt"

        print(f"[{i:3d}/{len(trace_files)}] Running: {trace.name} → {output_file.name}")

        cmd = [
            str(binary_path),
            "--warmup-instructions", str(args.n_warm),
            "--simulation-instructions", str(args.n_sim),
            str(trace),
        ]

        try:
            # Run simulation and redirect stdout to file (stderr goes to console if error)
            with open(output_file, "w", encoding="utf-8") as f:
                result = subprocess.run(
                    cmd,
                    stdout=f,
                    stderr=subprocess.PIPE,
                    check=True,
                    text=True,
                )
            print(f"    ✓ Completed (saved to {output_file.name})")
        except subprocess.CalledProcessError as e:
            print(f"    ✗ FAILED (exit code {e.returncode})")
            if e.stderr:
                print(f"    Error: {e.stderr.strip()}")
            # Still write partial output if any
            if output_file.exists():
                output_file.rename(output_file.with_suffix(".txt.ERROR"))
        except Exception as e:
            print(f"    ✗ Unexpected error: {e}")

    print("\nAll simulations finished!")
    print(f"Results are in: {output_dir.resolve()}/")


if __name__ == "__main__":
    main()