#!/usr/bin/env python3
"""Compile and exercise production code with simulated display/power APIs."""
from pathlib import Path
import subprocess
import tempfile

REPO = Path(__file__).resolve().parents[1]
with tempfile.TemporaryDirectory(prefix="monitor-brightness-") as temporary:
    executable = Path(temporary) / "test_brightness_sync"
    sources = sorted((REPO / "MonitorBrightnessSync").rglob("*.cpp"))
    sources = [path for path in sources if path.name != "main.cpp"]
    subprocess.run([
        "xcrun", "clang++", "-std=c++20", "-Wall", "-Wextra",
        "-I", str(REPO / "MonitorBrightnessSync"),
        str(REPO / "tests/test_brightness_sync.cpp"),
        *map(str, sources), "-o", str(executable),
    ], check=True)
    subprocess.run([str(executable)], check=True, timeout=30)
