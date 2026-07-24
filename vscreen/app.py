from __future__ import annotations

import tkinter as tk
from tkinter import messagebox, ttk
from typing import Any

from . import __version__, config as cfgmod
from . import vdd, win_display


class App(tk.Tk):
    def __init__(self) -> None:
        super().__init__()
        self.title(f"虚拟屏 virtual-screen {__version__}")
        self.geometry("1100x720")
        self.minsize(900, 600)

        self.cfg: dict[str, Any] = cfgmod.load_config()
        self._photos: list[tk.PhotoImage] = []
        self._preview_job: str | None = None

        self._build()
        self._load_fields_from_cfg()
        self._tick_preview()

    def _build(self) -> None:
        top = ttk.Frame(self, padding=8)
        top.pack(fill=tk.X)

        ttk.Label(top, text="虚拟屏配置（逻辑规格：物理像素 × 缩放）").pack(anchor=tk.W)
        self.rows_frame = ttk.Frame(top)
        self.rows_frame.pack(fill=tk.X, pady=4)
        self.row_vars: list[dict[str, tk.Variable]] = []

        btns = ttk.Frame(top)
        btns.pack(fill=tk.X, pady=4)
        ttk.Button(btns, text="应用", command=self.on_apply).pack(side=tk.LEFT, padx=(0, 6))
        ttk.Button(btns, text="清除虚拟屏", command=self.on_clear).pack(side=tk.LEFT, padx=(0, 6))
        ttk.Button(btns, text="保存配置", command=self.on_save).pack(side=tk.LEFT, padx=(0, 6))
        ttk.Button(btns, text="刷新监视器列表", command=self.on_refresh_list).pack(side=tk.LEFT, padx=(0, 6))
        ttk.Button(btns, text="驱动安装说明", command=self.on_driver_help).pack(side=tk.LEFT)

        self.status = tk.StringVar(value=self._status_line())
        ttk.Label(top, textvariable=self.status, wraplength=1060).pack(anchor=tk.W, pady=(4, 0))

        mid = ttk.Frame(self, padding=8)
        mid.pack(fill=tk.BOTH, expand=True)
        ttk.Label(mid, text="预览（优先抓虚拟屏；若无则抓非主屏，画面按比例缩小）").pack(anchor=tk.W)
        self.preview_host = ttk.Frame(mid)
        self.preview_host.pack(fill=tk.BOTH, expand=True, pady=4)

        self.list_box = tk.Text(self, height=6, wrap=tk.WORD)
        self.list_box.pack(fill=tk.X, padx=8, pady=(0, 8))
        self.on_refresh_list()

    def _status_line(self) -> str:
        inst = "已检测到驱动相关设备/目录" if vdd.vdd_installed() else "未检测到 Virtual Display Driver（应用前需安装）"
        return inst

    def _load_fields_from_cfg(self) -> None:
        for child in self.rows_frame.winfo_children():
            child.destroy()
        self.row_vars.clear()
        for i, d in enumerate(self.cfg.get("displays", [])):
            fr = ttk.Frame(self.rows_frame)
            fr.pack(fill=tk.X, pady=2)
            ttk.Label(fr, text=f"#{i+1}", width=4).pack(side=tk.LEFT)
            vars_map: dict[str, tk.Variable] = {}
            for key, width, default in (
                ("label", 10, f"屏{i+1}"),
                ("width", 6, 1920),
                ("height", 6, 1080),
                ("scale", 5, 100),
                ("hz", 4, 60),
            ):
                ttk.Label(fr, text=key).pack(side=tk.LEFT, padx=(6, 2))
                v = tk.StringVar(value=str(d.get(key, default)))
                ttk.Entry(fr, textvariable=v, width=width).pack(side=tk.LEFT)
                vars_map[key] = v
            self.row_vars.append(vars_map)

    def _cfg_from_fields(self) -> dict[str, Any]:
        displays = []
        for vars_map in self.row_vars:
            displays.append(
                {
                    "label": vars_map["label"].get().strip() or "屏",
                    "width": int(vars_map["width"].get()),
                    "height": int(vars_map["height"].get()),
                    "scale": int(vars_map["scale"].get()),
                    "hz": int(vars_map["hz"].get()),
                }
            )
        out = dict(self.cfg)
        out["displays"] = displays
        return out

    def on_save(self) -> None:
        try:
            cfg = self._cfg_from_fields()
        except ValueError:
            messagebox.showerror("配置错误", "宽/高/缩放/刷新率必须是整数")
            return
        errs = cfgmod.validate_config(cfg)
        if errs:
            messagebox.showerror("配置错误", "\n".join(errs))
            return
        cfgmod.save_config(cfg)
        self.cfg = cfg
        self.status.set("配置已保存到 config.json。 " + self._status_line())

    def on_apply(self) -> None:
        try:
            cfg = self._cfg_from_fields()
        except ValueError:
            messagebox.showerror("配置错误", "宽/高/缩放/刷新率必须是整数")
            return
        errs = cfgmod.validate_config(cfg)
        if errs:
            messagebox.showerror("配置错误", "\n".join(errs))
            return
        cfgmod.save_config(cfg)
        self.cfg = cfg
        try:
            msg = vdd.apply_config(cfg)
        except Exception as e:
            messagebox.showerror("应用失败", str(e))
            self.status.set(f"应用失败: {e}")
            return
        self.status.set(msg)
        self.on_refresh_list()
        messagebox.showinfo("应用", msg)

    def on_clear(self) -> None:
        if not messagebox.askyesno("清除", "禁用虚拟显示驱动设备？（可用设备管理器再启用）"):
            return
        try:
            msg = vdd.clear_virtual_displays()
        except Exception as e:
            messagebox.showerror("清除失败", str(e))
            return
        self.status.set(msg)
        self.on_refresh_list()
        messagebox.showinfo("清除", msg)

    def on_refresh_list(self) -> None:
        mons = win_display.list_monitors()
        lines = ["当前监视器："]
        for m in mons:
            tag = []
            if m.is_primary:
                tag.append("主屏")
            if m.likely_virtual:
                tag.append("疑似虚拟")
            if not m.is_primary and not m.likely_virtual:
                tag.append("副屏")
            lines.append(
                f"- {m.device_name}  {m.width}x{m.height}+{m.left},{m.top}  "
                f"{m.adapter_name or m.monitor_name}  [{','.join(tag) or '普通'}]"
            )
        lines.append("")
        lines.append(self._status_line())
        self.list_box.delete("1.0", tk.END)
        self.list_box.insert(tk.END, "\n".join(lines))

    def on_driver_help(self) -> None:
        messagebox.showinfo(
            "驱动安装",
            "本工具依赖系统级 Virtual Display Driver（IddCx），不自带驱动。\n\n"
            "1. 打开：https://github.com/VirtualDrivers/Virtual-Display-Driver/releases\n"
            "2. 下载并安装签名版驱动（按上游说明）\n"
            "3. 确认出现 C:\\VirtualDisplayDriver\\ 或设备管理器中有对应显示适配器\n"
            "4. 以管理员运行本工具，再点「应用」\n\n"
            "「清除」会 Disable 该 PnP 设备。预览不需要管理员。",
        )

    def _tick_preview(self) -> None:
        try:
            self._draw_preview()
        except Exception as e:
            self.status.set(f"预览异常: {e}")
        fps = max(1, int(self.cfg.get("preview_fps", 5)))
        self._preview_job = self.after(int(1000 / fps), self._tick_preview)

    def _draw_preview(self) -> None:
        targets = win_display.preview_targets(prefer_virtual=True)
        max_h = max(80, int(self.cfg.get("preview_max_height", 320)))
        for child in self.preview_host.winfo_children():
            child.destroy()
        self._photos.clear()
        if not targets:
            ttk.Label(self.preview_host, text="没有可预览的监视器").pack()
            return
        for mon in targets:
            scale = min(1.0, max_h / max(1, mon.height))
            out_w = max(1, int(mon.width * scale))
            out_h = max(1, int(mon.height * scale))
            fr = ttk.Frame(self.preview_host, padding=4)
            fr.pack(side=tk.LEFT, fill=tk.BOTH, expand=True)
            title = f"{mon.device_name}  {mon.width}x{mon.height}"
            if mon.likely_virtual:
                title += "  (虚拟)"
            ttk.Label(fr, text=title).pack(anchor=tk.W)
            try:
                rgb = win_display.capture_monitor_rgb(mon, out_w, out_h)
                ppm = win_display.rgb_to_ppm(rgb, out_w, out_h)
                img = tk.PhotoImage(data=ppm)
            except Exception as e:
                ttk.Label(fr, text=f"抓屏失败: {e}").pack()
                continue
            self._photos.append(img)  # 防 GC
            lbl = ttk.Label(fr, image=img)
            lbl.pack()


def main() -> None:
    app = App()
    app.mainloop()
