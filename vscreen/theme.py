"""界面主题：浅色 + 青绿强调（对齐本机其他工具观感）。"""

from __future__ import annotations

import tkinter as tk
from tkinter import ttk

C = {
    "bg": "#EEF2F6",
    "surface": "#FFFFFF",
    "surface2": "#F7FAFC",
    "border": "#D8E0EA",
    "text": "#0F1C2E",
    "muted": "#5B6B7C",
    "primary": "#0F766E",
    "primary_hover": "#0D9488",
    "primary_fg": "#FFFFFF",
    "accent_soft": "#CCFBF1",
    "danger": "#B91C1C",
    "preview_bg": "#0B1220",
    "bar": "#0F766E",
}


def font(size: int = 10, weight: str = "normal") -> tuple:
    return ("Microsoft YaHei UI", size, weight)


def apply_theme(root: tk.Tk) -> ttk.Style:
    root.configure(bg=C["bg"])
    style = ttk.Style(root)
    try:
        style.theme_use("clam")
    except tk.TclError:
        pass

    style.configure(".", background=C["bg"], foreground=C["text"], font=font(10))
    style.configure("TFrame", background=C["bg"])
    style.configure("Surface.TFrame", background=C["surface"])
    style.configure("Bar.TFrame", background=C["bar"])
    style.configure("Preview.TFrame", background=C["preview_bg"])

    style.configure("TLabel", background=C["bg"], foreground=C["text"], font=font(10))
    style.configure("Bar.TLabel", background=C["bar"], foreground="#FFFFFF", font=font(11, "bold"))
    style.configure("BarMuted.TLabel", background=C["bar"], foreground="#CCFBF1", font=font(9))
    style.configure("Muted.TLabel", background=C["bg"], foreground=C["muted"], font=font(9))
    style.configure("Surface.TLabel", background=C["surface"], foreground=C["text"], font=font(10))
    style.configure("Caption.TLabel", background=C["preview_bg"], foreground="#94A3B8", font=font(9))
    style.configure("Section.TLabel", background=C["surface"], foreground=C["primary"], font=font(10, "bold"))

    style.configure(
        "TEntry",
        fieldbackground=C["surface2"],
        foreground=C["text"],
        bordercolor=C["border"],
        insertcolor=C["text"],
        padding=5,
    )
    style.map("TEntry", bordercolor=[("focus", C["primary"])])

    style.configure(
        "TButton",
        background=C["surface"],
        foreground=C["text"],
        bordercolor=C["border"],
        padding=(12, 7),
        font=font(9),
    )
    style.map("TButton", background=[("active", C["accent_soft"]), ("pressed", "#99F6E4")])

    style.configure(
        "Primary.TButton",
        background=C["primary"],
        foreground=C["primary_fg"],
        bordercolor=C["primary"],
        padding=(14, 8),
        font=font(10, "bold"),
    )
    style.map(
        "Primary.TButton",
        background=[("active", C["primary_hover"]), ("pressed", "#134E4A")],
    )

    style.configure(
        "Ghost.TButton",
        background=C["bar"],
        foreground="#FFFFFF",
        bordercolor=C["bar"],
        padding=(10, 6),
        font=font(9),
    )
    style.map("Ghost.TButton", background=[("active", C["primary_hover"])])

    style.configure(
        "Danger.TButton",
        background="#FEE2E2",
        foreground=C["danger"],
        bordercolor="#FECACA",
        padding=(12, 7),
        font=font(9),
    )
    return style
