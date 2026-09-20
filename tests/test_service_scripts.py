#!/usr/bin/env python3
"""Exercise packaging without touching the user's installation or launchd."""
import os
from pathlib import Path
import plistlib
import subprocess
import tempfile

REPO = Path(__file__).resolve().parents[1]

with tempfile.TemporaryDirectory(prefix="monitor-service-") as temporary:
    root = Path(temporary)
    project = root / "Project & space"
    scripts = project / "scripts"
    scripts.mkdir(parents=True)
    user_root = root / "User & space"
    user_root.mkdir()
    commands = root / "commands"
    commands.mkdir()
    trace = root / "trace"
    # Redirect only script filesystem references; leave the real HOME unchanged.
    for name in ("install.sh", "uninstall.sh"):
        target = scripts / name
        target.write_text((REPO / "scripts" / name).read_text().replace(
            "$HOME", "$SERVICE_TEST_ROOT"))
        target.chmod(0o755)
    executable = project / "build/Build/Products/Release/MonitorBrightnessSync"
    executable.parent.mkdir(parents=True)
    executable.write_text("#!/bin/sh\nexit 0\n")
    executable.chmod(0o755)
    for name, body in {
        "xcodebuild": 'echo build >> "$SERVICE_TRACE"\nexit "${BUILD_RESULT:-0}"\n',
        "launchctl": 'printf "%s\\n" "$*" >> "$SERVICE_TRACE"\n',
    }.items():
        target = commands / name
        target.write_text("#!/bin/sh\n" + body)
        target.chmod(0o755)
    env = dict(os.environ, PATH=f"{commands}:/usr/bin:/bin:/usr/sbin:/sbin",
               SERVICE_TEST_ROOT=str(user_root), SERVICE_TRACE=str(trace))
    plist = user_root / "Library/LaunchAgents/com.monitorbrightnesssync.agent.plist"
    installed = user_root / "Library/Application Support/MonitorBrightnessSync/MonitorBrightnessSync"
    legacy = user_root / ".local/bin/MonitorBrightnessSync"
    legacy.parent.mkdir(parents=True)
    legacy.write_text("old")

    def run(name, *args, success=True, extra=None):
        result = subprocess.run([str(scripts / name), *args], cwd=root,
                                env=env | (extra or {}), capture_output=True, text=True)
        assert (result.returncode == 0) == success, result.stdout + result.stderr

    run("install.sh")
    data = plistlib.loads(plist.read_bytes())
    assert data["ProgramArguments"] == [str(installed), "--interval", "1"]
    assert data["RunAtLoad"] and data["KeepAlive"]
    assert data["StandardErrorPath"] == str(user_root / "Library/Logs/MonitorBrightnessSync/error.log")
    assert installed.exists() and not legacy.exists()
    events = trace.read_text().splitlines()
    assert [line.split()[0] for line in events] == ["build", "bootout", "enable", "bootstrap", "kickstart"]

    run("install.sh", "--interval", "0.5", "--min", "0.2", "--max", "0.8")
    assert plistlib.loads(plist.read_bytes())["ProgramArguments"][1:] == ["--interval", "0.5", "--min", "0.2", "--max", "0.8"]
    previous = plist.read_bytes(), installed.read_bytes()
    trace.write_text("")
    run("install.sh", success=False, extra={"BUILD_RESULT": "1"})
    assert trace.read_text() == "build\n"
    assert previous == (plist.read_bytes(), installed.read_bytes())
    for option in ("--once", "--help"):
        run("install.sh", option, success=False)
    run("install.sh", "--binary", str(executable), "--interval", "2")
    assert plistlib.loads(plist.read_bytes())["ProgramArguments"][1:] == ["--interval", "2"]
    run("install.sh", "--binary", "/missing/executable", success=False)
    log = user_root / "Library/Logs/MonitorBrightnessSync/output.log"
    log.write_text("retain")
    unrelated = installed.parent / "unrelated.txt"
    unrelated.write_text("retain")
    run("uninstall.sh")
    run("uninstall.sh")
    assert not installed.exists() and not plist.exists()
    assert log.read_text() == unrelated.read_text() == "retain"
    print("PASS: install, update, build failure, arguments, migration, prebuilt binary, and repeated uninstall")
