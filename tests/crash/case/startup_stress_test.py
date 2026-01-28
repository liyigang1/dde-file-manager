#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
================================================================================
DDE文件管理器 - 启动阶段压力测试脚本
================================================================================

【场景描述】
    复现文件管理器启动阶段崩溃问题，特别是 DHighDpi::init() 和 VtableHook
    相关的崩溃。通过频繁启动/关闭、多实例并发启动等方式触发问题。

【目标崩溃堆栈】
    #0  VtableHook::originalFun (libdxcb.so)
    #1  VtableHook::resetVfptrFun (libdxcb.so)
    #2  DHighDpi::init (libdxcb.so)
    #3  DPlatformIntegration::DPlatformIntegration (libdxcb.so)
    #4  DPlatformIntegrationPlugin::create (libdxcb.so)
    #5  QPlatformIntegrationFactory::create (libQt5Gui.so)
    ...
    #11 DApplication::DApplication (libdtkwidget.so)
    #12 main (dde-file-manager)

【测试内容】
    1. 快速循环启动/关闭文件管理器
    2. 多实例并发启动
    3. 启动后立即杀死进程
    4. 不同参数组合启动
    5. 监控崩溃次数和日志

【使用方法】
    # 默认配置运行（1000次启动循环，10个并发实例）
    python3 startup_stress_test.py

    # 自定义循环次数
    python3 startup_stress_test.py --cycles 500

    # 多实例并发模式
    python3 startup_stress_test.py --concurrent 5

    # 快速杀死模式（启动后立即kill）
    python3 startup_stress_test.py --quick-kill

    # 组合使用
    python3 startup_stress_test.py --cycles 200 --concurrent 3 --quick-kill

【命令行参数】
    --cycles        启动循环次数，默认 5000
    --concurrent    并发实例数，默认 10
    --quick-kill    启动后快速杀死（测试初始化阶段崩溃）
    --interval      启动间隔秒数，默认 1.0
    --kill-delay    quick-kill模式下的杀死延迟秒数，默认 0.5

【输出文件】
    监控报告：~/压测日志/startup_stress_YYYYMMDD_HHMMSS.json
    崩溃现场：~/压测日志/crash_YYYY-MM-DDTHH-MM-SS/

【依赖项】
    系统工具：scrot, xdotool
    Python库：psutil

    安装命令：
        sudo apt install scrot xdotool python3-psutil

【注意事项】
    1. 此测试会频繁启动/杀死进程，可能影响系统稳定性
    2. 建议在测试机上运行
    3. 需要图形界面环境
================================================================================
"""

import os
import subprocess
import time
import random
import psutil
import json
import datetime
import threading
import signal
from pathlib import Path
from concurrent.futures import ThreadPoolExecutor, as_completed

# ==================== 配置区 ====================
CONFIG = {
    "process_name": "dde-file-manager",
    "cycles": 1000,             # 启动循环次数
    "concurrent": 10,           # 并发实例数
    "quick_kill": False,        # 快速杀死模式
    "interval": 1.0,            # 启动间隔(秒)
    "kill_delay": 0.5,          # 杀死延迟(秒)
    "monitor_interval": 2,      # 监控间隔(秒)
    "log_dir": os.path.expanduser("~/压测日志"),
}

# 不同的启动参数组合，用于测试不同初始化路径
LAUNCH_VARIANTS = [
    [],                                          # 默认启动
    [os.path.expanduser("~")],                   # 打开主目录
    ["/tmp"],                                    # 打开/tmp
    ["--new-window"],                            # 新窗口
    [os.path.expanduser("~/Desktop")],           # 打开桌面
    ["computer:///"],                            # 计算机视图
    ["trash:///"],                               # 回收站
]


# ==================== 进程管理模块 ====================
class ProcessManager:
    """进程管理器"""

    @staticmethod
    def get_all_dfm_pids():
        """获取所有文件管理器进程PID"""
        pids = []
        for proc in psutil.process_iter(['pid', 'name']):
            try:
                if proc.info['name'] == CONFIG["process_name"]:
                    pids.append(proc.info['pid'])
            except (psutil.NoSuchProcess, psutil.AccessDenied):
                pass
        return pids

    @staticmethod
    def get_dfm_windows():
        """获取所有文件管理器窗口ID"""
        try:
            result = subprocess.run(
                ["xdotool", "search", "--name", "文件管理器"],
                capture_output=True, text=True, timeout=5
            )
            if result.returncode == 0 and result.stdout.strip():
                windows = result.stdout.strip().split('\n')
                print(f"  [窗口] 按名称找到 {len(windows)} 个窗口: {windows}")
                return windows
        except Exception as e:
            print(f"  [窗口] 按名称搜索失败: {e}")

        # 备用方案：按类名搜索
        try:
            result = subprocess.run(
                ["xdotool", "search", "--class", "dde-file-manager"],
                capture_output=True, text=True, timeout=5
            )
            if result.returncode == 0 and result.stdout.strip():
                windows = result.stdout.strip().split('\n')
                print(f"  [窗口] 按类名找到 {len(windows)} 个窗口: {windows}")
                return windows
        except Exception as e:
            print(f"  [窗口] 按类名搜索失败: {e}")

        print("  [窗口] 未找到任何窗口")
        return []

    @staticmethod
    def close_window(window_id):
        """关闭指定窗口（发送Alt+F4）"""
        try:
            print(f"  [窗口] 正在关闭窗口 {window_id}...")
            # 激活窗口
            ret1 = subprocess.run(["xdotool", "windowactivate", "--sync", window_id],
                         timeout=3, capture_output=True, text=True)
            print(f"  [窗口] windowactivate 返回: {ret1.returncode}, stderr: {ret1.stderr.strip()}")
            time.sleep(0.1)
            # 发送 Alt+F4 关闭
            ret2 = subprocess.run(["xdotool", "key", "--window", window_id, "alt+F4"],
                         timeout=3, capture_output=True, text=True)
            print(f"  [窗口] key alt+F4 返回: {ret2.returncode}, stderr: {ret2.stderr.strip()}")
            return True
        except Exception as e:
            print(f"  [窗口] 关闭窗口 {window_id} 失败: {e}")
            return False

    @staticmethod
    def close_all_dfm_windows(wait_timeout=3):
        """关闭所有文件管理器窗口"""
        windows = ProcessManager.get_dfm_windows()
        closed_count = 0

        print(f"  [窗口] 准备关闭 {len(windows)} 个窗口...")
        for win_id in windows:
            if ProcessManager.close_window(win_id):
                closed_count += 1
                time.sleep(0.2)

        # 等待窗口关闭
        if closed_count > 0:
            print(f"  [窗口] 等待窗口关闭，超时 {wait_timeout}s...")
            start = time.time()
            while time.time() - start < wait_timeout:
                remaining = ProcessManager.get_dfm_windows()
                if not remaining:
                    print(f"  [窗口] 所有窗口已关闭")
                    break
                time.sleep(0.3)
            else:
                remaining = ProcessManager.get_dfm_windows()
                print(f"  [窗口] 超时，仍有 {len(remaining)} 个窗口未关闭")

        return closed_count

    @staticmethod
    def kill_all_dfm():
        """强制杀死所有文件管理器进程"""
        pids = ProcessManager.get_all_dfm_pids()
        for pid in pids:
            try:
                os.kill(pid, signal.SIGKILL)
            except:
                pass
        return len(pids)

    @staticmethod
    def graceful_close_all(force_kill_timeout=5):
        """优雅关闭所有文件管理器（先关窗口，再杀进程）"""
        print(f"  [调试] graceful_close_all 被调用")
        # 先尝试关闭窗口
        closed = ProcessManager.close_all_dfm_windows(wait_timeout=3)

        # 等待进程退出
        start = time.time()
        while time.time() - start < force_kill_timeout:
            if not ProcessManager.is_dfm_running():
                print(f"  [调试] 进程已全部退出")
                return closed, 0  # 窗口关闭数, 强杀进程数

            time.sleep(0.3)

        # 超时后强制杀死
        killed = ProcessManager.kill_all_dfm()
        print(f"  [调试] 超时强杀 {killed} 个进程")
        return closed, killed

    @staticmethod
    def launch_dfm(args=None, wait=False):
        """启动文件管理器"""
        cmd = [CONFIG["process_name"]]
        if args:
            cmd.extend(args)

        try:
            if wait:
                proc = subprocess.Popen(cmd, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
                return proc
            else:
                subprocess.Popen(cmd, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
                return None
        except Exception as e:
            print(f"  [错误] 启动失败: {e}")
            return None

    @staticmethod
    def is_dfm_running():
        """检查是否有文件管理器在运行"""
        return len(ProcessManager.get_all_dfm_pids()) > 0


# ==================== 崩溃检测模块 ====================
class CrashDetector:
    """崩溃检测器"""

    def __init__(self):
        self.crash_count = 0
        self.crash_times = []
        self.lock = threading.Lock()

    def check_crash_files(self):
        """检查系统崩溃文件"""
        crash_dir = Path("/var/crash")
        if not crash_dir.exists():
            return []

        crashes = []
        for crash_file in crash_dir.glob("*.crash"):
            try:
                mtime = crash_file.stat().st_mtime
                # 只检查最近10分钟的崩溃
                if time.time() - mtime < 600:
                    if CONFIG["process_name"] in crash_file.name:
                        crashes.append(str(crash_file))
            except:
                pass
        return crashes

    def record_crash(self, timestamp=None):
        """记录一次崩溃"""
        with self.lock:
            self.crash_count += 1
            self.crash_times.append(timestamp or datetime.datetime.now().isoformat())

    def save_crash_scene(self, crash_id):
        """保存崩溃现场"""
        timestamp = datetime.datetime.now().isoformat().replace(':', '-')
        crash_dir = os.path.join(CONFIG["log_dir"], f"crash_{timestamp}")
        os.makedirs(crash_dir, exist_ok=True)

        # 截图
        try:
            screenshot_path = os.path.join(crash_dir, "screenshot.png")
            subprocess.run(["scrot", screenshot_path], timeout=10,
                         stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
        except:
            pass

        # 系统日志
        try:
            syslog_path = os.path.join(crash_dir, "syslog_tail.txt")
            result = subprocess.run(
                ["tail", "-n", "200", "/var/log/syslog"],
                capture_output=True, text=True, timeout=10
            )
            with open(syslog_path, 'w') as f:
                f.write(result.stdout)
        except:
            pass

        # dmesg
        try:
            dmesg_path = os.path.join(crash_dir, "dmesg_tail.txt")
            result = subprocess.run(
                ["dmesg", "--time-format=iso"],
                capture_output=True, text=True, timeout=10
            )
            with open(dmesg_path, 'w') as f:
                f.write('\n'.join(result.stdout.split('\n')[-100:]))
        except:
            pass

        # 复制crash文件
        try:
            for crash_file in self.check_crash_files():
                subprocess.run(["cp", crash_file, crash_dir], timeout=30)
        except:
            pass

        print(f"  [崩溃检测] 现场已保存: {crash_dir}")
        return crash_dir


# ==================== 压测执行器 ====================
class StartupStressTester:
    """启动压测执行器"""

    def __init__(self):
        self.crash_detector = CrashDetector()
        self.metrics = []
        self.start_time = None
        self.total_launches = 0
        self.successful_launches = 0
        self.failed_launches = 0

    def single_launch_test(self, iteration, variant_args=None):
        """单次启动测试"""
        timestamp = datetime.datetime.now().isoformat()
        result = {
            "iteration": iteration,
            "timestamp": timestamp,
            "args": variant_args or [],
            "status": "unknown"
        }

        # 记录启动前的进程数
        pids_before = set(ProcessManager.get_all_dfm_pids())
        print(f"  [调试] 启动前进程: {pids_before}")

        # 启动
        proc = ProcessManager.launch_dfm(variant_args, wait=True)
        self.total_launches += 1
        print(f"  [调试] 启动命令已执行, quick_kill={CONFIG['quick_kill']}")

        if CONFIG["quick_kill"]:
            # 快速杀死模式：等待短暂时间后关闭
            time.sleep(CONFIG["kill_delay"])

            # 检查是否还活着
            pids_after = set(ProcessManager.get_all_dfm_pids())
            new_pids = pids_after - pids_before
            print(f"  [调试] 启动后进程: {pids_after}, 新进程: {new_pids}")

            if new_pids:
                # 进程启动成功，先关窗口再杀进程
                print(f"  [调试] 检测到新进程，准备关闭窗口...")
                ProcessManager.close_all_dfm_windows(wait_timeout=1)
                time.sleep(0.2)
                # 如果还没退出，强制杀死
                for pid in new_pids:
                    try:
                        if psutil.pid_exists(pid):
                            os.kill(pid, signal.SIGKILL)
                    except:
                        pass
                result["status"] = "launched_and_closed"
                self.successful_launches += 1
            else:
                # 进程已经消失，可能崩溃
                result["status"] = "crashed_on_startup"
                self.failed_launches += 1
                self.crash_detector.record_crash(timestamp)
                self.crash_detector.save_crash_scene(iteration)
        else:
            # 普通模式：等待一段时间后检查
            time.sleep(CONFIG["interval"])

            pids_after = set(ProcessManager.get_all_dfm_pids())
            new_pids = pids_after - pids_before

            if new_pids:
                result["status"] = "running"
                result["pids"] = list(new_pids)
                self.successful_launches += 1

                # 获取内存信息
                try:
                    for pid in new_pids:
                        proc = psutil.Process(pid)
                        result["memory_mb"] = proc.memory_info().rss / (1024 * 1024)
                        break
                except:
                    pass
            else:
                result["status"] = "not_found"
                # 检查是否是崩溃
                if self.crash_detector.check_crash_files():
                    result["status"] = "crashed"
                    self.failed_launches += 1
                    self.crash_detector.record_crash(timestamp)
                    self.crash_detector.save_crash_scene(iteration)

        self.metrics.append(result)
        return result

    def concurrent_launch_test(self, iteration):
        """并发启动测试"""
        timestamp = datetime.datetime.now().isoformat()

        pids_before = set(ProcessManager.get_all_dfm_pids())

        # 并发启动多个实例
        threads = []
        for i in range(CONFIG["concurrent"]):
            variant = random.choice(LAUNCH_VARIANTS)
            t = threading.Thread(target=ProcessManager.launch_dfm, args=(variant,))
            threads.append(t)
            t.start()

        # 等待所有启动完成
        for t in threads:
            t.join()

        self.total_launches += CONFIG["concurrent"]

        if CONFIG["quick_kill"]:
            time.sleep(CONFIG["kill_delay"])
        else:
            time.sleep(CONFIG["interval"])

        # 检查结果
        pids_after = set(ProcessManager.get_all_dfm_pids())
        new_pids = pids_after - pids_before

        result = {
            "iteration": iteration,
            "timestamp": timestamp,
            "concurrent": CONFIG["concurrent"],
            "launched_count": len(new_pids),
            "status": "ok" if len(new_pids) >= CONFIG["concurrent"] else "partial"
        }

        if len(new_pids) < CONFIG["concurrent"]:
            missing = CONFIG["concurrent"] - len(new_pids)
            self.failed_launches += missing
            self.successful_launches += len(new_pids)

            if self.crash_detector.check_crash_files():
                result["status"] = "crashed"
                self.crash_detector.record_crash(timestamp)
                self.crash_detector.save_crash_scene(iteration)
        else:
            self.successful_launches += len(new_pids)

        # 关闭新启动的进程
        if CONFIG["quick_kill"]:
            # 先关窗口
            ProcessManager.close_all_dfm_windows(wait_timeout=1)
            time.sleep(0.2)
            # 再杀残留进程
            for pid in new_pids:
                try:
                    if psutil.pid_exists(pid):
                        os.kill(pid, signal.SIGKILL)
                except:
                    pass

        self.metrics.append(result)
        return result

    def run(self):
        """运行压测"""
        print("=" * 60)
        print("  DDE文件管理器 - 启动阶段压力测试")
        print(f"  循环次数: {CONFIG['cycles']}")
        print(f"  并发数: {CONFIG['concurrent']}")
        print(f"  快速杀死模式: {CONFIG['quick_kill']}")
        print("=" * 60)

        self.start_time = datetime.datetime.now()

        # 确保日志目录存在
        os.makedirs(CONFIG["log_dir"], exist_ok=True)

        # 先杀死所有现有进程
        killed = ProcessManager.kill_all_dfm()
        if killed:
            print(f"\n[准备] 已杀死 {killed} 个现有进程")
            time.sleep(1)

        print(f"\n[开始] 启动压测循环...")

        try:
            for i in range(CONFIG["cycles"]):
                if CONFIG["concurrent"] > 1:
                    result = self.concurrent_launch_test(i)
                else:
                    # 随机选择启动参数
                    variant = random.choice(LAUNCH_VARIANTS)
                    result = self.single_launch_test(i, variant)

                status_icon = "✓" if "crash" not in result["status"] else "✗"

                if (i + 1) % 10 == 0:
                    print(f"  [{i+1}/{CONFIG['cycles']}] {status_icon} "
                          f"成功:{self.successful_launches} 失败:{self.failed_launches} "
                          f"崩溃:{self.crash_detector.crash_count}")

                # 每轮之间清理进程
                print(f"  [调试] 准备清理...")
                ProcessManager.graceful_close_all()
                time.sleep(0.3)

        except KeyboardInterrupt:
            print("\n[中断] 用户终止测试，正在保存日志...")
            # 中断时立即保存报告
            self._save_report()
            self._print_summary()
            # 强制杀掉所有进程
            ProcessManager.kill_all_dfm()
            return
        except Exception as e:
            print(f"\n[错误] 测试异常: {e}")
        finally:
            # 清理所有进程
            try:
                ProcessManager.graceful_close_all()
            except:
                ProcessManager.kill_all_dfm()

            # 保存报告（如果还没保存）
            if not hasattr(self, '_report_saved'):
                self._save_report()
                self._print_summary()
                self._report_saved = True

    def _save_report(self):
        """保存测试报告"""
        self._report_saved = True
        report = {
            "test_type": "启动压测",
            "start_time": self.start_time.isoformat() if self.start_time else None,
            "end_time": datetime.datetime.now().isoformat(),
            "config": {
                "cycles": CONFIG["cycles"],
                "concurrent": CONFIG["concurrent"],
                "quick_kill": CONFIG["quick_kill"],
                "interval": CONFIG["interval"],
                "kill_delay": CONFIG["kill_delay"],
            },
            "summary": {
                "total_launches": self.total_launches,
                "successful_launches": self.successful_launches,
                "failed_launches": self.failed_launches,
                "crash_count": self.crash_detector.crash_count,
                "crash_times": self.crash_detector.crash_times,
            },
            "metrics": self.metrics
        }

        log_file = os.path.join(
            CONFIG["log_dir"],
            f"startup_stress_{self.start_time.strftime('%Y%m%d_%H%M%S')}.json"
        )

        with open(log_file, 'w', encoding='utf-8') as f:
            json.dump(report, f, ensure_ascii=False, indent=2)

        print(f"\n[报告] 已保存: {log_file}")

    def _print_summary(self):
        """打印测试摘要"""
        duration = datetime.datetime.now() - self.start_time

        print("\n" + "=" * 60)
        print("  压测结果摘要")
        print("=" * 60)
        print(f"  测试时长: {duration}")
        print(f"  总启动次数: {self.total_launches}")
        print(f"  成功次数: {self.successful_launches}")
        print(f"  失败次数: {self.failed_launches}")
        print(f"  崩溃次数: {self.crash_detector.crash_count}")
        if self.total_launches > 0:
            success_rate = self.successful_launches / self.total_launches * 100
            print(f"  成功率: {success_rate:.2f}%")
        print(f"  日志目录: {CONFIG['log_dir']}")
        print("=" * 60)


# ==================== 入口 ====================
if __name__ == "__main__":
    import argparse

    parser = argparse.ArgumentParser(
        description="DDE文件管理器启动阶段压力测试",
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog="""
示例:
  %(prog)s                           # 默认配置（5000次循环，10个并发）
  %(prog)s --cycles 500              # 500次循环
  %(prog)s --concurrent 5            # 5个并发实例
  %(prog)s --quick-kill              # 快速杀死模式
  %(prog)s --cycles 200 --concurrent 3 --quick-kill
        """
    )
    parser.add_argument("--cycles", type=int, default=5000,
                        help="启动循环次数 (默认: 5000)")
    parser.add_argument("--concurrent", type=int, default=10,
                        help="并发实例数 (默认: 10)")
    parser.add_argument("--quick-kill", action="store_true",
                        help="启动后快速杀死，测试初始化阶段崩溃")
    parser.add_argument("--interval", type=float, default=1.0,
                        help="启动间隔秒数 (默认: 1.0)")
    parser.add_argument("--kill-delay", type=float, default=0.5,
                        help="quick-kill模式的杀死延迟秒数 (默认: 0.5)")

    args = parser.parse_args()

    CONFIG["cycles"] = args.cycles
    CONFIG["concurrent"] = args.concurrent
    CONFIG["quick_kill"] = args.quick_kill
    CONFIG["interval"] = args.interval
    CONFIG["kill_delay"] = args.kill_delay

    tester = StartupStressTester()
    tester.run()
