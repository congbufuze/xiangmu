"""Run production STOP hook AND FreeRTOS port against a small SysTick model.

RTC time, RCC ready flags and wake sources are synthetic. The production clock
restore code runs too, including PLL delays and HSE failure recovery. Electrical
wake and actual oscillator startup still require board validation.
"""
from pathlib import Path
import json
import subprocess
import sys

root = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(root / 'build/test-deps'))
from elftools.elf.elffile import ELFFile
from unicorn import Uc, UC_ARCH_ARM, UC_MODE_THUMB, UC_MODE_MCLASS
from unicorn import UC_HOOK_CODE, UC_HOOK_MEM_READ, UC_HOOK_MEM_WRITE
from unicorn.arm_const import UC_ARM_REG_SP, UC_ARM_REG_LR, UC_ARM_REG_PC, UC_ARM_REG_R0, UC_CPU_ARM_CORTEX_M4

commands = json.loads((root / 'build/Debug/compile_commands.json').read_text())
includes = [arg for arg in commands[0]['command'].split() if arg.startswith('-I')]
out = root / 'build/stop_tickless_test.elf'
# Unicorn rejects some wide branches inside GCC's multi-instruction IT blocks.
# Disable that conversion only for this emulator build; firmware flags stay intact.
subprocess.run([sys.argv[1], '-mcpu=cortex-m4', '-mthumb', '-mfpu=fpv4-sp-d16',
    '-mfloat-abi=softfp', '-Os', '-g', '-std=c11', '-ffreestanding',
    '-fno-if-conversion', '-fno-if-conversion2',
    '-ffunction-sections', '-fdata-sections', '-nostdlib',
    '-DSTM32F411xE', '-DUSE_HAL_DRIVER', *includes,
    'tests/stop_tickless_test.c', sys.argv[2] if len(sys.argv) > 2 else 'User/Func/Src/AppPower.c',
    'Middlewares/Third_Party/FreeRTOS/Source/portable/GCC/ARM_CM4F/port.c',
    '-Ttests/core_logic.ld', '-Wl,--gc-sections', '-lgcc', '-o', str(out)],
    cwd=root, check=True)

checks = 0
for scenario, expected in [(0, 90), (1, 90), (2, 8), (3, 0), (4, 0), (5, 99),
                           (6, 99), (7, 270000), (8, 288), (9, 90)]:
    emu = Uc(UC_ARCH_ARM, UC_MODE_THUMB | UC_MODE_MCLASS)
    emu.ctl_set_cpu_model(UC_CPU_ARM_CORTEX_M4)
    for base, length in [(0x10000, 128*1024), (0x20000000, 256*1024),
                         (0x40000000, 0x30000), (0xE000E000, 4096), (0xE0001000, 4096)]:
        emu.mem_map(base, length)
    with out.open('rb') as stream:
        elf = ELFFile(stream)
        for segment in elf.iter_segments():
            if segment['p_type'] == 'PT_LOAD' and segment['p_filesz']:
                emu.mem_write(segment['p_vaddr'], segment.data())
        symbols = {s.name: s['st_value'] for s in elf.get_section_by_name('.symtab').iter_symbols()}
        entry = elf.header['e_entry']
    def put(addr, value): emu.mem_write(addr, int(value).to_bytes(4, 'little'))
    def get(addr): return int.from_bytes(emu.mem_read(addr, 4), 'little')
    put(symbols['scenario'], scenario)
    timer = {'ctrl': 0, 'load': 99999, 'val': 50000, 'flag': False}
    unlock = {'stage': 0, 'enabled': False, 'violations': 0}
    clock = {'cycles': 0, 'cr': 0, 'hse_start': 0, 'pll_start': 0}

    def code(uc, address, size, user):
        if address == (symbols['vPortSuppressTicksAndSleep'] & ~1):
            timer['val'] = 50000
        if timer['ctrl'] & 1:
            if timer['val'] == 0:
                timer['val'] = timer['load']
            else:
                timer['val'] -= 1
                if timer['val'] == 0: timer['flag'] = True
        # Plain Sleep fallback receives a synthetic SysTick expiry.
        if uc.mem_read(address, size) == b'\x30\xbf':
            timer['val'] = timer['load'] - 100
            timer['flag'] = True
            uc.reg_write(UC_ARM_REG_PC, (address + size) | 1)

    def read(uc, access, address, size, value, user):
        if address == 0xE000E010:
            put(address, timer['ctrl'] | (int(timer['flag']) << 16))
            timer['flag'] = False
        elif address == 0xE000E014: put(address, timer['load'])
        elif address == 0xE000E018: put(address, timer['val'])
        elif address == 0xE0001004: put(address, clock['cycles'])
        elif address == 0x40023800:
            clock['cycles'] += 1000
            cr = clock['cr']
            if not cr & (1 << 16): cr &= ~(1 << 17)
            if not cr & (1 << 24): cr &= ~(1 << 25)
            if cr & (1 << 16) and scenario != 8 and clock['cycles'] - clock['hse_start'] >= 16000:
                cr |= 1 << 17
            source_ready = not get(0x40023804) & (1 << 22) or cr & (1 << 17)
            pll_delay = 48000 if scenario == 9 else 16000
            if cr & (1 << 24) and source_ready and clock['cycles'] - clock['pll_start'] >= pll_delay:
                cr |= 1 << 25
            clock['cr'] = cr
            put(address, cr)

    def write(uc, access, address, size, value, user):
        if address == 0xE000E010: timer['ctrl'] = value & 7
        elif address == 0xE000E014: timer['load'] = value & 0xFFFFFF
        elif address == 0xE000E018:
            timer['val'] = 0
            timer['flag'] = False
        elif address == 0x40002824:  # RTC WPR unlock sequence
            if value == 0xCA: unlock['stage'] = 1
            elif value == 0x53 and unlock['stage'] == 1: unlock['enabled'] = True
            else: unlock.update(stage=0, enabled=False)
        elif address == 0x40002808:
            # HAL arm is stubbed; verify the production disarm is protected.
            if not value & (1 << 10) and not unlock['enabled']:
                unlock['violations'] += 1
        elif address == 0x40023800:
            previous = clock['cr']
            if value & (1 << 16) and not previous & (1 << 16): clock['hse_start'] = clock['cycles']
            if value & (1 << 24) and not previous & (1 << 24): clock['pll_start'] = clock['cycles']
            clock['cr'] = value
        elif address == 0x40023808:
            if value & 3 == 2 and clock['cr'] & (1 << 25): value = (value & ~12) | 8
            elif value & 3 == 0: value &= ~12
            # MMIO write callbacks precede the actual write. Apply read-only
            # SWS on the following read instead of writing over that store.
            clock['cfgr'] = value

    def read_cfgr(uc, access, address, size, value, user):
        put(address, clock.get('cfgr', 0))

    emu.hook_add(UC_HOOK_CODE, code)
    emu.hook_add(UC_HOOK_MEM_READ, read)
    emu.hook_add(UC_HOOK_MEM_WRITE, write)
    emu.hook_add(UC_HOOK_MEM_READ, read_cfgr, begin=0x40023808, end=0x4002380B)
    stop = 0x2FFF0
    emu.reg_write(UC_ARM_REG_SP, 0x2003FFF0)
    emu.reg_write(UC_ARM_REG_LR, stop | 1)
    try:
        emu.emu_start(entry | 1, stop, count=10000000)
    except Exception:
        pc = emu.reg_read(UC_ARM_REG_PC)
        print('Fault PC', hex(pc), 'instruction', emu.mem_read(pc, 8).hex())
        raise
    assert emu.reg_read(UC_ARM_REG_PC) == stop, f'scenario {scenario} did not return'
    assert emu.reg_read(UC_ARM_REG_R0) == 0
    actual = get(symbols['stepped_ticks'])
    assert actual == expected, (scenario, actual, expected)
    assert get(symbols['final_reload']) == 99999
    assert get(symbols['final_primask']) == 0
    assert get(symbols['clock_errors']) == 0
    assert get(symbols['restore_failures']) == (scenario == 8)
    assert get(symbols['restore_fallbacks']) == (scenario == 8)
    assert get(symbols['final_clock']) & 12 == 8
    if scenario == 8:
        assert get(symbols['final_pll']) == 16 | (200 << 6) | (4 << 24)
        checks += 1
    if scenario < 4 or scenario >= 6:
        assert get(symbols['stop_calls']) == (3000 if scenario == 7 else 1)
        assert get(symbols['rtc_disarmed']) == 1
        assert unlock['violations'] == 0, 'RTC disarmed without unlocking write protection'
        assert not unlock['enabled'], 'RTC left unprotected'
        checks += 4
    checks += 9
    print(f'PASS scenario {scenario}: kernel advanced {actual} ticks')
print(f'PASS: {checks} STOP/FreeRTOS integration checks')
