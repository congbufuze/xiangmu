"""Run the real AHT21 driver on ARM emulation, using a simulated I2C slave."""
from pathlib import Path
import subprocess
import sys
root = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(root / 'build' / 'test-deps'))
from elftools.elf.elffile import ELFFile
from unicorn import Uc, UC_ARCH_ARM, UC_MODE_THUMB, UC_MODE_MCLASS
from unicorn.arm_const import UC_ARM_REG_SP, UC_ARM_REG_LR, UC_ARM_REG_R0, UC_ARM_REG_PC
test_source = sys.argv[2] if len(sys.argv) > 2 else 'tests/sensor_driver_test.c'
out = root / 'build' / (Path(test_source).stem + '.elf')
subprocess.run([sys.argv[1], '-mcpu=cortex-m4', '-mthumb', '-mfloat-abi=soft',
                '-Os', '-std=c11', '-Wall', '-Wextra', '-Werror', '-ffreestanding',
                '-fno-builtin', '-nostdlib', '-IBSP/AHT21', '-IUser/Func/Inc', test_source,
                '-Ttests/core_logic.ld', '-lgcc', '-o', str(out)], cwd=root, check=True)
emu = Uc(UC_ARCH_ARM, UC_MODE_THUMB | UC_MODE_MCLASS)
emu.mem_map(0x10000, 128 * 1024)
emu.mem_map(0x20000000, 256 * 1024)
with out.open('rb') as stream:
    elf = ELFFile(stream)
    for segment in elf.iter_segments():
        if segment['p_type'] == 'PT_LOAD' and segment['p_filesz']:
            emu.mem_write(segment['p_vaddr'], segment.data())
    symbols = {s.name: s['st_value'] for s in elf.get_section_by_name('.symtab').iter_symbols()}
    entry = elf.header['e_entry']
stop = 0x2FFF0
emu.reg_write(UC_ARM_REG_SP, 0x2003FFF0)
emu.reg_write(UC_ARM_REG_LR, stop | 1)
emu.emu_start(entry | 1, stop, timeout=10_000_000, count=5_000_000)
assert emu.reg_read(UC_ARM_REG_PC) == stop, 'Test did not return'
result = emu.reg_read(UC_ARM_REG_R0)
checks = int.from_bytes(emu.mem_read(symbols['tests_passed'], 4), 'little')
assert result == 0, f'C test failed at {test_source}:{result}, after {checks} checks'
print(f'PASS: {checks} checks in {test_source}, executed from actual ARM C code')
