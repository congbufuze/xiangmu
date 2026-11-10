"""Emulate production AppPower C; peripheral/kernel services are substituted.
WFI is skipped by a code hook: real wake latency and electrical IRQs need hardware.
"""
from pathlib import Path
import json
import subprocess
import sys
root = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(root / 'build/test-deps'))
from elftools.elf.elffile import ELFFile
from unicorn import Uc, UC_ARCH_ARM, UC_MODE_THUMB, UC_MODE_MCLASS, UC_HOOK_CODE
from unicorn.arm_const import UC_ARM_REG_SP, UC_ARM_REG_LR, UC_ARM_REG_PC, UC_ARM_REG_R0
commands = json.loads((root / 'build/Debug/compile_commands.json').read_text())
command = commands[0]['command']
includes = [arg for arg in command.split() if arg.startswith('-I')]
out = root / 'build/power_runtime_test.elf'
subprocess.run([sys.argv[1], '-mcpu=cortex-m4', '-mthumb', '-mfpu=fpv4-sp-d16',
    '-mfloat-abi=softfp', '-Os', '-g', '-std=c11', '-ffunction-sections', '-fdata-sections',
    '-nostdlib', '-DSTM32F411xE', '-DUSE_HAL_DRIVER', *includes,
    'tests/power_runtime_test.c', 'User/Func/Src/AppPower.c',
    '-Ttests/core_logic.ld', '-Wl,--gc-sections', '-lgcc', '-o', str(out)], cwd=root, check=True)
emu = Uc(UC_ARCH_ARM, UC_MODE_THUMB | UC_MODE_MCLASS)
for base, size in [(0x10000, 128*1024), (0x20000000, 256*1024),
                   (0x40014000, 4096), (0xE000E000, 4096)]: emu.mem_map(base, size)
with out.open('rb') as stream:
    elf = ELFFile(stream)
    for seg in elf.iter_segments():
        if seg['p_type'] == 'PT_LOAD' and seg['p_filesz']: emu.mem_write(seg['p_vaddr'], seg.data())
    symbols = {s.name: s['st_value'] for s in elf.get_section_by_name('.symtab').iter_symbols()}
    entry = elf.header['e_entry']
def code(uc, address, size, user):
    if uc.mem_read(address, size) == b'\x30\xbf': uc.reg_write(UC_ARM_REG_PC, (address + size) | 1)
emu.hook_add(UC_HOOK_CODE, code)
stop = 0x2FFF0
emu.reg_write(UC_ARM_REG_SP, 0x2003FFF0)
emu.reg_write(UC_ARM_REG_LR, stop | 1)
try:
    emu.emu_start(entry | 1, stop, count=100000)
except Exception:
    pc = emu.reg_read(UC_ARM_REG_PC)
    print('Fault PC', hex(pc), emu.mem_read(pc, 8).hex())
    raise
assert emu.reg_read(UC_ARM_REG_PC) == stop, 'test did not return'
result = emu.reg_read(UC_ARM_REG_R0)
checks = int.from_bytes(emu.mem_read(symbols['tests_passed'], 4), 'little')
assert result == 0, f'power_runtime_test.c:{result}, after {checks} checks'
print(f'PASS: {checks} checks on production HAL tick adapter and IRQ/power hooks')
