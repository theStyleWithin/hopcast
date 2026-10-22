#!/usr/bin/env python3
"""Lists the DLLs that the programs and libraries in a Windows folder import but that aren't there.
Usage: win-missing-dlls.py <folder> [more folders searched for the DLLs]. GPL-2.0-or-later."""
import os
import sys

import pefile  # pip install pefile

root = sys.argv[1]
extra = sys.argv[2:]
system = os.path.join(os.environ.get("SystemRoot", r"C:\Windows"), "System32")

have = {}
for base in [root] + extra:
    for d, _, files in os.walk(base):
        for f in files:
            if f.lower().endswith(".dll"):
                have.setdefault(f.lower(), os.path.join(d, f))
sysdlls = {f.lower() for f in os.listdir(system)} if os.path.isdir(system) else set()

missing = {}
for d, _, files in os.walk(root):
    for f in files:
        if not f.lower().endswith((".dll", ".exe")):
            continue
        try:
            pe = pefile.PE(os.path.join(d, f), fast_load=True)
            pe.parse_data_directories(directories=[pefile.DIRECTORY_ENTRY["IMAGE_DIRECTORY_ENTRY_IMPORT"]])
        except Exception:
            continue
        for entry in getattr(pe, "DIRECTORY_ENTRY_IMPORT", []):
            name = entry.dll.decode("ascii", "replace").lower()
            if name.startswith(("api-ms-win-", "ext-ms-")) or name in sysdlls:
                continue
            if name not in {x.lower() for x in os.listdir(d)} and not os.path.exists(os.path.join(root, "bin", "64bit", name)):
                missing.setdefault(name, []).append(f)

for name, users in sorted(missing.items()):
    where = have.get(name, "NOT FOUND ANYWHERE SEARCHED")
    print(f"{name}\t<- {', '.join(sorted(set(users))[:4])}\t[{where}]")
print(f"{len(missing)} missing")
