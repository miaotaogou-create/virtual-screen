from __future__ import annotations

import json
import sys
from copy import deepcopy
from pathlib import Path
from typing import Any


def app_root() -> Path:
    """便携 exe 旁目录；开发时用仓库根目录。"""
    if getattr(sys, "frozen", False):
        return Path(sys.executable).resolve().parent
    return Path(__file__).resolve().parent.parent


ROOT = app_root()
DEFAULT_CONFIG_PATH = ROOT / "config.json"
EXAMPLE_CONFIG_PATH = ROOT / "config.example.json"

DEFAULT_CONFIG: dict[str, Any] = {
    "displays": [
        {"width": 1920, "height": 1080, "scale": 125, "hz": 60, "label": "虚拟屏1"},
        {"width": 1920, "height": 1080, "scale": 100, "hz": 60, "label": "虚拟屏2"},
    ],
    "preview_max_height": 320,
    "preview_fps": 5,
    "vdd_settings_path": r"C:\VirtualDisplayDriver\vdd_settings.xml",
}


def load_config(path: Path | None = None) -> dict[str, Any]:
    p = path or DEFAULT_CONFIG_PATH
    if not p.is_file():
        cfg = deepcopy(DEFAULT_CONFIG)
        if EXAMPLE_CONFIG_PATH.is_file():
            cfg = json.loads(EXAMPLE_CONFIG_PATH.read_text(encoding="utf-8"))
        save_config(cfg, p)
        return cfg
    return json.loads(p.read_text(encoding="utf-8"))


def save_config(cfg: dict[str, Any], path: Path | None = None) -> None:
    p = path or DEFAULT_CONFIG_PATH
    p.write_text(json.dumps(cfg, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")


def validate_config(cfg: dict[str, Any]) -> list[str]:
    errs: list[str] = []
    displays = cfg.get("displays")
    if not isinstance(displays, list) or not displays:
        errs.append("displays 必须是非空数组")
        return errs
    for i, d in enumerate(displays):
        for key in ("width", "height", "scale", "hz"):
            if key not in d:
                errs.append(f"displays[{i}] 缺少 {key}")
                continue
            try:
                v = int(d[key])
            except (TypeError, ValueError):
                errs.append(f"displays[{i}].{key} 必须是整数")
                continue
            if key in ("width", "height") and v < 640:
                errs.append(f"displays[{i}].{key} 过小（建议 ≥ 640）")
            if key == "scale" and v < 100:
                errs.append(f"displays[{i}].scale 过小（建议 ≥ 100）")
            if key == "hz" and v < 30:
                errs.append(f"displays[{i}].hz 过小（建议 ≥ 30）")
    return errs
