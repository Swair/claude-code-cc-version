#!/usr/bin/env python3
"""Skill Creator trigger: scan memory consolidation files for complex tasks."""
import json
import os
import re
import sys
from pathlib import Path

HOME = os.path.expanduser("~")
ROLES_DIR = os.path.join(HOME, ".prosophor", "roles")
PENDING_FILE = os.path.join(os.path.dirname(__file__), "pending.json")
MIN_TOOL_COUNT = 3

def find_complex_task():
    """Scan role memory consolidation files for multi-step tasks."""
    if not os.path.isdir(ROLES_DIR):
        return None

    for role_dir in sorted(os.listdir(ROLES_DIR)):
        cons_dir = os.path.join(ROLES_DIR, role_dir, "consolidation")
        if not os.path.isdir(cons_dir):
            continue

        for fname in sorted(os.listdir(cons_dir), reverse=True)[:5]:
            fpath = os.path.join(cons_dir, fname)
            if not fname.endswith(".md"):
                continue

            with open(fpath, "r", encoding="utf-8", errors="replace") as f:
                content = f.read()

            # Count tool mentions as a proxy for task complexity
            tool_mentions = len(re.findall(r"tool_use|Tool:|executed|ran command", content))
            if tool_mentions >= MIN_TOOL_COUNT:
                return {"role": role_dir, "date": fname.replace(".md", ""),
                        "path": fpath, "tool_count": tool_mentions}

    return None

# --- Main ---
task = find_complex_task()
if not task:
    sys.exit(0)  # No complex task found

# Write pending info
with open(PENDING_FILE, "w", encoding="utf-8") as f:
    json.dump(task, f, ensure_ascii=False, indent=2)

# Trigger reason (stdout → LLM input)
print(f"Complex task detected: role={task['role']}, date={task['date']}, tools={task['tool_count']}")
print(f"Memory file: {task['path']}")
sys.exit(1)  # Trigger!
