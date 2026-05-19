#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
dde-file-manager 搜索功能全面压测工具

压测模式:
  1. GUI 自动化搜索 - 通过 xdotool 模拟用户输入搜索关键词
  2. URL 方式搜索    - 通过 search:// URL scheme 直接触发搜索
  3. 多进程并发启动  - 启动多个 dde-file-manager 实例并发搜索
  4. 循环搜索压测    - 单实例快速切换不同关键词搜索
  5. 混合模式        - 以上方式组合, 最大化触发 QSettings 线程竞争
  6. 多窗口搜索      - 启动多个窗口同时搜索, 模拟多窗口并发操作

使用方法:
  python3 search_stress_test.py --mode gui --loops 50
  python3 search_stress_test.py --mode url --loops 100
  python3 search_stress_test.py --mode multi --instances 4
  python3 search_stress_test.py --mode loop --loops 200
  python3 search_stress_test.py --mode mixed --loops 50
  python3 search_stress_test.py --mode windows --windows 3 --loops 30

环境变量:
  DFM_BIN           dde-file-manager 二进制路径 (默认: dde-file-manager)
"""

import subprocess
import time
import os
import sys
import signal
import argparse
import re
import shutil

# ============================================================
# 搜索关键词库 - 包含 .desktop 文件名等会触发 DesktopFile 路径的关键词
# ============================================================
SEARCH_KEYWORDS = [
    # 匹配 .desktop 文件 (会触发 getFileDisplayName -> DesktopFile)
    ".desktop",
    "firefox",
    "chrome",
    "deepin",
    "terminal",
    "file-manager",
    "settings",
    "music",
    "camera",
    "editor",
    "calculator",
    "deepin-music",
    "deepin-movie",
    "deepin-calculator",
    "deepin-terminal",
    "deepin-screenshot",
    "deepin-image-viewer",
    "org.deepin",
    # 通配符 (会触发迭代搜索)
    "*.desktop",
    "deepin*",
    "lib*",
    "*.so",
    # 空搜索 / 单字符 (触发全量搜索)
    "a",
    "d",
    "e",
    # 长关键词 (触发正则搜索)
    "deepin-file-manager",
    "com.deepin",
]

# 搜索目录
SEARCH_DIRS = [
    "/usr/share/applications",
    "/usr/share/applications/kde",
    "/usr/lib",
    os.path.expanduser("~"),
    "/tmp",
]


def find_window_id(pid=None):
    """查找 dde-file-manager 窗口 ID
    使用 wmctrl -lp 通过 PID 精确匹配, 排除桌面窗口
    """
    wids = []

    # 方法 1: 通过 wmctrl -lp 用 PID 精确匹配 (最可靠, 区分多窗口)
    if pid:
        try:
            result = subprocess.run(
                ["wmctrl", "-lp"],
                capture_output=True, text=True, timeout=5
            )
            pid_str = str(pid)
            for line in result.stdout.strip().split('\n'):
                if "文件管理器" in line and "桌面" not in line:
                    parts = line.strip().split()
                    # wmctrl -lp 格式: <hex_wid> <desktop> <pid> <user> <title...>
                    if len(parts) >= 3 and parts[2] == pid_str:
                        wids.append(int(parts[0], 16))
        except Exception:
            pass

    # 方法 2: 通过 xdotool PID 搜索 (兜底)
    if not wids and pid:
        try:
            result = subprocess.run(
                ["xdotool", "search", "--pid", str(pid), "--name", "文件管理器"],
                capture_output=True, text=True, timeout=5
            )
            for w in result.stdout.strip().split('\n'):
                if w.strip():
                    wids.append(int(w))
        except Exception:
            pass

    # 方法 3: xdotool class 匹配 (无 PID 时)
    if not wids and not pid:
        try:
            result = subprocess.run(
                ["xdotool", "search", "--class", "dde-file-manager"],
                capture_output=True, text=True, timeout=5
            )
            for w in result.stdout.strip().split('\n'):
                if w.strip():
                    wid = int(w)
                    hex_wid = hex(wid)
                    wm_result = subprocess.run(
                        ["wmctrl", "-l"],
                        capture_output=True, text=True, timeout=5
                    )
                    for line in wm_result.stdout.strip().split('\n'):
                        if hex_wid in line and "文件管理器" in line and "桌面" not in line:
                            wids.append(wid)
                            break
        except Exception:
            pass

    return wids


def wait_for_window(timeout=30, pid=None):
    """等待 dde-file-manager 窗口出现"""
    start = time.time()
    while time.time() - start < timeout:
        wids = find_window_id(pid=pid)
        if wids:
            return wids[0]
        time.sleep(1)
    # 超时后打印调试信息
    print(f"  [DEBUG] wait_for_window 超时 ({timeout}s), pid={pid}")
    result = subprocess.run(["wmctrl", "-l"], capture_output=True, text=True, timeout=5)
    print(f"  [DEBUG] 当前窗口列表:")
    for line in result.stdout.strip().split('\n'):
        if line.strip():
            print(f"    {line}")
    return None


def wait_for_search_complete(log_path, timeout=120, since_pos=0):
    """监控日志文件等待搜索完成
    检测标志 (searchdiriterator.cpp:105):
      "taskId: ... search completed!"
    以及排序完成的标志 (filesortworker 内部完成后结果写入模型)

    Args:
        log_path: 日志文件路径
        timeout: 最大等待时间(秒)
        since_pos: 从文件此位置(字节)开始检测, 避免匹配到旧记录

    Returns:
        (True, new_pos) 搜索完成, new_pos 为文件新读取位置
        (False, new_pos) 超时未完成
    """
    start = time.time()
    last_size = since_pos
    completed_patterns = [
        "search completed!",
    ]

    while time.time() - start < timeout:
        try:
            if os.path.exists(log_path):
                size = os.path.getsize(log_path)
                if size > last_size:
                    with open(log_path, 'r', errors='replace') as f:
                        f.seek(since_pos)
                        new_content = f.read()
                    last_size = size

                    for pattern in completed_patterns:
                        if pattern in new_content:
                            # 搜索完成, 但排序可能还在进行, 额外等待一小段时间
                            time.sleep(1.0)
                            return True, size
        except Exception:
            pass
        time.sleep(0.5)

    return False, last_size


def activate_window(wid):
    """激活窗口并等待 (同步方式), 超时或失败不抛异常"""
    hex_wid = hex(wid)
    try:
        subprocess.run(["wmctrl", "-i", "-a", hex_wid],
                       capture_output=True, timeout=5)
    except subprocess.TimeoutExpired:
        pass
    except Exception:
        return False
    time.sleep(0.3)
    try:
        subprocess.run(["xdotool", "windowactivate", "--sync", str(wid),
                        "windowfocus", "--sync", str(wid),
                        "windowraise", str(wid)],
                       capture_output=True, timeout=10)
    except subprocess.TimeoutExpired:
        pass
    except Exception:
        return False
    time.sleep(0.3)
    return True


def gui_search(keyword, wid=None):
    """通过 xdotool 模拟 GUI 搜索操作
    dde-file-manager 的搜索机制: 在地址栏输入非路径文本自动触发搜索
    操作流程: Ctrl+L 聚焦地址栏 -> 清空 -> 输入关键词 -> 等待自动搜索
    (参考 TitleBarHelper::handleSearch, 文本非路径时自动调用 sendSearch)
    """
    if wid:
        activate_window(wid)
        time.sleep(0.5)

    for key_cmd in [
        ["xdotool", "key", "ctrl+l"],
        ["xdotool", "key", "ctrl+a", "Delete"],
        ["xdotool", "type", "--clearmodifiers", keyword],
    ]:
        try:
            subprocess.run(key_cmd, capture_output=True, timeout=5)
        except (subprocess.TimeoutExpired, Exception):
            pass
        time.sleep(0.3)

    time.sleep(0.2)


def press_escape(wid=None):
    """按 Escape 关闭搜索/返回上级"""
    if wid:
        activate_window(wid)
        time.sleep(0.2)
    try:
        subprocess.run(["xdotool", "key", "Escape"], capture_output=True, timeout=5)
    except (subprocess.TimeoutExpired, Exception):
        pass
    time.sleep(0.5)


def kill_all_dfm():
    """杀掉所有 dde-file-manager 进程 (排除自身和脚本进程)"""
    my_pid = os.getpid()
    try:
        result = subprocess.run(
            ["pgrep", "-f", "dde-file-manager"],
            capture_output=True, text=True, timeout=5
        )
        for line in result.stdout.strip().split('\n'):
            if not line.strip():
                continue
            pid = int(line.strip())
            if pid != my_pid and pid != os.getppid():
                try:
                    os.kill(pid, signal.SIGKILL)
                except (ProcessLookupError, PermissionError):
                    pass
    except Exception:
        pass
    time.sleep(1.5)


def build_search_url(keyword, search_dir="/"):
    """构建 search:// URL (与 SearchHelper::fromSearchFile 一致)
    格式: search:///路径?url=搜索目录&keyword=关键词&winId=窗口ID
    """
    import urllib.parse
    # search:/// + query params
    # fromSearchFile(targetUrl, keyword, winId) 构造:
    # scheme=search, path=/, query: url=targetUrl&keyword=keyword&winId=winId
    url = "search:///?url={}&keyword={}&winId=1".format(
        urllib.parse.quote(search_dir, safe=''),
        urllib.parse.quote(keyword, safe='')
    )
    return url


# 搜索插件日志 category (fmInfo 使用此 category)
SEARCH_LOG_CATEGORY = "org.deepin.dde.filemanager.plugin.dfmplugin_search.info=true"


def build_env():
    """构建子进程环境变量
    启用搜索插件的 info 日志, 以便 wait_for_search_complete 检测
    """
    env = os.environ.copy()
    logging_rules = env.get("QT_LOGGING_RULES", "")
    if logging_rules:
        logging_rules += ";" + SEARCH_LOG_CATEGORY
    else:
        logging_rules = SEARCH_LOG_CATEGORY
    env["QT_LOGGING_RULES"] = logging_rules
    return env


def check_crash_log(log_content):
    """检查日志中是否有崩溃信息 (非 ASan 模式)
    检测 Qt 崩溃处理器输出、glibc signal、内核信号等信息
    """
    crash_patterns = [
        r"Signal \d+ \(SIG",
        r"Segmentation fault",
        r"QFatal",
        r"ASSERT:",
        r"panic:",
        r"terminating with uncaught exception",
        r"pure virtual method called",
        r"std::terminate",
        r"terminate called",
        r"g_slice",
        r"GLib-CRITICAL",
        r"double free or corruption",
        r"free\(\): invalid (?:pointer|size)",
        r"malloc\(\): corrupted",
        r"\*\*\* glibc detected",
    ]
    for pattern in crash_patterns:
        if re.search(pattern, log_content, re.IGNORECASE):
            return True, pattern
    return False, None


def check_crash(proc, log_content):
    """崩溃检测: 通过进程退出码和日志模式匹配

    Returns:
        (crashed: bool, pattern: str or None)
    """
    # 检查进程退出码 (负数 = 被 signal 杀死)
    if proc and proc.returncode is not None and proc.returncode < 0:
        sig_name = signal.Signals(-proc.returncode).name
        return True, f"process killed by signal {sig_name} (exit code {proc.returncode})"

    # 检查日志中的崩溃模式
    return check_crash_log(log_content)


def cleanup_display():
    """清理 DISPLAY 环境以确保 GUI 操作正常"""
    display = os.environ.get("DISPLAY", ":0")
    return display


# ============================================================
# 压测模式实现
# ============================================================

def mode_gui(args):
    """模式 1: GUI 自动化搜索
    启动 dde-file-manager, 通过 xdotool 模拟用户在搜索栏输入关键词,
    触发完整的搜索流程: 搜索 -> 收集结果 -> 排序显示
    """
    print("=" * 60)
    print("[Mode 1] GUI 自动化搜索压测")
    print(f"  循环次数: {args.loops}")
    print(f"  搜索超时: {args.search_wait}s")
    print("=" * 60)

    cleanup_display()
    dfm_bin = os.environ.get("DFM_BIN",
                              "dde-file-manager")

    kill_all_dfm()
    env = build_env()

    proc = None
    wid = None
    log_path = "/tmp/crashrepro_gui.log"
    log_pos = 0

    try:
        with open(log_path, 'w') as logf:
            proc = subprocess.Popen(
                [dfm_bin],
                env=env, stdout=logf, stderr=subprocess.STDOUT,
                preexec_fn=os.setsid
            )

        wid = wait_for_window(pid=proc.pid)
        if not wid:
            print("[ERROR] 无法找到 dde-file-manager 窗口")
            return 1
        print(f"  窗口 ID: {wid} (pid={proc.pid})")

        for loop in range(args.loops):
            print(f"\n--- GUI Search Loop {loop + 1}/{args.loops} ---")

            wids = find_window_id(pid=proc.pid)
            current_wid = wids[0] if wids else wid

            keyword = SEARCH_KEYWORDS[loop % len(SEARCH_KEYWORDS)]
            print(f"  搜索关键词: {keyword}")

            # 记录当前日志位置, 用于检测本次搜索完成
            log_pos = os.path.getsize(log_path) if os.path.exists(log_path) else 0

            gui_search(keyword, current_wid)

            # 等待搜索完成 (监控日志中的 "search completed!")
            completed, log_pos = wait_for_search_complete(
                log_path, timeout=args.search_wait, since_pos=log_pos)
            if completed:
                print(f"  搜索完成")
            else:
                print(f"  搜索超时 ({args.search_wait}s), 继续下一轮")

            try:
                press_escape(current_wid)
            except Exception:
                print(f"  窗口操作失败 (可能已失效), 尝试重新获取...")
                new_wids = find_window_id(pid=proc.pid)
                if new_wids:
                    wid = new_wids[0]
                else:
                    print(f"  [WARNING] 窗口丢失, 跳过本轮")
                    break
    finally:
        # 收集进程输出
        if proc and proc.poll() is None:
            try:
                proc.send_signal(signal.SIGTERM)
                time.sleep(1)
                proc.send_signal(signal.SIGKILL)
            except Exception:
                pass
            proc.wait(timeout=5)

        if os.path.exists(log_path):
            with open(log_path, 'r') as f:
                output = f.read()
            crashed, pattern = check_crash(proc, output)
            if crashed:
                print(f"\n!!! 崩溃检测到: {pattern} !!!")
                print(f"日志: {log_path}")
                return 1
            os.unlink(log_path)

    print("\n[完成] GUI 搜索压测结束")
    return 0


def mode_url(args):
    """模式 2: URL 方式搜索
    通过 search:// URL 直接触发搜索, 每个 URL 使用独立进程,
    搜索完成后关闭进程
    """
    print("=" * 60)
    print("[Mode 2] URL 方式搜索压测")
    print(f"  循环次数: {args.loops}")
    print(f"  搜索等待: {args.search_wait}s")
    print(f"  并发数: {args.concurrency}")
    print("=" * 60)

    dfm_bin = os.environ.get("DFM_BIN",
                              "dde-file-manager")

    def run_single_search(loop_idx):
        keyword = SEARCH_KEYWORDS[loop_idx % len(SEARCH_KEYWORDS)]
        search_dir = SEARCH_DIRS[loop_idx % len(SEARCH_DIRS)]
        url = build_search_url(keyword, search_dir)
        log_path = f"/tmp/crashrepro_url_{loop_idx}.log"

        env = build_env()

        with open(log_path, 'w') as logf:
            proc = subprocess.Popen(
                [dfm_bin, url],
                env=env, stdout=logf, stderr=subprocess.STDOUT,
                preexec_fn=os.setsid
            )

            # 等待搜索完成 (监控日志)
            completed, _ = wait_for_search_complete(
                log_path, timeout=args.search_wait, since_pos=0)

            # 额外等待排序完成
            if completed:
                time.sleep(args.sort_wait)

            proc.send_signal(signal.SIGTERM)
            time.sleep(1)
            try:
                proc.send_signal(signal.SIGKILL)
            except Exception:
                pass
            proc.wait(timeout=5)

        with open(log_path, 'r') as logf:
            content = logf.read()
        crashed, pattern = check_crash(proc, content)
        if crashed:
            print(f"  !!! Loop {loop_idx}: 崩溃: {pattern}")
            print(f"  日志: {log_path}")
            return False
        else:
            status = "搜索完成" if completed else "搜索超时"
            print(f"  Loop {loop_idx}: OK ({status}, keyword={keyword}, dir={search_dir})")
            os.unlink(log_path)
            return True

    failed = False
    for i in range(args.loops):
        if failed:
            break
        run_single_search(i)

    print("\n[完成] URL 搜索压测结束")
    return 0 if not failed else 1


def mode_multi(args):
    """模式 3: 多进程并发启动
    同时启动多个 dde-file-manager 实例, 每个实例执行不同的搜索,
    最大化 QSettings 全局状态竞争
    """
    print("=" * 60)
    print("[Mode 3] 多进程并发搜索压测")
    print(f"  实例数: {args.instances}")
    print(f"  持续时间: {args.duration}s")
    print(f"  每实例搜索间隔: {args.search_interval}s")
    print("=" * 60)

    dfm_bin = os.environ.get("DFM_BIN",
                              "dde-file-manager")

    procs = []
    logs = []

    env = build_env()

    kill_all_dfm()

    for i in range(args.instances):
        keyword = SEARCH_KEYWORDS[i % len(SEARCH_KEYWORDS)]
        search_dir = SEARCH_DIRS[i % len(SEARCH_DIRS)]
        url = build_search_url(keyword, search_dir)
        log_path = f"/tmp/crashrepro_multi_{i}.log"

        with open(log_path, 'w') as logf:
            proc = subprocess.Popen(
                [dfm_bin, url],
                env=env, stdout=logf, stderr=subprocess.STDOUT,
                preexec_fn=os.setsid
            )
        procs.append(proc)
        logs.append(log_path)
        print(f"  实例 {i}: keyword={keyword}, dir={search_dir}, pid={proc.pid}")

    print(f"\n  {args.instances} 个实例已启动, 等待 {args.duration}s...")

    # 在等待期间, 每 search_interval 秒通过 xdotool 对随机窗口发起新搜索
    start = time.time()
    search_count = 0
    while time.time() - start < args.duration:
        wids = find_window_id()
        if wids:
            target_wid = wids[search_count % len(wids)]
            keyword = SEARCH_KEYWORDS[search_count % len(SEARCH_KEYWORDS)]
            try:
                gui_search(keyword, target_wid)
                print(f"  [{time.time() - start:.1f}s] 额外搜索 #{search_count}: {keyword}")
            except Exception:
                pass
            search_count += 1
        time.sleep(args.search_interval)

    print(f"\n  等待结束, 清理进程...")

    # 杀掉所有进程
    kill_all_dfm()

    # 检查日志
    crashed = False
    for i, log_path in enumerate(logs):
        if os.path.exists(log_path):
            with open(log_path, 'r') as f:
                content = f.read()
            proc_for_check = procs[i] if i < len(procs) else None
            crashed_flag, pattern = check_crash(proc_for_check, content)
            if crashed_flag:
                print(f"  !!! 实例 {i} 崩溃: {pattern}")
                print(f"  日志: {log_path}")
                crashed = True
            else:
                os.unlink(log_path)

    if crashed:
        return 1
    print(f"\n[完成] 多进程并发压测结束 (共 {search_count} 次搜索)")
    return 0


def mode_loop(args):
    """模式 4: 循环搜索压测
    单实例逐个关键词搜索, 每次等待搜索完成后切换下一个关键词
    """
    print("=" * 60)
    print("[Mode 4] 循环搜索压测 (等待搜索完成)")
    print(f"  循环次数: {args.loops}")
    print(f"  搜索超时: {args.search_wait}s")
    print("=" * 60)

    cleanup_display()
    dfm_bin = os.environ.get("DFM_BIN",
                              "dde-file-manager")

    kill_all_dfm()
    env = build_env()

    log_path = "/tmp/crashrepro_loop.log"
    with open(log_path, 'w') as logf:
        proc = subprocess.Popen(
            [dfm_bin],
            env=env, stdout=logf, stderr=subprocess.STDOUT,
            preexec_fn=os.setsid
        )

    wid = wait_for_window(pid=proc.pid)
    if not wid:
        print("[ERROR] 无法找到窗口")
        return 1

    print(f"  窗口 ID: {wid} (pid={proc.pid})")

    log_pos = 0
    for i in range(args.loops):
        keyword = SEARCH_KEYWORDS[i % len(SEARCH_KEYWORDS)]
        print(f"  [{i + 1}/{args.loops}] 搜索: {keyword}")

        wids = find_window_id(pid=proc.pid)
        current_wid = wids[0] if wids else wid

        # 记录日志位置
        log_pos = os.path.getsize(log_path) if os.path.exists(log_path) else 0

        try:
            gui_search(keyword, current_wid)
        except Exception as e:
            print(f"  搜索失败: {e}")
            continue

        # 等待本次搜索完成
        completed, log_pos = wait_for_search_complete(
            log_path, timeout=args.search_wait, since_pos=log_pos)
        if completed:
            print(f"  [{i + 1}/{args.loops}] 搜索完成")
        else:
            print(f"  [{i + 1}/{args.loops}] 搜索超时, 继续")

    # 清理
    try:
        proc.send_signal(signal.SIGTERM)
        time.sleep(1)
        proc.send_signal(signal.SIGKILL)
    except Exception:
        pass

    with open(log_path, 'r') as f:
        content = f.read()
    crashed, pattern = check_crash(proc, content)
    if crashed:
        print(f"\n!!! 崩溃: {pattern} !!!")
        print(f"日志: {log_path}")
        return 1

    os.unlink(log_path)
    print(f"\n[完成] 循环搜索压测结束")
    return 0


def mode_mixed(args):
    """模式 5: 混合模式
    同时使用 URL 搜索 + GUI 搜索 + crashrepro 插件,
    三管齐下最大化触发 QSettings 线程竞争
    """
    print("=" * 60)
    print("[Mode 5] 混合模式压测")
    print(f"  循环次数: {args.loops}")
    print(f"  持续时间: {args.duration}s")
    print("=" * 60)

    dfm_bin = os.environ.get("DFM_BIN",
                              "dde-file-manager")

    # Phase 1: 使用 crashrepro 插件启动 (如果存在)
    kill_all_dfm()
    env = build_env()

    plugin_path = "./build/src/plugins/libdfmplugin-crashrepro.so"
    has_crashrepro = os.path.exists(plugin_path)
    if has_crashrepro:
        print("  [Phase 1] crashrepro 插件已检测到, 将在后台运行")
        env["DFM_CRASHREPRO_DELAY"] = "3"
        env["DFM_CRASHREPRO_THREADS"] = "16"
        env["DFM_CRASHREPRO_DURATION"] = str(args.duration)
        env["DFM_CRASHREPRO_MAINCONTENTION"] = "1"
    else:
        print("  [Phase 1] crashrepro 插件未找到, 跳过")

    # 启动主实例
    log_path = "/tmp/crashrepro_mixed.log"
    with open(log_path, 'w') as logf:
        proc = subprocess.Popen(
            [dfm_bin],
            env=env, stdout=logf, stderr=subprocess.STDOUT,
            preexec_fn=os.setsid
        )

    wid = wait_for_window(pid=proc.pid)
    if not wid:
        print("[ERROR] 无法找到 dde-file-manager 窗口")
        kill_all_dfm()
        return 1
    print(f"  主实例 PID: {proc.pid}, 窗口 ID: {wid}")

    # Phase 2: 同时启动 URL 搜索的辅助实例
    url_procs = []
    for i in range(min(2, args.concurrency)):
        keyword = SEARCH_KEYWORDS[(i + 5) % len(SEARCH_KEYWORDS)]
        url = build_search_url(keyword, "/usr/share/applications")
        url_log = f"/tmp/crashrepro_mixed_url_{i}.log"
        with open(url_log, 'w') as f:
            p = subprocess.Popen(
                [dfm_bin, url],
                env=env, stdout=f, stderr=subprocess.STDOUT,
                preexec_fn=os.setsid
            )
        url_procs.append((p, url_log))
        print(f"  [Phase 2] URL 实例 {i}: keyword={keyword}, pid={p.pid}")

    # Phase 3: 对主实例进行 GUI 搜索 (等待每次搜索完成)
    print(f"  [Phase 3] 开始 GUI 搜索循环 (等待每次完成)...")
    start = time.time()
    log_pos = os.path.getsize(log_path) if os.path.exists(log_path) else 0
    for i in range(args.loops):
        if time.time() - start > args.duration:
            break

        keyword = SEARCH_KEYWORDS[i % len(SEARCH_KEYWORDS)]

        wids = find_window_id(pid=proc.pid)
        current_wid = wids[0] if wids else wid

        # 记录日志位置
        log_pos = os.path.getsize(log_path) if os.path.exists(log_path) else 0

        try:
            gui_search(keyword, current_wid)
        except Exception:
            continue

        # 等待搜索完成
        completed, log_pos = wait_for_search_complete(
            log_path, timeout=args.search_wait, since_pos=log_pos)
        if not completed:
            # 日志未检测到搜索完成, 额外等待一段时间确保排序结束
            extra_wait = min(args.sort_wait, args.duration - (time.time() - start))
            if extra_wait > 0:
                time.sleep(extra_wait)
        elapsed = time.time() - start
        status = "完成" if completed else "超时(已等待)"
        print(f"  [{elapsed:.1f}s] GUI 搜索 #{i + 1}: {keyword} ({status})")

    # 清理
    print("  清理进程...")
    kill_all_dfm()

    # 检查所有日志
    crashed = False
    all_logs = [(log_path, proc)] + [(lp, p) for p, lp in url_procs]
    for log, proc_for_check in all_logs:
        if os.path.exists(log):
            with open(log, 'r') as f:
                content = f.read()
            crashed_flag, pattern = check_crash(proc_for_check, content)
            if crashed_flag:
                print(f"  !!! 崩溃: {pattern}")
                print(f"  日志: {log}")
                crashed = True
            else:
                try:
                    os.unlink(log)
                except Exception:
                    pass

    return 1 if crashed else 0


def mode_inprocess(args):
    """模式 6: 进程内压测 (使用 crashrepro 插件)
    循环启动 dde-file-manager (加载 crashrepro 插件), 检测完成后关闭
    """
    print("=" * 60)
    print("[Mode 6] 进程内压测 (crashrepro 插件循环)")
    print(f"  循环次数: {args.loops}")
    print(f"  插件延迟: {args.plugin_delay}s")
    print(f"  插件线程: {args.plugin_threads}")
    print(f"  插件持续时间: {args.plugin_duration}s")
    print("=" * 60)

    dfm_bin = os.environ.get("DFM_BIN",
                              "dde-file-manager")

    plugin_path = "./build/src/plugins/libdfmplugin-crashrepro.so"
    if not os.path.exists(plugin_path):
        print(f"[ERROR] crashrepro 插件不存在: {plugin_path}")
        print("  请先编译: cmake --build build --target dfmplugin-crashrepro")
        return 1

    env = build_env()
    env["DFM_CRASHREPRO_DELAY"] = str(args.plugin_delay)
    env["DFM_CRASHREPRO_THREADS"] = str(args.plugin_threads)
    env["DFM_CRASHREPRO_DURATION"] = str(args.plugin_duration)

    for i in range(args.loops):
        print(f"\n--- In-process Loop {i + 1}/{args.loops} ---")

        # 清理残留
        kill_all_dfm()
        time.sleep(1)

        log_path = f"/tmp/crashrepro_inprocess_{i}.log"
        with open(log_path, 'w') as logf:
            proc = subprocess.Popen(
                [dfm_bin],
                env=env, stdout=logf, stderr=subprocess.STDOUT,
                preexec_fn=os.setsid
            )

        # 等待插件执行完成
        # 插件总时间 = delay + duration + 启动时间
        total_wait = args.plugin_delay + args.plugin_duration + 10
        start = time.time()
        finished = False

        while time.time() - start < total_wait:
            if proc.poll() is not None:
                # 进程已退出 (可能是崩溃)
                if proc.returncode is not None and proc.returncode < 0:
                    sig_name = signal.Signals(-proc.returncode).name
                    print(f"  !!! 进程被信号杀死: {sig_name}")
                    break
                break
            # 检查日志
            try:
                with open(log_path, 'r') as f:
                    content = f.read()
                if "=== finished === elapsed" in content:
                    finished = True
                    break
                if check_crash(proc, content)[0]:
                    print(f"  !!! 崩溃检测到!")
                    break
            except Exception:
                pass
            time.sleep(2)

        # 关闭进程
        try:
            os.killpg(os.getpgid(proc.pid), signal.SIGTERM)
        except Exception:
            pass
        time.sleep(1)
        try:
            os.killpg(os.getpgid(proc.pid), signal.SIGKILL)
        except Exception:
            pass
        proc.wait(timeout=10)

        # 检查结果
        try:
            with open(log_path, 'r') as f:
                content = f.read()
            crashed, pattern = check_crash(proc, content)
            if crashed:
                print(f"  !!! Loop {i + 1}: 崩溃: {pattern}")
                print(f"  日志: {log_path}")
                return 1
            elif finished:
                print(f"  Loop {i + 1}: 完成 (无崩溃)")
                os.unlink(log_path)
            else:
                print(f"  Loop {i + 1}: 超时, 继续下一轮")
                os.unlink(log_path)
        except Exception as e:
            print(f"  Loop {i + 1}: 读取日志失败: {e}")

    print(f"\n[完成] 进程内压测结束 ({args.loops} 次)")
    return 0


def count_filemanager_windows():
    """统计当前文件管理器窗口数量 (排除桌面)"""
    try:
        result = subprocess.run(
            ["wmctrl", "-l"],
            capture_output=True, text=True, timeout=5
        )
        count = 0
        for line in result.stdout.strip().split('\n'):
            if "文件管理器" in line and "桌面" not in line:
                count += 1
        return count
    except Exception:
        return 0


def wait_for_new_window(current_count, timeout=15):
    """等待新窗口出现 (窗口数 > current_count)
    Returns:
        新窗口的 wid, 或 None
    """
    start = time.time()
    while time.time() - start < timeout:
        wids = find_window_id()  # class-based match
        if len(wids) > current_count:
            # 返回最新的窗口
            return wids[-1]
        time.sleep(0.5)
    return None


def mode_windows(args):
    """模式 7: 多窗口并发搜索
    利用 dde-file-manager 的单实例 + -n (new-window) 机制:
      1. 启动 1 个 dde-file-manager 进程
      2. 通过 'dde-file-manager -n' 命令打开额外窗口 (参数通过 QLocalSocket 转发给主实例)
      3. 对每个窗口同时执行不同关键词搜索, 模拟多窗口并发操作
    """
    print("=" * 60)
    print("[Mode 7] 多窗口并发搜索压测")
    print(f"  窗口数量: {args.windows}")
    print(f"  循环次数: {args.loops}")
    print(f"  持续时间: {args.duration}s")
    print(f"  搜索超时: {args.search_wait}s")
    print("=" * 60)

    cleanup_display()
    dfm_bin = os.environ.get("DFM_BIN",
                              "dde-file-manager")

    kill_all_dfm()
    env = build_env()

    # === Step 1: 启动唯一的 dde-file-manager 进程 ===
    log_path = "/tmp/crashrepro_win_main.log"
    with open(log_path, 'w') as logf:
        proc = subprocess.Popen(
            [dfm_bin],
            env=env, stdout=logf, stderr=subprocess.STDOUT,
            preexec_fn=os.setsid
        )

    print(f"  主进程: pid={proc.pid}, 启动中...")
    wid0 = wait_for_window(pid=proc.pid, timeout=30)
    if not wid0:
        print("[ERROR] 无法找到主窗口")
        kill_all_dfm()
        return 1
    print(f"  窗口 0: wid={wid0} (pid={proc.pid})")

    # === Step 2: 通过 -n 参数打开额外窗口 ===
    wids = [wid0]
    for i in range(1, args.windows):
        current_count = count_filemanager_windows()
        # 启动 dde-file-manager -n: 单实例机制会将 -n 转发给主进程, 该进程立即退出
        subprocess.run(
            [dfm_bin, "-n"],
            env=env, capture_output=True, timeout=10,
            preexec_fn=lambda: os.setsid()
        )
        new_wid = wait_for_new_window(current_count, timeout=15)
        if new_wid:
            wids.append(new_wid)
            print(f"  窗口 {i}: wid={new_wid}")
        else:
            print(f"  [WARNING] 窗口 {i} 未能在超时内打开")

    actual_windows = len(wids)
    if actual_windows < 1:
        print("[ERROR] 没有可用窗口")
        kill_all_dfm()
        return 1

    print(f"\n  {actual_windows} 个窗口就绪, 开始并发搜索")

    crashed = False
    crash_detail = ""

    # 选择驱动方式: 按 loops 还是 duration
    use_duration = (args.duration > 0)
    start = time.time()
    loop = 0

    while not crashed:
        if use_duration and (time.time() - start) >= args.duration:
            break
        if not use_duration and loop >= args.loops:
            break

        print(f"\n--- Round {loop + 1} ---")
        round_start = time.time()

        # 记录日志位置
        log_pos = os.path.getsize(log_path) if os.path.exists(log_path) else 0

        # 为每个窗口分配不同关键词 (错开偏移)
        for wi in range(actual_windows):
            keyword_idx = (loop + wi * 3) % len(SEARCH_KEYWORDS)
            keyword = SEARCH_KEYWORDS[keyword_idx]

            print(f"  窗口 {wi}: 搜索 '{keyword}'", end="")
            try:
                gui_search(keyword, wids[wi])
                print()
            except Exception as e:
                print(f" (失败: {e})")
                continue

        # 等待搜索完成
        round_deadline = args.search_wait - (time.time() - round_start)
        if round_deadline < 1:
            round_deadline = 1

        completed, _ = wait_for_search_complete(
            log_path, timeout=round_deadline, since_pos=log_pos)
        status = "完成" if completed else "超时"
        print(f"  搜索: {status}")

        # 检查主进程是否崩溃 (只有 exit code=0 且进程提前退出才需要关注)
        if proc.poll() is not None:
            if proc.returncode != 0:
                print(f"\n  !!! 主进程异常退出 (exit code={proc.returncode}) !!!")
                crashed = True
            else:
                # exit code=0: 进程正常退出 (例如被其他实例的 -n 请求影响), 但不应该发生
                print(f"\n  [WARNING] 主进程已退出 (exit code=0), 可能影响后续测试")

        loop += 1

        # 轮次间短暂间隔
        if not crashed:
            time.sleep(1.0)

    # 统计结果
    elapsed = time.time() - start
    print(f"\n  总耗时: {elapsed:.1f}s, 完成 {loop} 轮搜索")

    # 清理
    print("  清理进程...")
    if proc.poll() is None:
        try:
            proc.send_signal(signal.SIGTERM)
        except Exception:
            pass
    time.sleep(2)
    if proc.poll() is None:
        try:
            proc.send_signal(signal.SIGKILL)
        except Exception:
            pass
    try:
        proc.wait(timeout=5)
    except Exception:
        pass

    # 检查日志
    if os.path.exists(log_path):
        with open(log_path, 'r') as f:
            content = f.read()
        found, pattern = check_crash(proc, content)
        if found:
            crashed = True
            crash_detail = f"崩溃: {pattern}"
            print(f"  !!! {crash_detail}")
            print(f"  日志: {log_path}")
        else:
            os.unlink(log_path)

    if crashed:
        print(f"\n[失败] 多窗口压测检测到异常: {crash_detail}")
        return 1

    print(f"\n[完成] 多窗口搜索压测结束 ({loop} 轮, {actual_windows} 窗口)")
    return 0


def main():
    parser = argparse.ArgumentParser(
        description="dde-file-manager 搜索功能全面压测工具",
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog="""
压测模式:
  gui       通过 xdotool 模拟用户在搜索栏输入关键词
  url       通过 search:// URL 方式触发搜索 (每轮独立进程)
  multi     同时启动多个 dde-file-manager 实例并发搜索
  loop      单实例快速切换不同关键词 (不等搜索完成)
  mixed     GUI + URL + crashrepro 插件三管齐下
  inprocess 循环启动 crashrepro 插件 (进程内模拟 getFileDisplayName)
  windows  启动多个独立窗口, 每个窗口同时执行搜索

示例:
  # GUI 搜索 50 轮
  python3 search_stress_test.py --mode gui --loops 50

  # URL 搜索 100 轮
  python3 search_stress_test.py --mode url --loops 100

  # 4 个实例并发搜索 60 秒
  python3 search_stress_test.py --mode multi --instances 4 --duration 60

  # 快速循环搜索 200 轮
  python3 search_stress_test.py --mode loop --loops 200 --search-interval 1

  # 混合模式
  python3 search_stress_test.py --mode mixed --loops 50 --duration 120

  # 进程内插件循环
  python3 search_stress_test.py --mode inprocess --loops 100 --plugin-duration 30

  # 多窗口搜索 30 轮
  python3 search_stress_test.py --mode windows --windows 3 --loops 30

  # 多窗口搜索 60 秒
  python3 search_stress_test.py --mode windows --windows 5 --duration 60
        """
    )

    parser.add_argument("--mode", "-m", choices=["gui", "url", "multi", "loop", "mixed", "inprocess", "windows"],
                        default="inprocess", help="压测模式 (默认: inprocess)")
    parser.add_argument("--loops", "-n", type=int, default=50, help="循环次数 (默认: 50)")
    parser.add_argument("--duration", "-d", type=int, default=60, help="持续时间(秒) (默认: 60)")
    parser.add_argument("--instances", "-i", type=int, default=4, help="多实例数量 (默认: 4)")
    parser.add_argument("--concurrency", "-c", type=int, default=2, help="并发数 (默认: 2)")
    parser.add_argument("--search-wait", type=float, default=30.0, help="搜索结果等待时间(秒) (默认: 30.0)")
    parser.add_argument("--sort-wait", type=float, default=2.0, help="排序完成等待时间(秒) (默认: 2.0)")
    parser.add_argument("--search-interval", type=float, default=2.0, help="搜索间隔(秒) (默认: 2.0)")
    parser.add_argument("--plugin-delay", type=int, default=3, help="crashrepro 插件延迟(秒) (默认: 3)")
    parser.add_argument("--plugin-threads", type=int, default=16, help="crashrepro 插件线程数 (默认: 16)")
    parser.add_argument("--plugin-duration", type=int, default=30, help="crashrepro 插件持续时间(秒) (默认: 30)")
    parser.add_argument("--windows", "-w", type=int, default=3, help="多窗口搜索的窗口数量 (默认: 3)")
    parser.add_argument("--dfm-bin", type=str, default=None,
                        help="dde-file-manager 二进制路径 (也可通过 DFM_BIN 环境变量设置)")

    args = parser.parse_args()

    if args.dfm_bin:
        os.environ["DFM_BIN"] = args.dfm_bin

    # 检查二进制文件
    dfm_bin = os.environ.get("DFM_BIN",
                              "dde-file-manager")
    if not os.path.exists(dfm_bin):
        print(f"[ERROR] dde-file-manager 二进制文件不存在: {dfm_bin}")
        print("  请通过 --dfm-bin 指定路径或设置 DFM_BIN 环境变量")
        sys.exit(1)

    # 检查 xdotool (GUI 模式需要)
    if args.mode in ("gui", "loop", "mixed", "windows"):
        if not shutil.which("xdotool"):
            print("[ERROR] xdotool 未安装, GUI 模式需要 xdotool")
            print("  安装: sudo apt install xdotool")
            sys.exit(1)

    print(f"dde-file-manager: {dfm_bin}")

    mode_map = {
        "gui": mode_gui,
        "url": mode_url,
        "multi": mode_multi,
        "loop": mode_loop,
        "mixed": mode_mixed,
        "inprocess": mode_inprocess,
        "windows": mode_windows,
    }

    ret = mode_map[args.mode](args)

    # 最终清理
    kill_all_dfm()

    if ret == 0:
        print("\n所有压测完成, 未检测到崩溃")
    else:
        print("\n压测检测到崩溃, 请查看日志")

    sys.exit(ret)


if __name__ == "__main__":
    main()
