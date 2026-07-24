"""无系统标题栏：青绿一体顶栏（拖拽 / 最小化 / 最大化 / 关闭）。对齐 qt-arm64-cross。"""

from __future__ import annotations

import ctypes
import tkinter as tk
from ctypes import wintypes

from .theme import C, font

user32 = ctypes.windll.user32
GWL_EXSTYLE = -20
WS_EX_APPWINDOW = 0x00040000
WS_EX_TOOLWINDOW = 0x00000080
SWP_NOSIZE = 0x0001
SWP_NOMOVE = 0x0002
SWP_NOZORDER = 0x0004
SWP_FRAMECHANGED = 0x0020
SWP_SHOWWINDOW = 0x0040

_BTN_W = 40
_BTN_H = 30
_ICON = 6
_STROKE = 1.25


def _hwnd(root: tk.Tk) -> int:
    root.update_idletasks()
    wid = root.winfo_id()
    parent = user32.GetParent(wid)
    return parent or wid


def show_in_taskbar(root: tk.Tk) -> None:
    hwnd = _hwnd(root)
    style = user32.GetWindowLongW(hwnd, GWL_EXSTYLE)
    style = (style & ~WS_EX_TOOLWINDOW) | WS_EX_APPWINDOW
    user32.SetWindowLongW(hwnd, GWL_EXSTYLE, style)
    user32.SetWindowPos(
        hwnd, 0, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_FRAMECHANGED | SWP_SHOWWINDOW
    )


def _paint_icon(cv: tk.Canvas, kind: str, color: str) -> None:
    cv.delete("icon")
    cx, cy = _BTN_W // 2, _BTN_H // 2
    s = _ICON
    w = _STROKE
    if kind == "min":
        cv.create_line(cx - s, cy + 1, cx + s, cy + 1, fill=color, width=w, tags="icon", capstyle=tk.ROUND)
    elif kind == "max":
        cv.create_rectangle(cx - s, cy - s, cx + s, cy + s, outline=color, width=w, tags="icon")
    elif kind == "restore":
        cv.create_rectangle(cx - s + 2, cy - s - 1, cx + s + 1, cy + s - 3, outline=color, width=w, tags="icon")
        cv.create_rectangle(
            cx - s - 1,
            cy - s + 2,
            cx + s - 3,
            cy + s + 1,
            outline=color,
            width=w,
            fill=C["bar"],
            tags="icon",
        )
    elif kind == "close":
        cv.create_line(cx - s, cy - s, cx + s, cy + s, fill=color, width=w, tags="icon", capstyle=tk.ROUND)
        cv.create_line(cx + s, cy - s, cx - s, cy + s, fill=color, width=w, tags="icon", capstyle=tk.ROUND)


class TitleChrome:
    def __init__(self, root: tk.Tk, *, subtitle: str, on_apply, on_clear, on_settings, on_close) -> None:
        self.root = root
        self.on_apply = on_apply
        self.on_clear = on_clear
        self.on_settings = on_settings
        self.on_close = on_close
        self.subtitle = subtitle
        self._drag_x = 0
        self._drag_y = 0
        self._maximized = False
        self._restore_geom = ""
        self._map_guard = False
        self._taskbar_ready = False

    def build(self) -> tk.Frame:
        self.root.overrideredirect(True)
        self.root.configure(highlightthickness=1, highlightbackground=C["bar_deep"], highlightcolor=C["bar_deep"])

        header = tk.Frame(self.root, bg=C["bar"], height=52)
        header.pack(fill=tk.X)
        header.pack_propagate(False)
        tk.Frame(header, bg=C["bar_deep"], height=2).pack(side=tk.BOTTOM, fill=tk.X)

        bar = tk.Frame(header, bg=C["bar"])
        bar.pack(fill=tk.BOTH, expand=True, padx=4)

        left = tk.Frame(bar, bg=C["bar"])
        left.pack(side=tk.LEFT, fill=tk.Y, padx=(12, 8), pady=8)
        mark = tk.Canvas(left, width=28, height=28, bg=C["bar"], highlightthickness=0)
        mark.pack(side=tk.LEFT, padx=(0, 10))
        mark.create_oval(2, 2, 26, 26, fill=C["accent_soft"], outline=C["accent_soft"])
        mark.create_text(14, 14, text="V", fill=C["primary"], font=font(11, "bold"))
        # 单行标题，避免中文副标题被顶栏裁切
        self.title_label = tk.Label(
            left,
            text=f"VirtualScreen  ·  {self.subtitle}",
            bg=C["bar"],
            fg="#FFFFFF",
            font=font(12, "bold"),
            anchor="w",
        )
        self.title_label.pack(side=tk.LEFT, fill=tk.Y)

        # 右侧：操作胶囊 + 窗控（与左侧图标垂直对齐）
        right = tk.Frame(bar, bg=C["bar"])
        right.pack(side=tk.RIGHT, padx=(0, 4), pady=11)

        actions = tk.Frame(right, bg=C["bar"])
        actions.pack(side=tk.LEFT, padx=(0, 8))
        self._pill(actions, "应用", self.on_apply, primary=True).pack(side=tk.LEFT, padx=3)
        self._pill(actions, "清除", self.on_clear).pack(side=tk.LEFT, padx=3)
        self._pill(actions, "设置", self.on_settings).pack(side=tk.LEFT, padx=3)

        winbtns = tk.Frame(right, bg=C["bar"])
        winbtns.pack(side=tk.LEFT)
        self._btn_min = self._chrome_btn(winbtns, "min", self.minimize)
        self._btn_max = self._chrome_btn(winbtns, "max", self.toggle_max)
        self._btn_close = self._chrome_btn(
            winbtns, "close", self.on_close, hover_bg="#DC2626", hover_fg="#FFFFFF"
        )

        for w in (header, bar, left, mark, self.title_label):
            self._bind_drag(w)

        self.root.bind("<Map>", self._on_map)
        self.root.after(80, self._init_taskbar)
        self._add_resize_grip()
        return header

    def _pill(self, parent, text: str, cmd, primary: bool = False) -> tk.Label:
        if primary:
            bg, fg, hbg = "#CCFBF1", C["primary"], "#99F6E4"
        else:
            bg, fg, hbg = "#0D9488", "#FFFFFF", "#14B8A6"
        lab = tk.Label(
            parent,
            text=text,
            bg=bg,
            fg=fg,
            font=font(9, "bold"),
            padx=12,
            pady=4,
            cursor="hand2",
        )
        lab.bind("<Button-1>", lambda _e: cmd())
        lab.bind("<Enter>", lambda _e, b=hbg: lab.configure(bg=b))
        lab.bind("<Leave>", lambda _e, b=bg: lab.configure(bg=b))
        return lab

    def _init_taskbar(self) -> None:
        if self._taskbar_ready:
            return
        show_in_taskbar(self.root)
        self._taskbar_ready = True

    def _add_resize_grip(self) -> None:
        grip = tk.Label(self.root, text="◢", bg=C["preview_bg"], fg="#64748B", cursor="size_nw_se", font=font(8))
        grip.place(relx=1.0, rely=1.0, anchor="se", x=-2, y=-1)
        self._rz = {"x": 0, "y": 0, "w": 0, "h": 0}

        def start(e):
            self._rz = {"x": e.x_root, "y": e.y_root, "w": self.root.winfo_width(), "h": self.root.winfo_height()}

        def move(e):
            if self._maximized:
                return
            nw = max(self.root.minsize()[0], self._rz["w"] + e.x_root - self._rz["x"])
            nh = max(self.root.minsize()[1], self._rz["h"] + e.y_root - self._rz["y"])
            self.root.geometry(f"{nw}x{nh}")

        grip.bind("<ButtonPress-1>", start)
        grip.bind("<B1-Motion>", move)

    def _chrome_btn(self, parent, kind: str, cmd, hover_bg=None, hover_fg=None) -> tk.Canvas:
        hover_bg = hover_bg or C["bar_deep"]
        hover_fg = hover_fg or "#FFFFFF"
        idle_fg = "#E2E8F0"
        cv = tk.Canvas(parent, width=_BTN_W, height=_BTN_H, bg=C["bar"], highlightthickness=0, cursor="hand2")
        cv.pack(side=tk.LEFT)
        _paint_icon(cv, kind, idle_fg)
        cv._icon_kind = kind  # type: ignore[attr-defined]

        def enter(_e, b=hover_bg, f=hover_fg):
            cv.configure(bg=b)
            _paint_icon(cv, getattr(cv, "_icon_kind", kind), f)

        def leave(_e):
            cv.configure(bg=C["bar"])
            _paint_icon(cv, getattr(cv, "_icon_kind", kind), idle_fg)

        cv.bind("<Enter>", enter)
        cv.bind("<Leave>", leave)
        cv.bind("<Button-1>", lambda _e: cmd())
        return cv

    def _set_max_icon(self, kind: str) -> None:
        self._btn_max._icon_kind = kind  # type: ignore[attr-defined]
        _paint_icon(self._btn_max, kind, "#E2E8F0")

    def _bind_drag(self, widget: tk.Misc) -> None:
        widget.bind("<ButtonPress-1>", self._start_drag)
        widget.bind("<B1-Motion>", self._on_drag)
        widget.bind("<Double-Button-1>", lambda _e: self.toggle_max())

    def _start_drag(self, event) -> None:
        if self._maximized:
            return
        self._drag_x = event.x_root - self.root.winfo_x()
        self._drag_y = event.y_root - self.root.winfo_y()

    def _on_drag(self, event) -> None:
        if self._maximized:
            return
        self.root.geometry(f"+{event.x_root - self._drag_x}+{event.y_root - self._drag_y}")

    def minimize(self) -> None:
        self._map_guard = True
        self.root.overrideredirect(False)
        self.root.iconify()
        self.root.after(100, lambda: setattr(self, "_map_guard", False))

    def _on_map(self, _event=None) -> None:
        if self._map_guard or self.root.state() != "normal":
            return
        if not self.root.overrideredirect():
            self._map_guard = True
            self.root.overrideredirect(True)
            show_in_taskbar(self.root)
            self.root.after(100, lambda: setattr(self, "_map_guard", False))

    def toggle_max(self) -> None:
        if not self._maximized:
            self._restore_geom = self.root.geometry()
            try:

                class RECT(ctypes.Structure):
                    _fields_ = [("l", wintypes.LONG), ("t", wintypes.LONG), ("r", wintypes.LONG), ("b", wintypes.LONG)]

                rect = RECT()
                user32.SystemParametersInfoW(0x0030, 0, ctypes.byref(rect), 0)
                self.root.geometry(f"{rect.r - rect.l}x{rect.b - rect.t}+{rect.l}+{rect.t}")
            except Exception:
                self.root.state("zoomed")
            self._maximized = True
            self._set_max_icon("restore")
        else:
            if self._restore_geom:
                self.root.geometry(self._restore_geom)
            self._maximized = False
            self._set_max_icon("max")

    def set_subtitle(self, text: str) -> None:
        self.subtitle = text
        self.title_label.configure(text=f"VirtualScreen  ·  {text}")
