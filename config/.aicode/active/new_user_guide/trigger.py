#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
新用户引导触发脚本（跨平台）
检测是否存在新用户标记文件
返回 0 = 触发（新用户），返回 1 = 不触发（老用户）
"""
import sys
from pathlib import Path


def check_first_run():
    """检查是否为新用户"""
    marker_file = Path.home() / ".aicode" / "first_run_marker"

    if marker_file.exists():
        print("检测到新用户标记文件")
        return True
    return False


if __name__ == "__main__":
    sys.exit(0 if check_first_run() else 1)
