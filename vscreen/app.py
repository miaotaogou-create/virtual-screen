from __future__ import annotations

import sys
import threading
import tkinter as tk
from tkinter import messagebox, ttk
from typing import Any, Callable

from . import __version__, config as cfgmod
from . import chrome, elevate, theme, vdd, win_display


class App(tk.Tk):
    def __init__(self, startup_action: str | None = None) -> None:
        super().__init__()
        self.title(f"虚拟屏 VirtualScreen {__version__}")
        self.geometry("1280x800")
        self.minsize(960, 640)
        theme.apply_theme(self)

        self.cfg: dict[str, Any] = cfgmod.load_config()
        self.cfg.setdefault("preview_fps", 2)

        self._photos: list[tk.PhotoImage] = []
        self._preview_index = 0
        self._preview_target_keys: list[str] = []
        self._tab_btns: list[tk.Label] = []
        self._preview_job: str | None = None
        self._settings_open = False
        self._busy = False
        self.row_vars: list[dict[str, tk.Variable]] = []
        self._chrome: chrome.TitleChrome | None = None

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
        admin = "管理员权限" if elevate.is_admin() else "普通权限（启动时若取消 UAC，操作仍会再弹）"
        self._chrome = chrome.TitleChrome(
            self,
            subtitle=admin,
            on_apply=self.on_apply,
            on_clear=self.on_clear,
            on_settings=self.toggle_settings,
            on_close=self.destroy,
        )
        self._chrome.build()

        self.body = ttk.Frame(self)
        self.body.pack(fill=tk.BOTH, expand=True)

        self.preview_host = ttk.Frame(self.body, style="Preview.TFrame")
        self.preview_host.place(relx=0, rely=0, relwidth=1, relheight=1)

        # 一次只预览一块屏；顶栏 Tab 切换，中间区域尽量放大
        self.tab_bar = tk.Frame(self.preview_host, bg=theme.C["preview_bg"], height=40)
        self.tab_bar.pack(fill=tk.X, padx=10, pady=(10, 0))
        self.tab_bar.pack_propagate(False)

        stage = ttk.Frame(self.preview_host, style="Preview.TFrame", padding=(10, 8, 10, 10))
        stage.pack(fill=tk.BOTH, expand=True)
        self.preview_cap = ttk.Label(stage, text="", style="Caption.TLabel")
        self.preview_cap.pack(anchor=tk.W)
        self.preview_label = ttk.Label(stage, style="Caption.TLabel", anchor="center")
        self.preview_label.pack(fill=tk.BOTH, expand=True)

        self.settings_panel = ttk.Frame(self.body, style="Surface.TFrame", padding=16)

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

    def _monitor_title(self, mon: win_display.MonitorInfo) -> str:
        title = f"{mon.device_name}  {mon.width}×{mon.height}"
        if mon.likely_virtual:
            title += "  · 虚拟"
        return title

    def _tab_label(self, mon: win_display.MonitorInfo, index: int) -> str:
        name = (mon.adapter_name or mon.monitor_name or "").strip()
        if mon.likely_virtual:
            return f"虚拟屏 {index + 1}"
        short = mon.device_name.replace("\\\\.\\", "")
        return name[:10] or short

    def _select_preview_tab(self, index: int) -> None:
        if index == self._preview_index:
            return
        self._preview_index = index
        self._refresh_tab_styles()
        try:
            self._draw_preview()
        except Exception as e:
            self.status.set(f"预览异常: {e}")

    def _refresh_tab_styles(self) -> None:
        for i, btn in enumerate(self._tab_btns):
            active = i == self._preview_index
            btn.configure(
                bg=theme.C["primary"] if active else theme.C["preview_bg"],
                fg="#FFFFFF" if active else "#94A3B8",
                highlightbackground=theme.C["primary"] if active else "#334155",
            )

    def _sync_preview_tabs(self, targets: list[win_display.MonitorInfo]) -> None:
        keys = [f"{m.device_name}:{m.width}x{m.height}" for m in targets]
        if keys == self._preview_target_keys and len(self._tab_btns) == len(targets):
            return
        self._preview_target_keys = keys
        for w in self.tab_bar.winfo_children():
            w.destroy()
        self._tab_btns.clear()
        if self._preview_index >= len(targets):
            self._preview_index = max(0, len(targets) - 1)
        for i, mon in enumerate(targets):
            btn = tk.Label(
                self.tab_bar,
                text=self._tab_label(mon, i),
                font=theme.font(9),
                padx=14,
                pady=6,
                cursor="hand2",
                highlightthickness=1,
                highlightbackground="#334155",
                bd=0,
            )
            btn.pack(side=tk.LEFT, padx=(0, 6), pady=4)
            btn.bind("<Button-1>", lambda _e, idx=i: self._select_preview_tab(idx))
            self._tab_btns.append(btn)
        self._refresh_tab_styles()

    def _draw_preview(self) -> None:
        targets = win_display.preview_targets(prefer_virtual=True)
        self.update_idletasks()
        # 扣除 Tab 栏与标题，中间区域尽量放大单屏
        host_w = max(320, self.preview_label.winfo_width() or self.preview_host.winfo_width() - 20)
        host_h = max(240, self.preview_label.winfo_height() or (self.preview_host.winfo_height() - 70))
        if not targets:
            self._sync_preview_tabs([])
            self.preview_cap.configure(text="没有可预览的监视器")
            self.preview_label.configure(image="", text="")
            self.footer_info.set("")
            self._photos = []
            return

        self._sync_preview_tabs(targets)
        idx = min(self._preview_index, len(targets) - 1)
        self._preview_index = idx
        mon = targets[idx]
        title = self._monitor_title(mon)
        self.preview_cap.configure(text=title)
        foot_bits = [self._monitor_title(m) for m in targets]
        self.footer_info.set(f"预览 {idx + 1}/{len(targets)}  ·  " + "  |  ".join(foot_bits))

        scale = min(host_w / max(1, mon.width), host_h / max(1, mon.height), 1.0)
        out_w = max(1, int(mon.width * scale))
        out_h = max(1, int(mon.height * scale))
        try:
            rgb = win_display.capture_monitor_rgb(mon, out_w, out_h)
            ppm = win_display.rgb_to_ppm(rgb, out_w, out_h)
            img = tk.PhotoImage(data=ppm)
            self.preview_label.configure(image=img, text="")
            self._photos = [img]  # 防 GC
        except Exception as e:
            self.preview_label.configure(image="", text=f"抓屏失败: {e}")
            self._photos = []


def main() -> None:
    action = None
    for a in ("install-driver", "apply", "clear"):
        if f"--{a}" in sys.argv:
            action = a
            break
    app = App(startup_action=action)
    app.mainloop()
