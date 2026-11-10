"""Check ELF load ranges and CubeMX/application task budgets without third-party packages."""
from pathlib import Path
import re
import struct

root = Path(__file__).resolve().parents[1]
image = root / "build" / "Debug" / "watch.elf"
data = image.read_bytes()
assert data[:7] == b"\x7fELF\x01\x01\x01", "Expected ELF32 little endian"
header = struct.unpack_from("<16sHHIIIIIHHHHHH", data)
assert header[2] == 40, "Expected ARM ELF"
entry, phoff, phentsize, phnum = header[4], header[5], header[9], header[10]
assert 0x0800C000 <= (entry & ~1) < 0x08080000, "Reset entry outside code Flash"
flash_bytes = 0
vectors = None
for index in range(phnum):
    kind, offset, vaddr, paddr, filesz, memsz, flags, align = struct.unpack_from(
        "<IIIIIIII", data, phoff + index * phentsize)
    if kind != 1 or not filesz:
        continue
    assert paddr + filesz <= 0x08004000 or paddr >= 0x0800C000, "ELF loads over settings sectors"
    if 0x08000000 <= paddr < 0x08080000:
        flash_bytes += filesz
        assert paddr + filesz <= 0x08080000, "Flash load exceeds device"
    if paddr == 0x08000000:
        vectors = struct.unpack_from("<II", data, offset)
assert vectors == (0x20020000, entry), "Unexpected initial stack/reset vector"

ioc = dict(line.split("=", 1) for line in (root / "watch.ioc").read_text().splitlines() if "=" in line)
rtos = (root / "Core/Src/freertos.c").read_text()
for task in ioc["FREERTOS.Tasks01"].split(";"):
    name, priority, stack, *_ = task.split(",")
    attrs = re.search(rf"{name}_attributes\s*=\s*\{{(.*?)\}}", rtos, re.S).group(1)
    assert re.search(rf"\.stack_size\s*=\s*{stack}\s*\*\s*4", attrs), f"Stack mismatch: {name}"
config = (root / "Core/Inc/FreeRTOSConfig.h").read_text()
assert re.search(r"configTOTAL_HEAP_SIZE\s+\(\(size_t\)" + ioc["FREERTOS.TOTAL_HEAP_SIZE"] + r"\)", config)
assert ioc["FREERTOS.Queues01"].startswith("KEYQueue,8,uint8_t,")
assert ioc["FREERTOS.configUSE_TICKLESS_IDLE"] == "1"
assert re.search(r"#define configUSE_TICKLESS_IDLE\s+1", config)
assert ioc["PA0-WKUP.Signal"] == "GPXTI0"
assert ioc["PA0-WKUP.GPIO_ModeDefaultEXTI"] == "GPIO_MODE_IT_RISING_FALLING"
assert ioc["PB9.Signal"] == "GPXTI9"
assert ioc["PB9.GPIO_ModeDefaultEXTI"] == "GPIO_MODE_IT_FALLING"
for irq in ("EXTI0_IRQn", "EXTI9_5_IRQn"):
    assert ioc["NVIC." + irq].startswith(r"true\:5\:"), irq
assert ";" not in ioc["FREERTOS.Queues01"]
assert "LV_USE_OS LV_OS_NONE" in (root / "Middlewares/GUI/lv_conf.h").read_text()
for old in ("Id_MessageQueue", "IdleBreak_MessageQueue", "Stop_MessageQueue", "HomeUpdateQueue"):
    assert old not in rtos
print(f"PASS: ARM ELF, reset vector, reserved settings sectors, 6 task stacks and RTOS heap; {flash_bytes} Flash load bytes")
