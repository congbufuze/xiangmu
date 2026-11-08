"""Build CMake compile commands sequentially on Windows; avoids Ninja pipe hangs.

Run CMake configure first. Rebuilds every translation unit and links with a
response file so the LVGL source list does not exceed Windows command limits.
"""
import argparse
import json
from pathlib import Path
import re
import subprocess
import sys

root = Path(__file__).resolve().parents[1]
build = root / "build" / "Debug"
commands = json.loads((build / "compile_commands.json").read_text())
parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument("--sources", nargs="+", help="Recompile only matching source path fragments, then relink")
args = parser.parse_args()
selected = [entry for entry in commands if not args.sources or any(
    fragment.lower() in entry["file"].replace("\\", "/").lower() for fragment in args.sources)]
if not selected:
    parser.error("No source files match")
log = build / "full-build.log"
with log.open("w", encoding="utf-8") as stream:
    for index, entry in enumerate(selected):
        (build / entry["output"]).parent.mkdir(parents=True, exist_ok=True)
        result = subprocess.run(entry["command"], cwd=build, shell=True,
                                stdout=stream, stderr=stream, timeout=90)
        if result.returncode:
            print(f"FAILED {entry['file']}; see {log}", flush=True)
            sys.exit(result.returncode)
        if index % 100 == 0:
            print(f"Compiled {index + 1}/{len(selected)}", flush=True)
    ninja = (build / "build.ninja").read_text()
    rule = re.search(r"^build watch\.elf:.*?(?=\n(?:#|build|\n))", ninja, re.M | re.S).group()
    # Object order follows the linker build edge, excluding order-only deps.
    objects = rule.splitlines()[0].split(":", 1)[1].split()[1:]
    objects = objects[:objects.index("||")] if "||" in objects else objects
    variables = dict(re.findall(r"^  (\w+) = (.*)$", rule, re.M))
    response = build / "CMakeFiles" / "firmware-link.rsp"
    response.write_text("\n".join(objects) + "\n" + variables.get("LINK_LIBRARIES", ""))
    compiler = commands[0]["command"].split(" ", 1)[0]
    link = f'{compiler} {variables.get("FLAGS", "")} {variables["LINK_FLAGS"]} @CMakeFiles/firmware-link.rsp -o watch.elf'
    result = subprocess.run(link, cwd=build, shell=True, stdout=stream, stderr=stream, timeout=180)
    if result.returncode:
        print(f"LINK FAILED; see {log}", flush=True)
        sys.exit(result.returncode)
print(log.read_text(encoding="utf-8", errors="replace")[-2000:])
print(f"Built {build / 'watch.elf'}")
