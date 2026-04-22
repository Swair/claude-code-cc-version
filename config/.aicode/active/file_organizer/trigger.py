#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
文件整理触发脚本（跨平台）
检测下载文件夹中文件数量，超过 50 个触发
返回 0 = 触发，返回 1 = 不触发
"""
import sys
from pathlib import Path


def check_downloads_folder():
    """检查下载文件夹文件数量"""
    # 跨平台获取 Downloads 目录
    if sys.platform == "win32":
        downloads = Path.home() / "Downloads"
    elif sys.platform == "darwin":  # macOS
        downloads = Path.home() / "Downloads"
    else:  # Linux
        downloads = Path.home() / "Downloads"
        # 如果 ~/Downloads 不存在，尝试 XDG 标准目录
        if not downloads.exists():
            xdg_download = Path.home() / ".local" / "share" / "Downloads"
            if xdg_download.exists():
                downloads = xdg_download

    if not downloads.exists():
        return False

    # 统计文件数量（不包括目录）
    file_count = sum(1 for f in downloads.iterdir() if f.is_file())

    if file_count > 50:
        print(f"下载文件夹有{file_count}个文件，建议整理")
        return True
    return False


if __name__ == "__main__":
    sys.exit(0 if check_downloads_folder() else 1)
