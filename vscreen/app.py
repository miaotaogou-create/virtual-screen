from __future__ import annotations

import json
import socket
import struct
import sys
import threading
import time
import webbrowser
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
from pathlib import Path
from typing import Any
from urllib.parse import urlparse

from . import __version__, config as cfgmod
from . import elevate, vdd, win_display

HOST = "127.0.0.1"
PORT_BASE = 17832


def rgb_to_bmp(rgb: bytes, w: int, h: int) -> bytes:
    """RGB24 → BMP（无第三方库）。"""
    row = (w * 3 + 3) & ~3
    pad = row - w * 3
    pixel = bytearray()
    for y in range(h - 1, -1, -1):
        i = y * w * 3
        for x in range(w):
            r, g, b = rgb[i], rgb[i + 1], rgb[i + 2]
            pixel.extend((b, g, r))
            i += 3
        pixel.extend(b"\x00" * pad)
    file_header = struct.pack("<2sIHHI", b"BM", 54 + len(pixel), 0, 0, 54)
    info_header = struct.pack(
        "<IiiHHIIiiii",
        40,
        w,
        h,
        1,
        24,
        0,
        len(pixel),
        2835,
        2835,
        0,
        0,
    )
    return file_header + info_header + pixel


INDEX_HTML = r"""<!DOCTYPE html>
<html lang="zh-CN">
<head>
<meta charset="utf-8"/>
<meta name="viewport" content="width=device-width, initial-scale=1"/>
<title>虚拟屏 VirtualScreen</title>
<style>
  :root { --bg:#0f1419; --card:#1a2332; --fg:#e7ecf3; --mut:#8b9bb4; --acc:#3d8bfd; --ok:#3dd68c; --bad:#ff6b6b; }
  * { box-sizing: border-box; }
  body { margin:0; font:14px/1.45 system-ui, "Segoe UI", sans-serif; background:var(--bg); color:var(--fg); }
  header { padding:16px 20px; border-bottom:1px solid #243044; display:flex; gap:12px; align-items:center; flex-wrap:wrap; }
  h1 { margin:0; font-size:18px; font-weight:600; }
  .pill { font-size:12px; color:var(--mut); padding:2px 8px; border:1px solid #31425c; border-radius:999px; }
  .pill.ok { color:var(--ok); border-color:#245c45; }
  .pill.bad { color:var(--bad); border-color:#6b2e2e; }
  main { padding:16px 20px 32px; display:grid; gap:16px; }
  .card { background:var(--card); border:1px solid #243044; border-radius:10px; padding:14px; }
  .row { display:flex; gap:8px; flex-wrap:wrap; align-items:center; margin:6px 0; }
  label { color:var(--mut); min-width:42px; }
  input { background:#0f1419; border:1px solid #31425c; color:var(--fg); border-radius:6px; padding:6px 8px; width:90px; }
  input.wide { width:120px; }
  button { background:var(--acc); color:#fff; border:0; border-radius:8px; padding:8px 14px; cursor:pointer; font-weight:600; }
  button.secondary { background:#31425c; }
  button.danger { background:#a33b3b; }
  button:disabled { opacity:.5; cursor:wait; }
  #status { color:var(--mut); white-space:pre-wrap; }
  #monitors { color:var(--mut); white-space:pre-wrap; font-family:ui-monospace, Consolas, monospace; font-size:12px; }
  .previews { display:flex; gap:12px; flex-wrap:wrap; }
  .previews figure { margin:0; }
  .previews img { max-height:320px; background:#000; border:1px solid #31425c; border-radius:6px; display:block; }
  .previews figcaption { color:var(--mut); font-size:12px; margin-top:4px; }
</style>
</head>
<body>
<header>
  <h1>虚拟屏 VirtualScreen</h1>
  <span id="ver" class="pill"></span>
  <span id="admin" class="pill"></span>
  <span id="driver" class="pill"></span>
</header>
<main>
  <section class="card">
    <div id="displays"></div>
    <div class="row" style="margin-top:12px">
      <button id="btnApply">应用</button>
      <button id="btnClear" class="danger">清除虚拟屏</button>
      <button id="btnInstall" class="secondary">安装驱动</button>
      <button id="btnSave" class="secondary">保存配置</button>
      <button id="btnRefresh" class="secondary">刷新</button>
      <button id="btnQuit" class="secondary">退出程序</button>
    </div>
    <p id="status"></p>
  </section>
  <section class="card">
    <div class="previews" id="previews"></div>
  </section>
  <section class="card">
    <div id="monitors"></div>
  </section>
</main>
<script>
let state = null;
const $ = (id) => document.getElementById(id);

async function api(path, opts) {
  const r = await fetch(path, opts);
  const t = await r.text();
  let j; try { j = JSON.parse(t); } catch { throw new Error(t || r.statusText); }
  if (!r.ok) throw new Error(j.error || t);
  return j;
}

function renderDisplays() {
  const box = $("displays");
  box.innerHTML = "";
  (state.config.displays || []).forEach((d, i) => {
    const div = document.createElement("div");
    div.className = "row";
    div.innerHTML = `
      <strong>#${i+1}</strong>
      <label>label</label><input class="wide" data-i="${i}" data-k="label" value="${d.label||""}"/>
      <label>width</label><input data-i="${i}" data-k="width" value="${d.width}"/>
      <label>height</label><input data-i="${i}" data-k="height" value="${d.height}"/>
      <label>scale</label><input data-i="${i}" data-k="scale" value="${d.scale}"/>
      <label>hz</label><input data-i="${i}" data-k="hz" value="${d.hz}"/>`;
    box.appendChild(div);
  });
}

function readDisplays() {
  const out = (state.config.displays || []).map((d) => ({...d}));
  document.querySelectorAll("#displays input").forEach((el) => {
    const i = +el.dataset.i, k = el.dataset.k;
    out[i][k] = (k === "label") ? el.value : parseInt(el.value, 10);
  });
  return out;
}

function renderMonitors() {
  const lines = ["当前监视器："];
  (state.monitors || []).forEach((m) => {
    const tag = [];
    if (m.is_primary) tag.push("主屏");
    if (m.likely_virtual) tag.push("疑似虚拟");
    if (!m.is_primary && !m.likely_virtual) tag.push("副屏");
    lines.push(`- ${m.device_name}  ${m.width}x${m.height}+${m.left},${m.top}  ${m.adapter_name||m.monitor_name}  [${tag.join(",")||"普通"}]`);
  });
  $("monitors").textContent = lines.join("\n");
}

function renderPreviews() {
  const box = $("previews");
  box.innerHTML = "";
  const n = (state.preview_count || 0);
  const ts = Date.now();
  for (let i = 0; i < n; i++) {
    const fig = document.createElement("figure");
    fig.innerHTML = `<img src="/api/preview/${i}.bmp?t=${ts}" alt="preview ${i}"/><figcaption>预览 #${i+1}</figcaption>`;
    box.appendChild(fig);
  }
  if (!n) box.textContent = "没有可预览的监视器";
}

async function refresh() {
  state = await api("/api/state");
  $("ver").textContent = "v" + state.version;
  $("admin").textContent = state.admin ? "管理员" : "普通权限（操作会弹 UAC）";
  $("admin").className = "pill " + (state.admin ? "ok" : "");
  $("driver").textContent = state.driver ? "驱动已就绪" : "未检测到驱动";
  $("driver").className = "pill " + (state.driver ? "ok" : "bad");
  $("status").textContent = state.message || "";
  renderDisplays();
  renderMonitors();
  renderPreviews();
}

async function withBusy(btn, fn) {
  const buttons = [...document.querySelectorAll("button")];
  buttons.forEach(b => b.disabled = true);
  try { await fn(); }
  catch (e) { $("status").textContent = "失败: " + e.message; alert(e.message); }
  finally { buttons.forEach(b => b.disabled = false); }
}

$("btnRefresh").onclick = () => withBusy($("btnRefresh"), refresh);
$("btnSave").onclick = () => withBusy($("btnSave"), async () => {
  const displays = readDisplays();
  const j = await api("/api/config", { method:"POST", headers:{"Content-Type":"application/json"}, body: JSON.stringify({displays}) });
  $("status").textContent = j.message || "已保存";
  await refresh();
});
$("btnApply").onclick = () => withBusy($("btnApply"), async () => {
  const displays = readDisplays();
  await api("/api/config", { method:"POST", headers:{"Content-Type":"application/json"}, body: JSON.stringify({displays}) });
  const j = await api("/api/apply", { method:"POST" });
  if (j.elevating) { $("status").textContent = "已请求管理员权限，请在 UAC 确认…"; return; }
  $("status").textContent = j.message || "完成";
  alert(j.message || "完成");
  await refresh();
});
$("btnClear").onclick = () => withBusy($("btnClear"), async () => {
  if (!confirm("禁用虚拟显示驱动设备？")) return;
  const j = await api("/api/clear", { method:"POST" });
  if (j.elevating) { $("status").textContent = "已请求管理员权限，请在 UAC 确认…"; return; }
  $("status").textContent = j.message || "完成";
  alert(j.message || "完成");
  await refresh();
});
$("btnInstall").onclick = () => withBusy($("btnInstall"), async () => {
  if (!confirm("下载并安装 Virtual Display Driver？需要管理员权限。")) return;
  const j = await api("/api/install-driver", { method:"POST" });
  if (j.elevating) { $("status").textContent = "已请求管理员权限，请在 UAC 确认…"; return; }
  $("status").textContent = j.message || "完成";
  alert(j.message || "完成");
  await refresh();
});
$("btnQuit").onclick = () => api("/api/quit", { method:"POST" }).then(() => { document.body.innerHTML = "<p style='padding:24px'>程序已退出，可关闭此页。</p>"; });

refresh();
setInterval(() => { if (state) renderPreviews(); }, 2000);
</script>
</body>
</html>
"""


class AppState:
    def __init__(self) -> None:
        self.cfg = cfgmod.load_config()
        self.message = ""
        self.server: ThreadingHTTPServer | None = None


STATE = AppState()


def _json_response(handler: BaseHTTPRequestHandler, code: int, obj: Any) -> None:
    data = json.dumps(obj, ensure_ascii=False).encode("utf-8")
    handler.send_response(code)
    handler.send_header("Content-Type", "application/json; charset=utf-8")
    handler.send_header("Content-Length", str(len(data)))
    handler.send_header("Cache-Control", "no-store")
    handler.end_headers()
    handler.wfile.write(data)


def _read_json(handler: BaseHTTPRequestHandler) -> dict[str, Any]:
    n = int(handler.headers.get("Content-Length") or 0)
    raw = handler.rfile.read(n) if n else b"{}"
    return json.loads(raw.decode("utf-8") or "{}")


def _elevate(action: str) -> dict[str, Any]:
    if elevate.is_admin():
        return {"elevating": False}
    if not elevate.relaunch_as_admin([f"--{action}"]):
        raise RuntimeError("无法弹出 UAC，或用户已取消")
    # 提权成功后结束当前普通权限进程
    threading.Thread(target=lambda: (time.sleep(0.3), _shutdown()), daemon=True).start()
    return {"elevating": True}


def _shutdown() -> None:
    if STATE.server:
        threading.Thread(target=STATE.server.shutdown, daemon=True).start()


class Handler(BaseHTTPRequestHandler):
    def log_message(self, fmt: str, *args) -> None:  # noqa: A003
        return

    def do_GET(self) -> None:  # noqa: N802
        path = urlparse(self.path).path
        try:
            if path in ("/", "/index.html"):
                data = INDEX_HTML.encode("utf-8")
                self.send_response(200)
                self.send_header("Content-Type", "text/html; charset=utf-8")
                self.send_header("Content-Length", str(len(data)))
                self.end_headers()
                self.wfile.write(data)
                return
            if path == "/api/state":
                mons = [m.__dict__ for m in win_display.list_monitors()]
                targets = win_display.preview_targets(prefer_virtual=True)
                _json_response(
                    self,
                    200,
                    {
                        "version": __version__,
                        "admin": elevate.is_admin(),
                        "driver": vdd.vdd_installed(),
                        "config": STATE.cfg,
                        "monitors": mons,
                        "preview_count": len(targets),
                        "message": STATE.message or (
                            "驱动已就绪。改完分辨率/缩放后点「应用」。"
                            if vdd.vdd_installed()
                            else "未检测到驱动：先点「安装驱动」。"
                        ),
                    },
                )
                return
            if path.startswith("/api/preview/") and path.endswith(".bmp"):
                idx_s = path[len("/api/preview/") : -4]
                idx = int(idx_s)
                targets = win_display.preview_targets(prefer_virtual=True)
                if idx < 0 or idx >= len(targets):
                    self.send_error(404)
                    return
                mon = targets[idx]
                max_h = max(80, int(STATE.cfg.get("preview_max_height", 320)))
                scale = min(1.0, max_h / max(1, mon.height))
                out_w = max(1, int(mon.width * scale))
                out_h = max(1, int(mon.height * scale))
                rgb = win_display.capture_monitor_rgb(mon, out_w, out_h)
                data = rgb_to_bmp(rgb, out_w, out_h)
                self.send_response(200)
                self.send_header("Content-Type", "image/bmp")
                self.send_header("Content-Length", str(len(data)))
                self.send_header("Cache-Control", "no-store")
                self.end_headers()
                self.wfile.write(data)
                return
            self.send_error(404)
        except Exception as e:
            _json_response(self, 500, {"error": str(e)})

    def do_POST(self) -> None:  # noqa: N802
        path = urlparse(self.path).path
        try:
            if path == "/api/config":
                body = _read_json(self)
                cfg = dict(STATE.cfg)
                cfg["displays"] = body.get("displays") or cfg.get("displays")
                errs = cfgmod.validate_config(cfg)
                if errs:
                    _json_response(self, 400, {"error": "\n".join(errs)})
                    return
                cfgmod.save_config(cfg)
                STATE.cfg = cfg
                _json_response(self, 200, {"message": "配置已保存到 exe 同目录 config.json"})
                return
            if path == "/api/apply":
                elev = _elevate("apply")
                if elev.get("elevating"):
                    _json_response(self, 200, elev)
                    return
                msg = vdd.apply_config(STATE.cfg)
                STATE.message = msg
                _json_response(self, 200, {"message": msg})
                return
            if path == "/api/clear":
                elev = _elevate("clear")
                if elev.get("elevating"):
                    _json_response(self, 200, elev)
                    return
                msg = vdd.clear_virtual_displays()
                STATE.message = msg
                _json_response(self, 200, {"message": msg})
                return
            if path == "/api/install-driver":
                elev = _elevate("install-driver")
                if elev.get("elevating"):
                    _json_response(self, 200, elev)
                    return
                msg = vdd.install_driver()
                STATE.message = msg
                _json_response(self, 200, {"message": msg})
                return
            if path == "/api/quit":
                _json_response(self, 200, {"message": "bye"})
                _shutdown()
                return
            self.send_error(404)
        except Exception as e:
            _json_response(self, 500, {"error": str(e)})


def _free_port() -> int:
    for port in range(PORT_BASE, PORT_BASE + 30):
        with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as s:
            try:
                s.bind((HOST, port))
                return port
            except OSError:
                continue
    raise RuntimeError("找不到可用本地端口")


def _run_startup_action(action: str | None) -> None:
    if not action:
        return
    try:
        if action == "install-driver":
            STATE.message = vdd.install_driver()
        elif action == "apply":
            STATE.cfg = cfgmod.load_config()
            STATE.message = vdd.apply_config(STATE.cfg)
        elif action == "clear":
            STATE.message = vdd.clear_virtual_displays()
    except Exception as e:
        STATE.message = f"{action} 失败: {e}"


def main() -> None:
    action = None
    for a in ("install-driver", "apply", "clear"):
        if f"--{a}" in sys.argv:
            action = a
            break
    _run_startup_action(action)

    port = _free_port()
    STATE.server = ThreadingHTTPServer((HOST, port), Handler)
    url = f"http://{HOST}:{port}/"
    # 冒烟标记
    try:
        if getattr(sys, "frozen", False):
            (Path(sys.executable).resolve().parent / "VirtualScreen-gui-ok.txt").write_text(
                url + "\n", encoding="utf-8"
            )
    except Exception:
        pass
    threading.Thread(target=lambda: (time.sleep(0.4), webbrowser.open(url)), daemon=True).start()
    print(f"VirtualScreen {__version__}  {url}")
    try:
        STATE.server.serve_forever()
    finally:
        STATE.server.server_close()
