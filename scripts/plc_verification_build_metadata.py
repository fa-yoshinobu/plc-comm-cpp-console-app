Import("env")

import hashlib
import os
import platformio
import subprocess
from pathlib import Path


def git_bytes(repo, *args):
    git_environment = os.environ.copy()
    git_environment["GIT_OPTIONAL_LOCKS"] = "0"
    completed = subprocess.run(
        ["git", "--no-optional-locks", "-C", str(repo), *args],
        check=False,
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
        env=git_environment,
    )
    if completed.returncode != 0:
        detail = completed.stderr.decode("utf-8", errors="replace").strip()
        raise RuntimeError(
            "Cannot create verification-firmware provenance for "
            + str(repo)
            + ": "
            + (detail or "git command failed")
        )
    return completed.stdout


def source_id(repo):
    head_raw = git_bytes(repo, "rev-parse", "--verify", "HEAD")
    status = git_bytes(
        repo,
        "status",
        "--porcelain=v1",
        "-z",
        "--untracked-files=all",
    )
    head = head_raw.decode("ascii", errors="replace").strip()[:12]
    if len(head) != 12:
        raise RuntimeError("Git returned an invalid HEAD for " + str(repo))
    if not status:
        return head + "-clean"

    digest = hashlib.sha256()
    digest.update(status)
    diff = git_bytes(repo, "diff", "--binary", "HEAD", "--", ".")
    digest.update(diff)
    for record in status.split(b"\0"):
        if not record.startswith(b"?? "):
            continue
        relative = os.fsdecode(record[3:])
        path = repo / relative
        if path.is_file():
            digest.update(relative.encode("utf-8", errors="surrogateescape"))
            digest.update(path.read_bytes())
    return head + "-dirty." + digest.hexdigest()[:12]


def quoted_macro(value):
    escaped = value.replace("\\", "\\\\").replace('"', '\\"')
    return '\\"' + escaped + '\\"'


project = Path(env.subst("$PROJECT_DIR"))
environment = env.subst("$PIOENV")
platformio_core = platformio.__version__
if not platformio_core:
    raise RuntimeError("Cannot determine the PlatformIO Core version")
app_source = source_id(project)

if environment.endswith("-local"):
    slmp_source = source_id(project.parent / "plc-comm-slmp-cpp-minimal")
    mc_source = source_id(project.parent / "plc-comm-mcprotocol-serial-cpp")
else:
    slmp_source = "registry:slmp-connect-cpp-minimal@3.1.0"
    mc_source = "registry:mcprotocol-serial-cpp@3.1.0"

ui_source = (
    "registry:M5Unified@0.2.18+M5GFX@0.2.25"
    if environment.startswith("stamplc-")
    else "none"
)

build_material = "|".join(
    (
        environment,
        "platformio:" + platformio_core,
        app_source,
        slmp_source,
        mc_source,
        ui_source,
    )
)
firmware_build_id = hashlib.sha256(build_material.encode("utf-8")).hexdigest()[:16]

env.Append(
    CPPDEFINES=[
        ("PLC_CONSOLE_BUILD_ENV", quoted_macro(environment)),
        ("PLC_CONSOLE_PIO_CORE_VERSION", quoted_macro(platformio_core)),
        ("PLC_CONSOLE_FIRMWARE_BUILD_ID", quoted_macro(firmware_build_id)),
        ("PLC_CONSOLE_APP_SOURCE_ID", quoted_macro(app_source)),
        ("PLC_CONSOLE_SLMP_SOURCE_ID", quoted_macro(slmp_source)),
        ("PLC_CONSOLE_MC_SOURCE_ID", quoted_macro(mc_source)),
        ("PLC_CONSOLE_UI_SOURCE_ID", quoted_macro(ui_source)),
    ]
)
