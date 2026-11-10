"""Exercise the actual FreeRTOS Cortex-M4F tickless port with synthetic MMIO.
This checks elapsed-tick arithmetic and abort paths, not real silicon timing.
"""
from pathlib import Path
import subprocess
import sys
root = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(root / 'build' / 'test-deps'))
from elftools.elf.elffile import ELFFile
from unicorn import Uc, UC_ARCH_ARM, UC_MODE_THUMB, UC_MODE_MCLASS, UC_HOOK_CODE, UC_HOOK_MEM_READ
from unicorn.arm_const import UC_ARM_REG_SP, UC_ARM_REG_LR, UC_ARM_REG_PC

rtos = 'Middlewares/Third_Party/FreeRTOS/Source'
out = root / 'build/tickless_port_test.elf'
subprocess.run([sys.argv[1], '-mcpu=cortex-m4', '-mthumb', '-mfpu=fpv4-sp-d16', '-mfloat-abi=softfp',
    '-Os', '-g', '-std=c11', '-ffunction-sections', '-fdata-sections', '-nostdlib',
    '-ICore/Inc', '-I' + rtos + '/include', '-I' + rtos + '/portable/GCC/ARM_CM4F',
    'tests/tickless_port_test.c', rtos + '/portable/GCC/ARM_CM4F/port.c',
    '-Ttests/core_logic.ld', '-Wl,--gc-sections', '-lgcc', '-o', str(out)],
    cwd=root, check=True)

checks = 0
for scenario, expected in [(0, 0), (1, 9), (2, 3), (3, 166)]:
    emu = Uc(UC_ARCH_ARM, UC_MODE_THUMB | UC_MODE_MCLASS)
    emu.mem_map(0x10000, 128 * 1024)
    emu.mem_map(0x20000000, 256 * 1024)
    emu.mem_map(0xE000E000, 4096)
    with out.open('rb') as stream:
        elf = ELFFile(stream)
        for seg in elf.iter_segments():
            if seg['p_type'] == 'PT_LOAD' and seg['p_filesz']:
                emu.mem_write(seg['p_vaddr'], seg.data())
        symbols = {s.name: s['st_value'] for s in elf.get_section_by_name('.symtab').iter_symbols()}
        entry = elf.header['e_entry']
    def put(addr, value): emu.mem_write(addr, value.to_bytes(4, 'little'))
    def get(addr): return int.from_bytes(emu.mem_read(addr, 4), 'little')
    put(symbols['scenario'], scenario)
    state = {'slept': False, 'flag_read': False}
    def code(uc, address, size, user):
        if address == (symbols['PreSleepProcessing'] & ~1):
            state['slept'] = True
            # Full sleep: counter reloads; early IRQ: 3.5 tick periods elapsed.
            if scenario == 2: put(0xE000E018, 650000)
            else: put(0xE000E018, get(0xE000E014) - 1000)
    def read(uc, access, address, size, value, user):
        if address == 0xE000E010 and state['slept'] and not state['flag_read']:
            # Production port first reads CTRL after the post-sleep hook.
            put(address, 5 | (0 if scenario == 2 else 1 << 16))
            state['flag_read'] = True
    emu.hook_add(UC_HOOK_CODE, code)
    emu.hook_add(UC_HOOK_MEM_READ, read)
    stop = 0x2FFF0
    emu.reg_write(UC_ARM_REG_SP, 0x2003FFF0)
    emu.reg_write(UC_ARM_REG_LR, stop | 1)
    emu.emu_start(entry | 1, stop, count=100000)
    assert emu.reg_read(UC_ARM_REG_PC) == stop, f'scenario {scenario} did not return'
    assert get(symbols['stepped_ticks']) == expected, (scenario, get(symbols['stepped_ticks']), expected)
    assert get(symbols['sleep_calls']) == (scenario != 0)
    assert get(symbols['wake_calls']) == (scenario != 0)
    assert get(0xE000E014) == 99999, 'normal SysTick reload not restored'
    checks += 5
print(f'PASS: {checks} checks on production tickless port (abort, full, early IRQ, max interval)')
