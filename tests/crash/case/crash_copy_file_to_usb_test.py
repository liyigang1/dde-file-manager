#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
================================================================================
DDE文件管理器 - USB复制粘贴与崩溃检测自动化测试脚本（最近窗口+焦点窗口版）
================================================================================

【场景描述】
    检测U盘、创建嵌套测试数据（100KB文件）、通过dde-file-manager窗口自动化实现复制粘贴操作，
    按最近打开窗口查找、延迟2秒、复制后延迟5秒、按最近窗口粘贴、按键执行后延迟1秒、找焦点窗口并列举属性、关闭窗口，
    删除测试目录、关闭脚本打开的所有窗口、检查coredump文件。

【测试内容】
    1. 检查是否有U盘，没有U盘则记录报错并退出
    2. 在~/Download目录下创建_crash_copy_paste目录，再在_crash_copy_paste下创建_crash_copy_paste目录
    3. 在最新创建的嵌套目录下创建1000个50KB以下的随机文件
    4. 将~/Download/_crash_copy_paste转换为绝对路径的file:// URL并使用dde-file-manager -n打开
    5. 延迟2秒，找到最近打开的窗口，选中_crash_copy_paste，使用快捷键ctrl+c复制
    6. 找到U盘所有挂载点的绝对路径后转换为url并使用dde-file-manager -n打开
    7. 延迟2秒后找到最近打开的窗口，使用快捷键ctrl+v粘贴，快捷键执行后等1秒后找焦点窗口，列举出窗口的全部属性并关闭这个窗口
    8. 删除~/Download/_crash_copy_paste目录和U盘挂点下的_crash_copy_paste目录
    9. 关闭当前脚本打开的所有窗口
    10. 检查/var/lib/systemd/coredump目录下是否有文件名称包含"dde-file-m"的文件，
        文件创建时间在脚本运行期间，有就调用../tool/export_coredumps.sh脚本分析堆栈，并退出

【使用方法】
    python3 test_recent_and_focus.py

【输出文件】
    日志文件：~/压测日志/recent_focus_test_YYYYMMDD_HHMMSS.log

【依赖项】
    系统工具：xdotool, wmctrl
    Python库：标准库

【注意事项】
    1. 需要图形界面环境
    2. 需要U盘已挂载
    3. 需要xdotool、wmctrl工具
    4. 脚本执行期间不要操作鼠标键盘
    5. 脚本会删除测试目录并关闭所有它打开的窗口，请确保数据安全
    6. 建议在测试机上运行
    7. 需要systemd-coredump服务运行以捕获崩溃
================================================================================
"""

import os
import subprocess
import time
import random
import datetime
import logging
import shutil
from urllib.parse import quote
from pathlib import Path
import sys

# ==================== 配置区 ====================
CONFIG = {
    "dde_file_manager": "/usr/bin/dde-file-manager",
    "test_dir_name": "_crash_copy_paste",
    "num_files": 1000,
    "max_file_size": 102400 - 1,  # 小于50KB (50KB - 1 byte)
    "min_file_size": 1024,        # 最小1KB
    "download_dir": os.path.expanduser("~/Downloads"),  # 注意：是Download不是Downloads
    "log_dir": os.path.expanduser("~/压测日志"),
    "fm_open_delay": 1,           # 等待文件管理器打开
    "window_activate_delay": 1,   # 等待窗口激活
    "key_press_delay": 0.2,       # 键盘按键延迟
    "action_delay": 1,            # 动作间隔
    "find_window_delay": 2,       # 查找窗口延迟（步骤4和6使用2秒）
    "focus_check_delay": 1,       # 焦点窗口检查延迟（步骤6按键执行后等待1秒）
    "coredump_dir": "/var/lib/systemd/coredump",  # coredump目录
    "script_dir": os.path.dirname(os.path.abspath(__file__)),  # 脚本所在目录
}

# 全局变量：跟踪脚本打开的窗口
script_opened_windows = []

# ==================== 日志配置 ====================
def setup_logging():
    """
    设置日志系统
    """
    log_dir = CONFIG["log_dir"]
    os.makedirs(log_dir, exist_ok=True)

    timestamp = datetime.datetime.now().strftime("%Y%m%d_%H%M%S")
    log_file = os.path.join(log_dir, f"recent_focus_test_{timestamp}.log")

    logging.basicConfig(
        level=logging.INFO,
        format='%(asctime)s - %(levelname)s - %(message)s',
        handlers=[
            logging.FileHandler(log_file, encoding='utf-8'),
            logging.StreamHandler(sys.stdout)
        ]
    )

    logging.info(f"日志文件: {log_file}")
    return log_file

# ==================== USB检测 ====================
def find_usb_mount_points():
    """
    查找U盘挂载点
    """
    logging.info("=" * 70)
    logging.info("步骤1: 检查U盘")
    logging.info("=" * 70)

    usb_mounts = []
    detected_drives = set()

    try:
        # 方法1: 使用lsblk查找可移动设备
        result = subprocess.run(
            ['lsblk', '-J', '-o', 'NAME,TYPE,MOUNTPOINT,RM,SIZE,VENDOR,MODEL'],
            capture_output=True,
            text=True
        )

        if result.returncode == 0 and result.stdout:
            import json
            blks = json.loads(result.stdout)

            if 'blockdevices' in blks:
                for device in blks['blockdevices']:
                    if device.get('rm') == '1':  # 可移动设备
                        name = device.get('name', '')
                        mountpoint = device.get('mountpoint')
                        size = device.get('size', '')
                        vendor = device.get('vendor', '')
                        model = device.get('model', '')

                        if mountpoint and os.path.ismount(mountpoint):
                            usb_mounts.append(mountpoint)
                            drive_info = f"{name} ({vendor} {model}, {size}) -> {mountpoint}"
                            logging.info(f"找到U盘: {drive_info}")
                            detected_drives.add(name)

        # 方法2: 检查/proc/mounts
        with open('/proc/mounts', 'r') as f:
            for line in f:
                parts = line.split()
                if len(parts) >= 2:
                    mount_point = parts[1]
                    device = parts[0]

                    # 跳过系统挂载点
                    if any(skip in mount_point for skip in
                           ['/run/media', '/media/', '/mnt', '/home', '/boot',
                            '/snap', '/var/lib/snapd', '/proc', '/sys', '/dev']):
                        # 检查是否是USB设备
                        if any(usb in device for usb in ['/dev/sd', '/dev/mmcblk']):
                            # 检查设备是否可移动
                            try:
                                result = subprocess.run(
                                    ['lsblk', '-n', '-d', '-o', 'RM', device],
                                    capture_output=True,
                                    text=True
                                )
                                if result.stdout.strip() == '1':
                                    if mount_point not in usb_mounts:
                                        usb_mounts.append(mount_point)
                                        logging.info(f"在/proc/mounts中找到U盘: {device} -> {mount_point}")
                            except:
                                pass

    except Exception as e:
        logging.error(f"查找U盘失败: {e}")

    # 去重并返回
    usb_mounts = list(dict.fromkeys(usb_mounts))  # 保持顺序并去重

    if not usb_mounts:
        logging.error("=" * 70)
        logging.error("未找到U盘！请插入U盘并确保已挂载。")
        logging.error("脚本将退出。")
        logging.error("=" * 70)
        return []

    logging.info(f"共找到 {len(usb_mounts)} 个U盘挂载点")
    for mount in usb_mounts:
        logging.info(f"  - {mount}")

    return usb_mounts

# ==================== 文件创建 ====================
def create_test_files_with_nested_dir(base_dir, dir_name, num_files, max_size):
    """
    创建嵌套测试目录和文件

    目录结构：
    base_dir/dir_name/dir_name/ (嵌套)
    """
    logging.info("=" * 70)
    logging.info("步骤2: 创建测试数据")
    logging.info("=" * 70)

    # 创建第一层目录
    first_level_dir = os.path.join(base_dir, dir_name)
    try:
        os.makedirs(first_level_dir, exist_ok=True)
        logging.info(f"✓ 创建第一层目录: {first_level_dir}")
    except Exception as e:
        logging.error(f"✗ 创建第一层目录失败: {e}")
        return None

    # 创建第二层嵌套目录
    nested_dir = os.path.join(first_level_dir, dir_name)
    try:
        os.makedirs(nested_dir, exist_ok=True)
        logging.info(f"✓ 创建第二层嵌套目录: {nested_dir}")
    except Exception as e:
        logging.error(f"✗ 创建第二层嵌套目录失败: {e}")
        return None

    # 在嵌套目录下创建文件
    logging.info(f"创建 {num_files} 个随机文件...")
    created_count = 0

    for i in range(num_files):
        try:
            file_size = random.randint(1024, max_size)
            filename = f"test_file_{i:05d}.dat"
            file_path = os.path.join(nested_dir, filename)

            with open(file_path, 'wb') as f:
                # 生成随机数据
                random_data = os.urandom(file_size)
                f.write(random_data)

            created_count += 1

            if (i + 1) % 100 == 0:
                logging.info(f"已创建 {i + 1}/{num_files} 个文件")

        except Exception as e:
            logging.error(f"创建文件 {filename} 失败: {e}")

    logging.info(f"✓ 成功创建 {created_count}/{num_files} 个文件")
    logging.info(f"✓ 测试目录创建完成: {nested_dir}")

    return nested_dir

# ==================== 窗口管理 ====================
def find_all_windows():
    """
    查找所有窗口
    """
    logging.info("查找所有窗口...")

    windows = []

    try:
        result = subprocess.run(['wmctrl', '-l'], capture_output=True, text=True)

        if result.returncode != 0:
            logging.warning("wmctrl命令执行失败")
            return windows

        lines = result.stdout.strip().split('\n')

        for line in lines:
            parts = line.split(None, 5)
            if len(parts) < 5:
                continue

            window_id = parts[0]
            title = parts[4]

            if window_id.startswith('0x'):
                desktop = parts[1]
                window_class = parts[3] if len(parts) > 3 else ""

                windows.append({
                    "window_id": window_id,
                    "title": title,
                    "class": window_class,
                    "desktop": desktop
                })
                logging.info(f"找到窗口: {window_id} - {title}")

    except subprocess.CalledProcessError as e:
        logging.error(f"执行wmctrl失败: {e}")
    except Exception as e:
        logging.error(f"查找窗口时出错: {e}")

    logging.info(f"共找到 {len(windows)} 个窗口")
    return windows

def get_most_recent_window():
    """
    获取最近打开的窗口（窗口列表中的最后一个）
    """
    windows = find_all_windows()

    if not windows:
        logging.warning("未找到任何窗口")
        return None

    recent_window = windows[-1]
    logging.info(f"✓ 找到最近打开的窗口: {recent_window['window_id']} - {recent_window['title']}")
    return recent_window

def find_focused_window():
    """
    找到当前焦点的窗口
    """
    logging.info("查找焦点窗口...")

    try:
        result = subprocess.run(['xdotool', 'getwindowfocus'], capture_output=True, text=True)

        if result.returncode == 0 and result.stdout:
            window_id = result.stdout.strip()

            # 获取窗口名称
            name_result = subprocess.run(
                ['xdotool', 'getwindowname', window_id],
                capture_output=True,
                text=True
            )

            window_title = name_result.stdout.strip() if name_result.returncode == 0 else "Unknown"

            logging.info(f"✓ 找到焦点窗口: {window_id} - {window_title}")
            return {"window_id": window_id, "title": window_title}

    except Exception as e:
        logging.error(f"查找焦点窗口时出错: {e}")

    return None

def get_window_properties(window_id):
    """
    获取窗口的所有属性并列举
    """
    logging.info("=" * 70)
    logging.info(f"获取窗口 {window_id} 的所有属性")
    logging.info("=" * 70)

    try:
        # 使用 wmctrl -l 获取基础信息
        result = subprocess.run(['wmctrl', '-l'], capture_output=True, text=True)

        if result.returncode == 0 and result.stdout:
            lines = result.stdout.strip().split('\n')
            for line in lines:
                parts = line.split(None, 5)
                if len(parts) >= 5 and parts[0] == window_id:
                    wm_info = {
                        "Window ID": parts[0],
                        "Desktop": parts[1],
                        "PID": parts[2] if len(parts) > 2 else "N/A",
                        "Window Class": parts[3] if len(parts) > 3 else "N/A",
                        "Window Title": parts[4] if len(parts) > 4 else "N/A"
                    }

                    logging.info("--- wmctrl 基础属性 ---")
                    for key, value in wm_info.items():
                        logging.info(f"  {key}: {value}")

        # 使用 xdotool 获取更多属性
        try:
            # 获取窗口大小和位置
            result = subprocess.run(
                ['xdotool', 'getwindowgeometry', window_id],
                capture_output=True,
                text=True
            )
            if result.returncode == 0:
                logging.info("--- xdotool 窗口几何属性 ---")
                for line in result.stdout.strip().split('\n'):
                    logging.info(f"  {line}")

            # 获取窗口名称
            result = subprocess.run(
                ['xdotool', 'getwindowname', window_id],
                capture_output=True,
                text=True
            )
            if result.returncode == 0 and result.stdout:
                logging.info("--- xdotool 窗口名称 ---")
                logging.info(f"  Window Name: {result.stdout.strip()}")

            # 获取窗口类名
            result = subprocess.run(
                ['xdotool', 'getwindowclassname', window_id],
                capture_output=True,
                text=True
            )
            if result.returncode == 0 and result.stdout:
                logging.info("--- xdotool 窗口类名 ---")
                logging.info(f"  Window Class: {result.stdout.strip()}")

            # 获取窗口可见性/焦点状态
            result = subprocess.run(
                ['xdotool', 'getwindowfocus'],
                capture_output=True,
                text=True
            )
            if result.returncode == 0 and result.stdout:
                is_focused = result.stdout.strip() == window_id
                logging.info("--- 窗口焦点状态 ---")
                logging.info(f"  Is Focused: {is_focused}")

        except Exception as e:
            logging.warning(f"获取 xdotool 窗口属性时出错: {e}")

        logging.info("=" * 70)

    except Exception as e:
        logging.error(f"获取窗口属性时出错: {e}")

def activate_window(window_id):
    """
    激活指定窗口
    """
    logging.info(f"激活窗口: {window_id}")

    try:
        subprocess.run(['wmctrl', '-ia', window_id], check=True)
        time.sleep(CONFIG["window_activate_delay"])
        return True
    except subprocess.CalledProcessError as e:
        logging.error(f"激活窗口失败: {e}")
        return False

def send_key_combo(key):
    """
    发送键盘组合键
    """
    logging.info(f"发送按键: {key}")
    time.sleep(CONFIG["key_press_delay"])

    try:
        subprocess.run(['xdotool', 'key', key], check=True)
        time.sleep(CONFIG["action_delay"])
        return True
    except subprocess.CalledProcessError as e:
        logging.error(f"发送按键失败: {e}")
        return False

def select_directory_by_name(dir_name):
    """
    通过键盘选择指定目录（先按名称首字母跳转，再向下选择）
    """
    logging.info(f"尝试选择目录: {dir_name}")

    try:
        time.sleep(CONFIG["key_press_delay"])

        first_char = dir_name[0].lower()

        if first_char.isdigit():
            digit_key = str(first_char)
            subprocess.run(['xdotool', 'key', digit_key], check=True)
            logging.info(f"按数字键: {digit_key}")
        else:
            subprocess.run(['xdotool', 'key', first_char], check=True)
            logging.info(f"按字母键: {first_char}")

        time.sleep(CONFIG["action_delay"])

        send_key_combo('Down')

        return True

    except Exception as e:
        logging.error(f"选择目录时出错: {e}")
        return False

def copy_selected_directory():
    """
    复制选中的目录
    """
    logging.info("执行复制操作 (Ctrl+C)")

    return send_key_combo('ctrl+c')

def paste_directory():
    """
    粘贴目录
    """
    logging.info("执行粘贴操作 (Ctrl+V)")

    return send_key_combo('ctrl+v')

def close_window(window_id):
    """
    关闭指定的窗口
    """
    logging.info(f"关闭窗口: {window_id}")

    try:
        subprocess.run(['wmctrl', '-ic', window_id], check=True)
        logging.info(f"✓ 已关闭窗口: {window_id}")
        return True
    except subprocess.CalledProcessError as e:
        logging.error(f"✗ 关闭窗口失败: {window_id} - {e}")
        return False

def open_directory_with_fm(url, wait_time=None):
    """
    使用dde-file-manager -n在新窗口中打开目录URL，并返回窗口对象
    """
    global script_opened_windows

    if wait_time is None:
        wait_time = CONFIG["fm_open_delay"]

    logging.info(f"使用dde-file-manager -n打开URL: {url}")

    try:
        proc = subprocess.Popen(
            [CONFIG["dde_file_manager"], '-n', url],
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE
        )

        time.sleep(wait_time)

        if proc.poll() is None:
            logging.info("dde-file-manager 已在新窗口启动")
        else:
            logging.info(f"dde-file-manager 进程 (PID: {proc.pid}) 已退出")

        # 等待窗口出现并获取窗口信息
        time.sleep(CONFIG["fm_open_delay"])

        # 查找最新打开的窗口
        windows = find_all_windows()
        if windows:
            new_window = windows[-1]  # 最新窗口
            # 检查是否已跟踪过
            if new_window['window_id'] not in [w['window_id'] for w in script_opened_windows]:
                script_opened_windows.append(new_window)
                logging.info(f"✓ 已跟踪窗口: {new_window['window_id']} - {new_window['title']}")

        return proc

    except Exception as e:
        logging.error(f"打开dde-file-manager失败: {e}")
        return None

def delete_directories(source_path, usb_mounts):
    """
    删除源目录和U盘目录
    """
    test_dir_name = CONFIG["test_dir_name"]
    source_dir = os.path.join(source_path, test_dir_name)

    delete_success = True

    logging.info("=" * 70)
    logging.info("步骤8: 删除测试目录")
    logging.info("=" * 70)

    logging.info(f"删除源目录: {source_dir}")
    if os.path.exists(source_dir):
        try:
            shutil.rmtree(source_dir)
            logging.info(f"✓ 成功删除源目录: {source_dir}")
        except Exception as e:
            logging.error(f"✗ 删除源目录失败: {e}")
            delete_success = False
    else:
        logging.warning(f"源目录不存在: {source_dir}")

    for usb in usb_mounts:
        usb_dir = os.path.join(usb, test_dir_name)
        logging.info(f"删除U盘目录: {usb_dir}")
        if os.path.exists(usb_dir):
            try:
                shutil.rmtree(usb_dir)
                logging.info(f"✓ 成功删除U盘目录: {usb_dir}")
            except Exception as e:
                logging.error(f"✗ 删除U盘目录失败: {e}")
                delete_success = False
        else:
            logging.warning(f"U盘目录不存在: {usb_dir}")

    return delete_success

def close_all_script_windows():
    """
    关闭当前脚本打开的所有窗口
    """
    global script_opened_windows

    logging.info("=" * 70)
    logging.info("步骤9: 关闭当前脚本打开的所有窗口")
    logging.info("=" * 70)

    success_count = 0
    fail_count = 0

    if not script_opened_windows:
        logging.info("没有未关闭的脚本窗口")
        return True

    logging.info(f"共 {len(script_opened_windows)} 个窗口需要关闭")

    for window in script_opened_windows:
        window_id = window['window_id']
        logging.info(f"关闭窗口: {window_id} - {window['title']}")

        try:
            subprocess.run(['wmctrl', '-ic', window_id], check=True)
            success_count += 1
            time.sleep(0.3)
        except Exception as e:
            logging.error(f"✗ 关闭窗口失败: {window_id} - {e}")
            fail_count += 1

    if success_count > 0:
        logging.info(f"✓ 成功关闭 {success_count} 个窗口")
    if fail_count > 0:
        logging.error(f"✗ 关闭失败 {fail_count} 个窗口")

    # 清空跟踪列表
    script_opened_windows.clear()

    return fail_count == 0

# ==================== Coredump检测 ====================
def check_coredumps_and_analyze(start_time):
    """
    检查/var/lib/systemd/coredump目录下是否有包含"dde-file-m"的文件，
    且文件创建时间在脚本运行期间（即修改时间晚于start_time）
    如果有就调用export_coredumps.sh分析

    参数:
        start_time: 脚本开始运行时间（datetime对象）
    """
    logging.info("=" * 70)
    logging.info("步骤10: 检查coredump文件")
    logging.info("=" * 70)

    coredump_dir = CONFIG["coredump_dir"]

    # 检查coredump目录是否存在
    if not os.path.exists(coredump_dir):
        logging.warning(f"coredump目录不存在: {coredump_dir}")
        return False

    logging.info(f"检查目录: {coredump_dir}")
    logging.info(f"脚本开始时间: {start_time}")

    # 查找包含dde-file-m的文件，且修改时间在脚本开始时间之后
    coredump_files = []
    try:
        for filename in os.listdir(coredump_dir):
            if 'dde-file-m' in filename:
                filepath = os.path.join(coredump_dir, filename)
                if os.path.isfile(filepath):
                    # 检查文件修改时间是否在脚本开始时间之后
                    file_mtime = os.path.getmtime(filepath)
                    file_time = datetime.datetime.fromtimestamp(file_mtime)

                    if file_time >= start_time:
                        coredump_files.append(filepath)
                        logging.info(f"找到运行期间的coredump: {filename} (时间: {file_time})")
                    else:
                        logging.debug(f"忽略脚本运行前的coredump: {filename} (时间: {file_time})")

    except Exception as e:
        logging.error(f"扫描coredump目录失败: {e}")
        return False

    if not coredump_files:
        logging.info("✓ 未找到包含'dde-file-m'的coredump文件")
        logging.info("测试完成，dde-file-manager未崩溃")
        return True
    else:
        logging.info("=" * 70)
        logging.error(f"检测到 {len(coredump_files)} 个dde-file-manager的coredump文件！")
        logging.info("=" * 70)

        for coredump_file in coredump_files:
            filename = os.path.basename(coredump_file)
            file_size = os.path.getsize(coredump_file)
            mtime = datetime.datetime.fromtimestamp(os.path.getmtime(coredump_file))
            logging.error(f"  - {filename} ({file_size / 1024 / 1024:.2f} MB, 时间: {mtime})")

        # 调用export_coredumps.sh脚本
        logging.info("=" * 70)
        logging.info("调用export_coredumps.sh分析coredump...")
        logging.info("=" * 70)

        script_dir = CONFIG["script_dir"]
        export_script = os.path.join(script_dir, "..", "tool", "export_coredumps.sh")

        if not os.path.exists(export_script):
            logging.error(f"export_coredumps.sh脚本不存在: {export_script}")
            return False

        logging.info(f"脚本路径: {export_script}")

        try:
            # 使用--name参数只分析dde-file-manager的coredump
            result = subprocess.run(
                ['bash', export_script, '--name', 'dde-file-manager'],
                capture_output=True,
                text=True,
                timeout=120  # 2分钟超时
            )

            # 记录输出
            logging.info("=" * 70)
            logging.info("export_coredumps.sh 输出:")
            logging.info("=" * 70)

            if result.stdout:
                for line in result.stdout.split('\n'):
                    logging.info(line)

            if result.stderr:
                logging.error("export_coredumps.sh 错误输出:")
                for line in result.stderr.split('\n'):
                    logging.error(line)

            if result.returncode == 0:
                logging.info("✓ export_coredumps.sh 执行完成")
            else:
                logging.error(f"✗ export_coredumps.sh 执行失败，返回码: {result.returncode}")

        except subprocess.TimeoutExpired:
            logging.error("export_coredumps.sh 执行超时")
        except Exception as e:
            logging.error(f"执行export_coredumps.sh失败: {e}")

        logging.info("=" * 70)
        logging.info("检测到dde-file-manager崩溃，已调用export_coredumps.sh分析")
        logging.info("退出脚本")
        logging.info("=" * 70)

        return False

# ==================== 主函数 ====================
def main():
    """主函数"""
    global script_opened_windows

    log_file = setup_logging()
    script_start_time = datetime.datetime.now()

    logging.info("=" * 70)
    logging.info("USB复制粘贴与崩溃检测自动化测试开始")
    logging.info("=" * 70)
    logging.info(f"脚本开始时间: {script_start_time}")

    # 清空窗口跟踪列表
    script_opened_windows = []

    # 步骤1: 检查USB
    usb_mounts = find_usb_mount_points()
    if not usb_mounts:
        # 没有U盘，错误已经记录，直接退出
        return 1

    # 步骤2: 创建测试文件
    nested_dir = create_test_files_with_nested_dir(
        CONFIG["download_dir"],
        CONFIG["test_dir_name"],
        CONFIG["num_files"],
        CONFIG["max_file_size"]
    )

    if nested_dir is None:
        logging.error("创建测试目录失败")
        return 1

    # 步骤3: 打开下载目录
    logging.info("=" * 70)
    logging.info("步骤3: 打开下载目录")
    logging.info("=" * 70)

    source_outer_dir = os.path.join(CONFIG["download_dir"], CONFIG["test_dir_name"])
    source_url = f"file://{quote(source_outer_dir)}"
    open_directory_with_fm(source_url)

    # 步骤4: 延迟5秒，找到最近打开的窗口，选择，复制
    logging.info("=" * 70)
    logging.info("步骤4: 查找最近窗口并复制")
    logging.info("=" * 70)

    logging.info(f"等待 {CONFIG['find_window_delay']} 秒...")
    time.sleep(CONFIG['find_window_delay'])

    target_window = get_most_recent_window()
    if target_window:
        activate_window(target_window['window_id'])
        select_directory_by_name(CONFIG["test_dir_name"])
        copy_selected_directory()
    else:
        logging.error("未找到任何窗口，无法继续")
        return 1

    # 步骤5: 找到U盘所有挂载点的绝对路径后转换为url并使用dde-file-manager -n打开
    logging.info("=" * 70)
    logging.info("步骤5: 打开U盘目录")
    logging.info("=" * 70)

    # 打开第一个U盘挂载点
    usb_mount = usb_mounts[0]
    usb_url = f"file://{quote(usb_mount)}"
    open_directory_with_fm(usb_url)

    # 步骤6: 延迟5秒后找到最近打开的窗口，粘贴，按键执行后等待2秒后找焦点窗口，列举属性，关闭
    logging.info("=" * 70)
    logging.info("步骤6: 粘贴并列举焦点窗口属性")
    logging.info("=" * 70)

    # 找到最近打开的窗口
    recent_window = get_most_recent_window()
    if recent_window:
        activate_window(recent_window['window_id'])
        paste_directory()
    else:
        logging.error("未找到任何窗口")
        return 1

    # 按键执行后等待1秒
    logging.info(f"按键执行后等待 {CONFIG['focus_check_delay']} 秒...")
    time.sleep(CONFIG['focus_check_delay'])

    # 找焦点窗口
    focused_window = find_focused_window()
    if focused_window:
        # 列举窗口的全部属性
        # get_window_properties(focused_window['window_id'])

        # 关闭这个窗口
        close_window(focused_window['window_id'])
    else:
        logging.warning("未找到焦点窗口")

	# 等待拷贝结束
    logging.info(f"等待拷贝结束 {CONFIG['find_window_delay']} 秒...")
    time.sleep(CONFIG['find_window_delay'])


    # 步骤7: 删除测试目录
    delete_directories(CONFIG["download_dir"], usb_mounts)

    # 步骤8: 关闭当前脚本打开的所有窗口
    close_all_script_windows()

    # 步骤9: 检查coredump
    logging.info(f"脚本开始时间: {script_start_time}")
    success = check_coredumps_and_analyze(script_start_time)

    if not success:
        logging.error("检测到dde-file-manager崩溃")
        return 1

    logging.info("=" * 70)
    logging.info("测试完成，未检测到崩溃")
    logging.info(f"日志文件: {log_file}")
    logging.info("=" * 70)

    return 0

if __name__ == "__main__":
    sys.exit(main())
