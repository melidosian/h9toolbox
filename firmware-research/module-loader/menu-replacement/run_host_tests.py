#!/usr/bin/env python3
"""Run the production menu/editor C against the host input and storage harnesses."""
from pathlib import Path
import subprocess
import sys

REPO = Path(__file__).resolve().parents[3]
MODULE = "firmware-research/module-loader"
RADIO = f"{MODULE}/menu-replacement"
cases = [
    ("channel-label-storage", [], [f"{RADIO}/test_channel_labels.c", f"{RADIO}/channel_labels.c",
                                    f"{MODULE}/modules/common/toolbox_settings_pair.c"]),
    ("channel-label-clone", [], [f"{RADIO}/test_channel_label_clone.c", f"{RADIO}/channel_labels.c",
                                  f"{MODULE}/modules/common/toolbox_settings_pair.c"]),
    ("channel-name-view", [], [f"{RADIO}/test_channel_name_view.c", f"{RADIO}/channel_name_scroll.c"]),
    ("channel-name-scroll", [], [f"{RADIO}/test_channel_name_scroll.c", f"{RADIO}/channel_name_scroll.c"]),
    ("aprs-client", [], [f"{MODULE}/modules/aprs/test_aprs.c"]),
    ("menu", [], [f"{MODULE}/modules/menu/test_toolbox_menu.c"]),
    ("preview", ["-DH9_RADIO_MENU_PREVIEW"], [f"{MODULE}/modules/menu/test_toolbox_menu.c"]),
    ("unified", ["-DH9_RADIO_SETTINGS"], [f"{MODULE}/modules/menu/test_toolbox_menu.c", f"{RADIO}/radio_settings.c", f"{RADIO}/radio_actions_catalog.c", f"{RADIO}/radio_aprs.c"]),
    ("settings", [], [f"{RADIO}/test_radio_settings.c", f"{RADIO}/radio_settings.c"]),
    ("editor", [], [f"{RADIO}/test_radio_editor.c", f"{RADIO}/radio_editor.c", f"{RADIO}/radio_settings.c"]),
    ("channels", [], [f"{RADIO}/test_radio_channels.c", f"{RADIO}/radio_channels.c", f"{RADIO}/radio_settings.c"]),
    ("channel-editor", [], [f"{RADIO}/test_radio_channel_editor.c", f"{RADIO}/radio_channel_editor.c",
                            f"{RADIO}/radio_channels.c", f"{RADIO}/radio_settings.c"]),
    ("bluetooth", [], [f"{RADIO}/test_radio_bluetooth.c"]),
    ("scan", [], [f"{RADIO}/test_radio_scan.c"]),
    ("system", ["-DH9_RESIDENT_TX"], [f"{RADIO}/test_radio_system.c", f"{RADIO}/radio_system.c", f"{RADIO}/radio_settings.c"]),
]
commands = ["set -eu"]
for name, flags, sources in cases:
    # These arguments are fixed repository-relative names, without shell metacharacters.
    commands.append(" ".join(["gcc", "-std=c11", "-fno-builtin", "-Wall", "-Werror",
                              f"-I{MODULE}/modules/common", *flags, *sources, "-o", f"/tmp/{name}"]))
    commands.append(f"timeout 10s /tmp/{name}")
result = subprocess.run(["docker", "run", "--rm", "--mount", f"type=bind,source={REPO},target=/src",
                         "-w", "/src", "h9-hostcc", "/bin/sh", "-c", "\n".join(commands)])
sys.exit(result.returncode)
