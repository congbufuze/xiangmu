"""Execute actual ARM C code using Unicorn; no test firmware is flashed.

Dependencies: pip install --target build/test-deps unicorn pyelftools
Usage: python tests/run_core_logic.py [path/to/arm-none-eabi-gcc]
"""
from pathlib import Path
import subprocess
import sys

root = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(root / "build" / "test-deps"))
from elftools.elf.elffile import ELFFile
from unicorn import Uc, UC_ARCH_ARM, UC_MODE_THUMB, UC_MODE_MCLASS
from unicorn.arm_const import UC_ARM_REG_SP, UC_ARM_REG_LR, UC_ARM_REG_R0

compiler = sys.argv[1] if len(sys.argv) > 1 else "arm-none-eabi-gcc"
out = root / "build" / "core-logic.elf"
subprocess.run([compiler, "-mcpu=cortex-m4", "-mthumb", "-mfloat-abi=soft", "-Os",
                "-g", "-std=c11", "-Wall", "-Wextra", "-Werror", "-ffreestanding",
                "-fno-builtin", "-nostdlib", "-IUser/Func/Inc", "tests/core_logic_test.c",
                "User/Func/Src/AppTypes.c", "User/Func/Src/ScreenPower.c",
                "User/Func/Src/SettingsJournal.c", "-Ttests/core_logic.ld", "-o", str(out)],
               cwd=root, check=True)
emu = Uc(UC_ARCH_ARM, UC_MODE_THUMB | UC_MODE_MCLASS)
emu.mem_map(0x10000, 128 * 1024)
emu.mem_map(0x20000000, 256 * 1024)
with out.open("rb") as stream:
    elf = ELFFile(stream)
    for segment in elf.iter_segments():
        if segment["p_type"] == "PT_LOAD" and segment["p_filesz"]:
            emu.mem_write(segment["p_vaddr"], segment.data())
    symbols = {s.name: s["st_value"] for s in elf.get_section_by_name(".symtab").iter_symbols()}
    entry = elf.header["e_entry"]
stop = 0x2FFF0
emu.reg_write(UC_ARM_REG_SP, 0x2003FFF0)
emu.reg_write(UC_ARM_REG_LR, stop | 1)
emu.emu_start(entry | 1, stop, timeout=120_000_000, count=500_000_000)
from unicorn.arm_const import UC_ARM_REG_PC
if emu.reg_read(UC_ARM_REG_PC) != stop:
    raise RuntimeError("Emulation timed out before tests returned")
result = emu.reg_read(UC_ARM_REG_R0)
checks = int.from_bytes(emu.mem_read(symbols["tests_passed"], 4), "little")
if result:
    raise AssertionError(f"C test failed at core_logic_test.c:{result}, after {checks} checks")
print(f"PASS: {checks} checks executed from real ARM C code")
