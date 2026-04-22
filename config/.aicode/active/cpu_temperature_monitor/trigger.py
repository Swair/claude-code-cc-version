#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
CPU 温度监控触发脚本（跨平台）
返回 0 = 触发（温度超过阈值），返回 1 = 不触发
"""
import sys
import platform
import subprocess


def get_cpu_temp_windows():
    """Windows: 通过 WMI 获取 CPU 温度"""
    try:
        result = subprocess.run(
            ["powershell", "-Command",
             "(Get-WmiObject MSAcpi_ThermalZoneTemperature -root root/wmi | Select-Object -First 1).CurrentTemperature"],
            capture_output=True, text=True, timeout=5
        )
        if result.stdout.strip():
            raw_temp = int(result.stdout.strip())
            # 开氏温度 * 10 → 摄氏度
            return (raw_temp / 10) - 273.15
    except Exception as e:
        print(f"无法获取温度 (Windows): {e}", file=sys.stderr)
    return None


def get_cpu_temp_linux():
    """Linux: 读取 /sys/class/thermal 温度"""
    try:
        # 尝试多个温度传感器路径
        for path in ["/sys/class/thermal/thermal_zone0/temp",
                     "/sys/class/thermal/thermal_zone1/temp"]:
            try:
                with open(path, "r") as f:
                    temp = float(f.read().strip()) / 1000
                    if temp > 0:  # 有效温度
                        return temp
            except FileNotFoundError:
                continue
    except Exception as e:
        print(f"无法获取温度 (Linux): {e}", file=sys.stderr)
    return None


def get_cpu_temp_macos():
    """macOS: 通过 osx-cpu-temp 或 powermetrics 获取温度"""
    try:
        # 尝试 powermetrics (系统自带)
        result = subprocess.run(
            ["sudo", "powermetrics", "--samplers", "smc", "-n", "1", "-i", "100"],
            capture_output=True, text=True, timeout=5
        )
        for line in result.stdout.split("\n"):
            if "CPU die temperature" in line or "GPU die temperature" in line:
                # 解析："CPU die temperature: 45.00 C"
                parts = line.split(":")
                if len(parts) >= 2:
                    temp_str = parts[1].strip().replace("C", "").strip()
                    return float(temp_str)
    except Exception as e:
        print(f"无法获取温度 (macOS): {e}", file=sys.stderr)
    return None


def get_cpu_temp():
    """获取 CPU 温度（跨平台）"""
    system = platform.system()

    if system == "Windows":
        return get_cpu_temp_windows()
    elif system == "Linux":
        return get_cpu_temp_linux()
    elif system == "Darwin":  # macOS
        return get_cpu_temp_macos()

    return None


if __name__ == "__main__":
    THRESHOLD = 85.0  # 温度阈值（摄氏度）

    temp = get_cpu_temp()

    if temp is None:
        # 无法获取温度，不触发
        sys.exit(1)

    if temp > THRESHOLD:
        print(f"CPU 温度{temp:.1f}°C，超过阈值{THRESHOLD}°C")
        sys.exit(0)  # 触发
    else:
        sys.exit(1)  # 不触发
