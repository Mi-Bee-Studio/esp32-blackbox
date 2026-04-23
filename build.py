#!/usr/bin/env python3
"""
ESP32 Blackbox Build Script
Usage: python build.py <target> [action] [port]
  target: esp32c3 | esp32c6
  action: build | flash | monitor | clean
  port:   COM port number (e.g. COM3)
"""

import os
import sys
import subprocess
import shutil

IDF_TOOLS_PATH = r"C:\Espressif"
IDF_PATH = r"C:\Users\micke\esp\.espressif\v6.0\esp-idf"
ESP_IDF_VERSION = "6.0.0"
IDF_PYTHON_ENV_PATH = r"C:\Espressif\python_env\idf6.0_py3.13_env"
PROJECT_DIR = os.path.dirname(os.path.abspath(__file__))
LOG_FILE = os.path.join(PROJECT_DIR, "build_log.txt")

TARGETS = ["esp32c3", "esp32c6"]
ACTIONS = ["build", "flash", "monitor", "clean"]


def log(msg):
    print(msg)
    with open(LOG_FILE, "a", encoding="utf-8") as f:
        f.write(msg + "\n")


def run_cmd(cmd_list, cwd=None):
    """Run command and return (returncode, stdout, stderr)"""
    env = os.environ.copy()
    env["IDF_TOOLS_PATH"] = IDF_TOOLS_PATH
    env["IDF_PATH"] = IDF_PATH
    env["ESP_IDF_VERSION"] = ESP_IDF_VERSION
    env["IDF_PYTHON_ENV_PATH"] = IDF_PYTHON_ENV_PATH

    tool_paths = [
        os.path.join(IDF_TOOLS_PATH, "tools", "ninja", "1.12.1"),
        os.path.join(IDF_TOOLS_PATH, "tools", "cmake", "3.30.2", "bin"),
        os.path.join(IDF_TOOLS_PATH, "tools", "idf-exe", "1.0.3"),
        os.path.join(
            IDF_TOOLS_PATH,
            "tools",
            "riscv32-esp-elf",
            "esp-15.2.0_20251204",
            "riscv32-esp-elf",
            "bin",
        ),
        os.path.join(IDF_PYTHON_ENV_PATH, "Scripts"),
    ]
    env["PATH"] = ";".join(tool_paths) + ";" + env.get("PATH", "")

    result = subprocess.run(
        cmd_list,
        cwd=cwd or PROJECT_DIR,
        capture_output=True,
        text=True,
        timeout=600,
        env=env,
    )
    return result.returncode, result.stdout, result.stderr


def setup_idf_env():
    """Initialize ESP-IDF environment by calling export.bat"""
    export_bat = os.path.join(IDF_PATH, "export.bat")
    if not os.path.exists(export_bat):
        log(f"ERROR: {export_bat} not found")
        return False

    result = subprocess.run(
        [export_bat],
        capture_output=True,
        text=True,
        shell=True,
        env=os.environ.copy(),
    )

    if result.returncode != 0:
        log(f"WARNING: export.bat returned {result.returncode}")
        if result.stderr:
            log(f"  stderr: {result.stderr[-500:]}")

    return True


def main():
    if len(sys.argv) < 2:
        print(__doc__)
        sys.exit(1)

    target = sys.argv[1].lower()
    action = (sys.argv[2] if len(sys.argv) > 2 else "build").lower()
    port = sys.argv[3] if len(sys.argv) > 3 else None

    if target not in TARGETS:
        log(f"ERROR: Invalid target '{target}'. Must be one of: {TARGETS}")
        sys.exit(1)
    if action not in ACTIONS:
        log(f"ERROR: Invalid action '{action}'. Must be one of: {ACTIONS}")
        sys.exit(1)

    if os.path.exists(LOG_FILE):
        os.remove(LOG_FILE)

    log("=" * 50)
    log(" ESP32 Blackbox Build Script")
    log(f" Target: {target}")
    log(f" Action: {action}")
    if port:
        log(f" Port:   {port}")
    log("=" * 50)

    idf_python = os.path.join(IDF_PYTHON_ENV_PATH, "Scripts", "python.exe")
    idf_py = os.path.join(IDF_PATH, "tools", "idf.py")
    if not os.path.exists(idf_python):
        log(f"ERROR: Python not found at {idf_python}")
        sys.exit(1)
    if not os.path.exists(idf_py):
        log(f"ERROR: idf.py not found at {idf_py}")
        sys.exit(1)

    log(f"[INFO] IDF Path: {IDF_PATH}")
    log(f"[INFO] Project: {PROJECT_DIR}")

    sdkconfig_path = os.path.join(PROJECT_DIR, "sdkconfig")

    if action == "clean":
        build_dir = os.path.join(PROJECT_DIR, "build")
        if os.path.exists(build_dir):
            shutil.rmtree(build_dir)
            log("[1/3] Cleaned build directory")
        for f in [sdkconfig_path, sdkconfig_path + ".old"]:
            if os.path.exists(f):
                os.remove(f)
                log(f"[1/3] Removed {os.path.basename(f)}")

        log(f"[2/3] Setting target to {target}...")
        rc, out, err = run_cmd([idf_python, idf_py, "set-target", target])
        log(out)
        if err.strip():
            log(err)
        if rc != 0:
            log(f"ERROR: set-target failed (rc={rc})")
            sys.exit(rc)

        log("[3/3] Building...")
        rc, out, err = run_cmd([idf_python, idf_py, "build"])
        log(out)
        if err.strip():
            log(err)
        if rc == 0:
            log("=" * 50)
            log(" BUILD SUCCESS")
            log("=" * 50)
        else:
            log(f"ERROR: Build failed (rc={rc})")
        sys.exit(rc)

    need_set_target = False
    if not os.path.exists(sdkconfig_path):
        log(f"[1/3] First build - setting target to {target}...")
        need_set_target = True
    else:
        with open(sdkconfig_path, "r", encoding="utf-8") as f:
            content = f.read()
        if f'CONFIG_IDF_TARGET="{target}"' not in content:
            log(f"[1/3] Target changed - reconfiguring to {target}...")
            need_set_target = True

    if need_set_target:
        rc, out, err = run_cmd([idf_python, idf_py, "set-target", target])
        log(out)
        if err.strip() and "deprecated" not in err.lower():
            log(err)
        if rc != 0:
            log(f"ERROR: set-target failed (rc={rc})")
            sys.exit(rc)

    log("[2/3] Building...")
    rc, out, err = run_cmd([idf_python, idf_py, "build"])
    log(out)
    if err.strip() and "warning" in err.lower():
        log(err)
    if rc != 0:
        log(f"ERROR: Build failed (rc={rc})")
        sys.exit(rc)

    flash_args = []
    if port:
        flash_args = ["-p", port]

    if action == "flash":
        log("[3/3] Flashing...")
        rc, out, err = run_cmd(
            [idf_python, idf_py] + flash_args + ["flash"]
        )
        log(out)
        if err.strip():
            log(err)
        if rc != 0:
            log(f"ERROR: Flash failed (rc={rc})")
            sys.exit(rc)
        log(" FLASH SUCCESS")

    elif action == "monitor":
        log("[3/3] Flashing and monitoring (Ctrl+] to exit)...")
        log("NOTE: Monitor mode requires interactive terminal.")
        log("      Run manually: idf.py flash monitor")
        rc, out, err = run_cmd(
            [idf_python, idf_py] + flash_args + ["flash"],
        )
        log(out)
        if err.strip():
            log(err)
        if rc != 0:
            log(f"ERROR: Flash failed (rc={rc})")
            sys.exit(rc)
        log(" FLASH SUCCESS. Run 'idf.py monitor' separately.")

    elif action == "build":
        log("=" * 50)
        log(" BUILD SUCCESS")
        firmware = os.path.join(PROJECT_DIR, "build", "esp32-blackbox.bin")
        if os.path.exists(firmware):
            size = os.path.getsize(firmware)
            log(f" Firmware: {firmware}")
            log(f" Size:    {size:,} bytes ({size/1024:.1f} KB)")
        log("=" * 50)

    sys.exit(0)


if __name__ == "__main__":
    main()
