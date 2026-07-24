"""最小自检：配置读写 + 能抓到至少一块屏。"""

from __future__ import annotations

import sys
import tempfile
from pathlib import Path

from vscreen import config as cfgmod
from vscreen import win_display


def main() -> int:
    cfg = cfgmod.load_config()
    errs = cfgmod.validate_config(cfg)
    assert not errs, errs

    with tempfile.TemporaryDirectory() as td:
        p = Path(td) / "c.json"
        cfgmod.save_config(cfg, p)
        cfg2 = cfgmod.load_config(p)
        assert cfg2["displays"][0]["width"] == cfg["displays"][0]["width"]

    mons = win_display.list_monitors()
    assert mons, "枚举不到监视器"
    mon = mons[0]
    rgb = win_display.capture_monitor_rgb(mon, 64, 36)
    assert len(rgb) == 64 * 36 * 3
    # 不全黑也不全白才算抓到桌面（极简启发式）
    assert any(b != 0 for b in rgb[::17]), "抓屏结果异常偏空"
    print("selfcheck ok:", f"{len(mons)} monitors, capture {mon.device_name}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
