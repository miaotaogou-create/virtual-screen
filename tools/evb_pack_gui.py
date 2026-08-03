# -*- coding: utf-8 -*-
"""驱动 EVB：把 stage 里的 Qt DLL/插件打进单文件。"""
from __future__ import annotations

import shutil
import time
from pathlib import Path

from pywinauto import Application, Desktop, mouse
from pywinauto.keyboard import send_keys

ROOT = Path(r"C:\ZYL\workspace\projects\virtual-screen")
STAGE = ROOT / "build" / "evb_stage"
OUT = ROOT / "dist" / "portable" / "VirtualScreen.exe"
PROJECT = ROOT / "pack" / "VirtualScreen.evb"
INPUT = STAGE / "VirtualScreen.exe"
EVB = Path(r"C:\ZYL\tools\enigma-vb\app\enigmavb.exe")


def click_center(ctrl):
    r = ctrl.rectangle()
    mouse.click(coords=((r.left + r.right) // 2, (r.top + r.bottom) // 2))


def dismiss_dialogs(main_handle=None, rounds=10):
    for _ in range(rounds):
        clicked = False
        for w in Desktop(backend="uia").windows():
            if main_handle and w.handle == main_handle:
                continue
            cls = w.class_name() or ""
            if cls not in ("#32770",) and "Form" not in cls and "Tnt" not in cls and "Tfrm" not in cls:
                continue
            for title in ("确定", "OK", "&OK", "是", "Yes", "关闭", "Close"):
                try:
                    btn = w.child_window(title=title, control_type="Button")
                    if btn.exists(timeout=0.2):
                        click_center(btn)
                        clicked = True
                        time.sleep(0.35)
                        break
                except Exception:
                    pass
        if not clicked:
            break


def find_file_dialog(timeout=8):
    """带「打开(O)/选择文件夹」的对话框，避免误匹配其它 #32770。"""
    end = time.time() + timeout
    while time.time() < end:
        for w in Desktop(backend="uia").windows():
            if w.class_name() != "#32770":
                continue
            try:
                texts = [(b.window_text() or "") for b in w.descendants(control_type="Button")]
            except Exception:
                continue
            joined = " ".join(texts)
            if "打开(O)" in joined or "打开(&O)" in joined or "&Open" in joined or "选择文件夹" in joined:
                return w
            if w.descendants(control_type="ListItem") and any(t in ("打开", "Open", "取消", "Cancel") for t in texts):
                return w
        time.sleep(0.2)
    return None


def dialog_still_open():
    return find_file_dialog(timeout=0.4) is not None


def set_dialog_path(dlg, path: Path):
    dlg.set_focus()
    send_keys("%d")
    time.sleep(0.15)
    send_keys("^a")
    send_keys(str(path), with_spaces=True)
    send_keys("{ENTER}")
    time.sleep(0.7)


def click_open(dlg):
    preferred = []
    fallback = []
    for b in dlg.descendants(control_type="Button"):
        t = (b.window_text() or "").strip()
        if t in ("打开(O)", "打开(&O)", "&Open", "Open", "选择文件夹", "选择文件夹(F)"):
            preferred.append(b)
        elif t in ("打开", "Open") or t.startswith("打开("):
            preferred.append(b)
        elif "选择" in t and "文件夹" in t:
            preferred.append(b)
    if preferred:
        click_center(preferred[0])
        return
    # 绝不点「打开上一页」之类
    for b in dlg.descendants(control_type="Button"):
        t = (b.window_text() or "").strip()
        if t == "打开" or t.startswith("打开("):
            fallback.append(b)
    if fallback:
        click_center(fallback[0])
        return
    send_keys("{ENTER}")


def add_menu_item(win, item_title: str):
    for attempt in range(4):
        dismiss_dialogs(win.handle, rounds=4)
        win.set_focus()
        time.sleep(0.25)
        click_center(win.child_window(title="Add...", control_type="Button"))
        time.sleep(0.55)
        popup = None
        for w in Desktop(backend="uia").windows():
            if w.class_name() == "#32768":
                # 只要带 MenuItem 的
                try:
                    items = [c.window_text() for c in w.descendants(control_type="MenuItem")]
                except Exception:
                    items = []
                if any(items):
                    popup = w
                    print("menu", items)
                    break
        if not popup:
            send_keys("{ESC}")
            time.sleep(0.2)
            continue
        for c in popup.descendants(control_type="MenuItem"):
            if c.window_text() == item_title:
                click_center(c)
                return
        send_keys("{ESC}")
        time.sleep(0.2)
    raise SystemExit(f"Add 菜单未出现或无此项: {item_title}")


def add_files(win, names: list[str]):
    add_menu_item(win, "Add File(s)")
    dlg = find_file_dialog()
    if not dlg:
        raise SystemExit("文件对话框未出现")
    set_dialog_path(dlg, STAGE)
    time.sleep(0.5)
    dlg = find_file_dialog() or dlg
    items = {}
    for it in dlg.descendants(control_type="ListItem"):
        name = it.window_text() or ""
        if name:
            items[name] = it
    print("dialog items", list(items.keys()))
    to_click = [items[n] for n in names if n in items]
    if not to_click:
        raise SystemExit(f"找不到文件: {names}")
    click_center(to_click[0])
    time.sleep(0.08)
    send_keys("{VK_CONTROL down}")
    try:
        for it in to_click[1:]:
            click_center(it)
            time.sleep(0.04)
    finally:
        send_keys("{VK_CONTROL up}")
    click_open(dlg)
    for _ in range(30):
        time.sleep(0.2)
        if not dialog_still_open():
            break
    else:
        send_keys("{ESC}")
        time.sleep(0.3)
    dismiss_dialogs(win.handle, rounds=10)
    win.set_focus()
    time.sleep(0.4)


def find_folder_dialog(timeout=8):
    """「浏览文件夹」对话框（树 + 确定/取消）。"""
    end = time.time() + timeout
    while time.time() < end:
        for w in Desktop(backend="uia").windows():
            if w.class_name() != "#32770":
                continue
            try:
                btns = [(b.window_text() or "").strip() for b in w.descendants(control_type="Button")]
                trees = w.descendants(control_type="TreeItem")
            except Exception:
                continue
            if ("确定" in btns or "OK" in btns) and ("取消" in btns or "Cancel" in btns) and trees:
                return w
            title = w.window_text() or ""
            if "浏览" in title or "Browse" in title:
                return w
        time.sleep(0.2)
    return None


def add_folder(win, folder: Path):
    add_menu_item(win, "Add Folder Recursive")
    dlg = find_folder_dialog(timeout=10)
    if not dlg:
        raise SystemExit("文件夹对话框未出现")
    dlg.set_focus()
    time.sleep(0.2)
    # 文件夹(F): 编辑框填绝对路径
    edits = [e for e in dlg.descendants(control_type="Edit") if e.is_visible()]
    if edits:
        # 通常最后一个或带「文件夹」标签的
        target_edit = edits[-1]
        try:
            target_edit.set_edit_text(str(folder))
        except Exception:
            click_center(target_edit)
            send_keys("^a")
            send_keys(str(folder), with_spaces=True)
    else:
        send_keys(str(folder), with_spaces=True)
    time.sleep(0.3)
    clicked = False
    for b in dlg.descendants(control_type="Button"):
        t = (b.window_text() or "").strip()
        if t in ("确定", "OK", "&OK"):
            click_center(b)
            clicked = True
            break
    if not clicked:
        send_keys("{ENTER}")
    for _ in range(30):
        time.sleep(0.2)
        if find_folder_dialog(timeout=0.3) is None:
            break
    else:
        send_keys("{ESC}")
        time.sleep(0.3)
    dismiss_dialogs(win.handle, rounds=12)
    win.set_focus()
    time.sleep(0.4)


def main():
    OUT.parent.mkdir(parents=True, exist_ok=True)
    PROJECT.parent.mkdir(parents=True, exist_ok=True)
    if not INPUT.exists():
        raise SystemExit(f"缺少 {INPUT}，请先准备 build/evb_stage")
    if OUT.exists():
        try:
            OUT.unlink()
        except Exception as e:
            raise SystemExit(f"无法删除旧输出: {e}")

    try:
        app = Application(backend="uia").connect(path="enigmavb.exe")
    except Exception:
        Application(backend="uia").start(str(EVB))
        time.sleep(2.5)
        app = Application(backend="uia").connect(path="enigmavb.exe")

    win = app.window(class_name="TfrmMain.UnicodeClass")
    win.set_focus()
    time.sleep(0.4)
    send_keys("{ESC}{ESC}")
    time.sleep(0.2)

    edits = sorted(win.descendants(control_type="Edit"), key=lambda e: e.rectangle().top)
    edits[0].set_edit_text(str(INPUT))
    edits[1].set_edit_text(str(OUT))
    print("paths ok")

    # 一次性递归加入整个 stage（含 DLL 与 plugins），比多次 Add 稳
    print("add folder recursive", STAGE)
    add_folder(win, STAGE)

    # 若树里带了 VirtualScreen.exe，尽量删掉（输入 exe 本身不需要再虚拟一份）
    try:
        tree = win.child_window(control_type="Tree")
        for it in tree.descendants(control_type="TreeItem"):
            t = it.window_text() or ""
            if t.lower() == "virtualscreen.exe":
                click_center(it)
                time.sleep(0.2)
                click_center(win.child_window(title="Remove", control_type="Button"))
                time.sleep(0.4)
                dismiss_dialogs(win.handle, rounds=4)
                print("removed VirtualScreen.exe from virtual tree")
                break
    except Exception as e:
        print("skip remove exe:", e)

    print("process...")
    win.set_focus()
    time.sleep(0.3)
    click_center(win.child_window(title="Process", control_type="Button"))
    time.sleep(0.8)
    dismiss_dialogs(win.handle, rounds=8)

    for i in range(240):
        time.sleep(0.5)
        dismiss_dialogs(win.handle, rounds=2)
        if OUT.exists() and OUT.stat().st_size > 1_000_000:
            break
        boxed = STAGE / "VirtualScreen_boxed.exe"
        if boxed.exists() and boxed.stat().st_size > 1_000_000:
            shutil.move(str(boxed), str(OUT))
            break
        if i % 10 == 0:
            print(" waiting", i, OUT.stat().st_size if OUT.exists() else 0)

    print("OUT", OUT.exists(), OUT.stat().st_size if OUT.exists() else 0)
    if not OUT.exists():
        raise SystemExit("打包失败：未生成输出文件")

    win.set_focus()
    send_keys("^s")
    time.sleep(1.0)
    for w in Desktop(backend="uia").windows():
        t = (w.window_text() or "").lower()
        if "save" in t or "保存" in t:
            edits = [e for e in w.descendants(control_type="Edit") if e.is_visible()]
            if edits:
                edits[-1].set_edit_text(str(PROJECT))
                time.sleep(0.2)
                send_keys("{ENTER}")
                time.sleep(0.8)
                send_keys("{LEFT}{ENTER}")
            break

    profiles = ROOT / "dist" / "profiles"
    if profiles.exists():
        dest = OUT.parent / "profiles"
        if dest.exists():
            shutil.rmtree(dest)
        shutil.copytree(profiles, dest)
    ex = ROOT / "dist" / "config.example.json"
    if ex.exists():
        shutil.copy2(ex, OUT.parent / "config.example.json")
        if not (OUT.parent / "config.json").exists():
            shutil.copy2(ex, OUT.parent / "config.json")
    print("project", PROJECT.exists())
    print("DONE", OUT)


if __name__ == "__main__":
    main()
