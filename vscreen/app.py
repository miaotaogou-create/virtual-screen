from __future__ import annotations

import sys
import threading
import tkinter as tk
from tkinter import messagebox, ttk
from typing import Any, Callable

from . import __version__, config as cfgmod
from . import elevate, theme, vdd, win_display


class App(tk.Tk):
    def __init__(self, startup_action: str | None = None) -> None:
        super().__init__()
        self.title(f"虚拟屏 VirtualScreen {__version__}")
        self.geometry("1280x800")
        self.minsize(960, 640)
        theme.apply_theme(self)

        self.cfg: dict[str, Any] = cfgmod.load_config()
        # 预览默认更稳：2fps，避免整页闪烁感
        self.cfg.setdefault("preview_fps", 2)

        self._photos: list[tk.PhotoImage] = []
        self._preview_labels: list[ttk.Label] = []
        self._preview_caps: list[ttk.Label] = []
        self._preview_job: str | None = None
        self._settings_open = False
        self._busy = False
        self.row_vars: list[dict[str, tk.Variable]] = []

        self._build()
        self._load_fields_from_cfg()
        self._tick_preview()
        if startup_action:
            self.after(200, lambda: self._run_startup_action(startup_action))

    def _run_startup_action(self, action: str) -> None:
        if action == "install-driver":
            self.on_install_driver(already_elevated=True)
        elif action == "apply":
            self.on_apply(already_elevated=True)
        elif action == "clear":
            self.on_clear(already_elevated=True)

    def _build(self) -> None:
        # 顶栏：操作；参数进侧栏，预览占满
        bar = ttk.Frame(self, style="Bar.TFrame", padding=(14, 10))
        bar.pack(fill=tk.X)
        ttk.Label(bar, text="VirtualScreen", style="Bar.TLabel").pack(side=tk.LEFT)
        admin = "管理员" if elevate.is_admin() else "普通权限"
        ttk.Label(bar, text=f"  ·  {admin}", style="BarMuted.TLabel").pack(side=tk.LEFT)

        ttk.Button(bar, text="设置", style="Ghost.TButton", command=self.toggle_settings).pack(
            side=tk.RIGHT, padx=(6, 0)
        )
        ttk.Button(bar, text="清除", style="Ghost.TButton", command=self.on_clear).pack(
            side=tk.RIGHT, padx=(6, 0)
        )
        ttk.Button(bar, text="应用", style="Ghost.TButton", command=self.on_apply).pack(
            side=tk.RIGHT, padx=(6, 0)
        )

        # 主体：预览全幅 + 可滑出的设置层
        self.body = ttk.Frame(self)
        self.body.pack(fill=tk.BOTH, expand=True)

        self.preview_host = ttk.Frame(self.body, style="Preview.TFrame")
        self.preview_host.place(relx=0, rely=0, relwidth=1, relheight=1)

        self.settings_panel = ttk.Frame(self.body, style="Surface.TFrame", padding=16)
        # 默认收起

        foot = ttk.Frame(self, padding=(14, 8))
        foot.pack(fill=tk.X)
        self.status = tk.StringVar(value=self._status_line())
        self.footer_info = tk.StringVar(value="")
        ttk.Label(foot, textvariable=self.footer_info, style="Muted.TLabel").pack(side=tk.LEFT)
        ttk.Label(foot, textvariable=self.status, style="Muted.TLabel").pack(side=tk.RIGHT)

        self._build_settings_content()

    def _build_settings_content(self) -> None:
        p = self.settings_panel
        for w in p.winfo_children():
            w.destroy()

        ttk.Label(p, text="虚拟屏规格", style="Section.TLabel").pack(anchor=tk.W)
        ttk.Label(
            p,
            text="物理像素 × 缩放；应用后被测程序按此逻辑分辨率运行。",
            style="Muted.TLabel",
        ).pack(anchor=tk.W, pady=(2, 10))

        self.rows_frame = ttk.Frame(p, style="Surface.TFrame")
        self.rows_frame.pack(fill=tk.X)

        btns = ttk.Frame(p, style="Surface.TFrame")
        btns.pack(fill=tk.X, pady=(14, 8))
        ttk.Button(btns, text="应用配置", style="Primary.TButton", command=self.on_apply).pack(
            side=tk.LEFT, padx=(0, 8)
        )
        ttk.Button(btns, text="保存", command=self.on_save).pack(side=tk.LEFT, padx=(0, 8))
        ttk.Button(btns, text="安装驱动", command=self.on_install_driver).pack(side=tk.LEFT, padx=(0, 8))
        ttk.Button(btns, text="清除虚拟屏", style="Danger.TButton", command=self.on_clear).pack(
            side=tk.LEFT
        )

        ttk.Label(p, text="监视器", style="Section.TLabel").pack(anchor=tk.W, pady=(12, 4))
        self.list_box = tk.Text(
            p,
            height=10,
            wrap=tk.WORD,
            font=theme.font(9),
            bg=theme.C["surface2"],
            fg=theme.C["text"],
            relief="flat",
            highlightthickness=1,
            highlightbackground=theme.C["border"],
        )
        self.list_box.pack(fill=tk.BOTH, expand=True)
        ttk.Button(p, text="刷新列表", command=self.on_refresh_list).pack(anchor=tk.E, pady=(8, 0))
        ttk.Button(p, text="关闭设置", command=self.toggle_settings).pack(anchor=tk.E, pady=(8, 0))
        self.on_refresh_list()

    def toggle_settings(self) -> None:
        if self._settings_open:
            self.settings_panel.place_forget()
            self._settings_open = False
            return
        # 右侧抽屉，不抢预览主视线
        self.settings_panel.place(relx=1.0, rely=0, anchor="ne", relheight=1, relwidth=0.38, width=420)
        self.settings_panel.lift()
        self._settings_open = True
        self.on_refresh_list()

    def _status_line(self) -> str:
        if vdd.vdd_installed():
            return "驱动已就绪"
        return "未检测到驱动，请打开设置 → 安装驱动"

    def _load_fields_from_cfg(self) -> None:
        for child in self.rows_frame.winfo_children():
            child.destroy()
        self.row_vars.clear()
        for i, d in enumerate(self.cfg.get("displays", [])):
            card = ttk.Frame(self.rows_frame, style="Surface.TFrame", padding=(0, 0, 0, 10))
            card.pack(fill=tk.X)
            ttk.Label(card, text=f"屏 {i + 1}", style="Section.TLabel").pack(anchor=tk.W)
            vars_map: dict[str, tk.Variable] = {}
            grid = ttk.Frame(card, style="Surface.TFrame")
            grid.pack(fill=tk.X, pady=4)
            for col, (key, width, default, tip) in enumerate(
                (
                    ("label", 12, f"虚拟屏{i + 1}", "名称"),
                    ("width", 7, 1920, "宽"),
                    ("height", 7, 1080, "高"),
                    ("scale", 5, 100, "缩放%"),
                    ("hz", 4, 60, "Hz"),
                )
            ):
                cell = ttk.Frame(grid, style="Surface.TFrame")
                cell.grid(row=0, column=col, padx=(0, 8), sticky="w")
                ttk.Label(cell, text=tip, style="Muted.TLabel").pack(anchor=tk.W)
                v = tk.StringVar(value=str(d.get(key, default)))
                ttk.Entry(cell, textvariable=v, width=width).pack(anchor=tk.W)
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

    def _elevate_and_exit(self, action: str) -> None:
        try:
            if not elevate.relaunch_as_admin([f"--{action}"]):
                messagebox.showerror("提权失败", "无法弹出 UAC，或用户已取消。")
                return
        except Exception as e:
            messagebox.showerror("提权失败", str(e))
            return
        self.destroy()
        sys.exit(0)

    def _set_busy(self, busy: bool, tip: str = "") -> None:
        self._busy = busy
        if tip:
            self.status.set(tip)
        # 忙碌时暂停预览抓屏，减轻卡顿感
        if busy and self._preview_job:
            try:
                self.after_cancel(self._preview_job)
            except Exception:
                pass
            self._preview_job = None
        elif not busy and self._preview_job is None:
            self._tick_preview()

    def _run_bg(self, work: Callable[[], str], title: str, busy_tip: str) -> None:
        if self._busy:
            messagebox.showinfo("请稍候", "已有任务在进行中。")
            return

        def progress(msg: str) -> None:
            self.after(0, lambda m=msg: self.status.set(m))

        def runner() -> None:
            try:
                msg = work(progress)
            except Exception as e:
                err = str(e)

                def fail() -> None:
                    self._set_busy(False)
                    self.status.set(f"{title}失败: {err}")
                    messagebox.showerror(title, err)

                self.after(0, fail)
                return

            def ok() -> None:
                self._set_busy(False)
                self.status.set(msg)
                self.on_refresh_list()
                messagebox.showinfo(title, msg)

            self.after(0, ok)

        self._set_busy(True, busy_tip)
        threading.Thread(target=runner, daemon=True).start()

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
        self.status.set("配置已保存")

    def on_install_driver(self, already_elevated: bool = False) -> None:
        if not already_elevated and not elevate.is_admin():
            if not messagebox.askyesno("安装驱动", "将下载并安装 Virtual Display Driver，需要管理员。继续？"):
                return
            self._elevate_and_exit("install-driver")
            return

        def work(progress) -> str:
            progress("正在下载/安装驱动…")
            return vdd.install_driver()

        self._run_bg(work, "安装驱动", "正在安装驱动，界面可继续预览以外的操作…")

    def on_apply(self, already_elevated: bool = False) -> None:
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
        if not already_elevated and not elevate.is_admin():
            self._elevate_and_exit("apply")
            return

        def work(progress) -> str:
            return vdd.apply_config(cfg, progress=progress)

        self._run_bg(work, "应用", "正在应用配置…")

    def on_clear(self, already_elevated: bool = False) -> None:
        if not already_elevated:
            if not messagebox.askyesno("清除", "禁用虚拟显示驱动设备？"):
                return
            if not elevate.is_admin():
                self._elevate_and_exit("clear")
                return

        def work(progress) -> str:
            progress("正在清除虚拟屏…")
            return vdd.clear_virtual_displays()

        self._run_bg(work, "清除", "正在清除虚拟屏…")

    def on_refresh_list(self) -> None:
        if not hasattr(self, "list_box"):
            return
        mons = win_display.list_monitors()
        lines = []
        for m in mons:
            tag = []
            if m.is_primary:
                tag.append("主屏")
            if m.likely_virtual:
                tag.append("虚拟")
            if not m.is_primary and not m.likely_virtual:
                tag.append("副屏")
            lines.append(
                f"{m.device_name}  {m.width}×{m.height}  "
                f"{m.adapter_name or m.monitor_name}  [{','.join(tag)}]"
            )
        self.list_box.delete("1.0", tk.END)
        self.list_box.insert(tk.END, "\n".join(lines) or "（无）")
        self.status.set(self._status_line())

    def _tick_preview(self) -> None:
        try:
            self._draw_preview()
        except Exception as e:
            self.status.set(f"预览异常: {e}")
        fps = max(1, int(self.cfg.get("preview_fps", 2)))
        self._preview_job = self.after(int(1000 / fps), self._tick_preview)

    def _ensure_preview_slots(self, n: int) -> None:
        while len(self._preview_labels) < n:
            fr = ttk.Frame(self.preview_host, style="Preview.TFrame", padding=8)
            fr.pack(side=tk.LEFT, fill=tk.BOTH, expand=True)
            cap = ttk.Label(fr, text="", style="Caption.TLabel")
            cap.pack(anchor=tk.W)
            lbl = ttk.Label(fr, style="Caption.TLabel")
            lbl.pack(fill=tk.BOTH, expand=True)
            self._preview_caps.append(cap)
            self._preview_labels.append(lbl)
        # 多余槽隐藏
        for i, lbl in enumerate(self._preview_labels):
            parent = lbl.master
            if i < n:
                parent.pack(side=tk.LEFT, fill=tk.BOTH, expand=True)
            else:
                parent.pack_forget()

    def _draw_preview(self) -> None:
        targets = win_display.preview_targets(prefer_virtual=True)
        self.update_idletasks()
        host_w = max(320, self.preview_host.winfo_width())
        host_h = max(240, self.preview_host.winfo_height() - 28)
        if not targets:
            self._ensure_preview_slots(1)
            self._preview_caps[0].configure(text="没有可预览的监视器")
            self._preview_labels[0].configure(image="", text="")
            self.footer_info.set("")
            self._photos = []
            return

        self._ensure_preview_slots(len(targets))
        new_photos: list[tk.PhotoImage] = []
        foot_bits: list[str] = []
        slot_w = max(160, host_w // len(targets) - 16)

        for i, mon in enumerate(targets):
            scale = min(slot_w / max(1, mon.width), host_h / max(1, mon.height), 1.0)
            out_w = max(1, int(mon.width * scale))
            out_h = max(1, int(mon.height * scale))
            title = f"{mon.device_name}  {mon.width}×{mon.height}"
            if mon.likely_virtual:
                title += "  · 虚拟"
            self._preview_caps[i].configure(text=title)
            foot_bits.append(title)
            try:
                rgb = win_display.capture_monitor_rgb(mon, out_w, out_h)
                ppm = win_display.rgb_to_ppm(rgb, out_w, out_h)
                img = tk.PhotoImage(data=ppm)
                self._preview_labels[i].configure(image=img, text="")
                new_photos.append(img)
            except Exception as e:
                self._preview_labels[i].configure(image="", text=f"抓屏失败: {e}")

        self._photos = new_photos  # 防 GC
        self.footer_info.set("  |  ".join(foot_bits))


def main() -> None:
    action = None
    for a in ("install-driver", "apply", "clear"):
        if f"--{a}" in sys.argv:
            action = a
            break
    app = App(startup_action=action)
    app.mainloop()
