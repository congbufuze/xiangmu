# 低功耗管理第一步与第二步

更新日期：2026 年 11 月 16 日。当前实现基于 FreeRTOS Tickless + RTC 经过时间补偿，在息屏空闲窗口进入短片段 STOP；单段最长约 90 ms，RTC、按键或触摸中断用于唤醒。本日先修正 RTC 时间单位、写保护及 SysTick 重装载问题，随后通过连接的 ST-Link 抓到仍然复位的实板调用栈，进一步替换了唤醒时的时钟恢复流程。验收须区分仿真、软件触发中断与手动按键/触摸/抬腕。

## 状态行为

## 深度睡眠状态

息屏并且 FreeRTOS 预计空闲不少于 25 tick 时，pre-sleep hook 使用 RTC WUT 设置最多约 90 ms 的 STOP 窗口。PA0/EXTI0、PB9/EXTI9_5 和 RTC WUT 均作为唤醒源。STOP 退出后，代码先冻结 SysTick，在 HSI 下恢复 PLL，再关闭本次 WUT。RTC 的 1/256 秒子秒时间差用于计算已流逝的 tick 计数。

Cortex-M SysTick 的 `VAL` 写入行为是清零而非载入目标值。STOP 返回时，代码按 FreeRTOS 端口的 `expected_ticks * counts_per_tick` 参考周期减去 RTC 实测经过计数，再将剩余计数写入 `LOAD`。这样端口后续根据当前计数器计算已经过的完整 tick，并用 `vTaskStepTick()` 补偿。时钟恢复本身改用 DWT 周期计时，避免被冻结的 RTOS tick 或粗粒度 RTC 干扰超时判断。单段睡眠限制约 90 ms，为 100 ms 传感器服务留出运行窗口；抬腕仍由 MPU 轮询任务处理。

RTC 子秒差的单位是 1/256 秒，必须乘每秒 CPU 周期数 `SystemCoreClock`，再除以 256；旧代码乘的是每毫秒周期数，少计约 1000 倍。换算使用 64 位中间值，并保留睡眠前已经过的 tick 小数部分。重启 SysTick 后等待 `VAL` 首次装载，随后恢复端口原 `LOAD`；补偿最多推进到下一任务期限前一个 tick，最后一个 tick 由中断处理，防止跨过调度期限。停用 RTC WUT 前先解除写保护，处理完成再锁定。

### 实板定位到的时钟恢复故障

ST-Link 读取实板时，先验证板内 Flash 各 ELF 加载段与本机文件一致，再暂停并冻结调试暂停期间的 IWDG，以获取一致的寄存器和 RAM 快照。捕获到的调用链为 `prvIdleTask → vPortSuppressTicksAndSleep(100) → PreSleepProcessing → SystemClock_Config → Error_Handler`，`PRIMASK=1`，`stop_entries/exits=24/23`，`watchdog_refresh_count=18`，故障钩子为零。RCC 当时仍使用 HSI，HSE/PLL 随后已 ready。寄存器及反汇编对应 PLL 的超时错误分支：最后一次超时差为 4ms，而 HAL 的 PLL 超时阈值是 2ms。

旧恢复流程用 RTC 子秒为 `HAL_GetTick` 提供时基，一个量化单位约 3.9ms；PLL 即使实际只等待短时间，也可能恰逢子秒边界而得到 4ms，误判为超过 2ms。随后通用 `Error_Handler` 保持全局关中断，KEY/触摸和任务均不能执行，最终 IWDG 复位。之前的测试把 `SystemClock_Config` 替换成直接设置 ready 的桩，因此没有覆盖这个真实故障。

当前使用独立的恢复函数：保留 STOP 前 PLLCFGR、总线分频和 Flash 等待周期，仅开启源振荡器、等待 PLL 就绪、切回 PLL。等待使用 HSI 下的 DWT 周期计数，并加有限循环预算；不依赖 RTC 的粗粒度日历、不依赖任务调度，也不重复初始化 TIM11。若外部时钟确实超时，尝试用 HSI/16×200/2 恢复相同的 100MHz，记录故障并暂停后续 STOP；本项目未使用要求 48MHz 的 USB/SDIO。若备用 PLL 也无法工作，则仍由故障处理和看门狗处理真实时钟失效，不能掩盖硬件故障。

调试器报告实板 Device ID 为 `0x441`、STM32F412、512KiB，当前 CubeMX 工程型号为 STM32F411RE。该差异已记录；本次没有在未核对封装、引脚和全部外设的情况下整体迁移芯片配置。

|资源|亮屏或变暗|息屏|
|---|---|---|
|LCD|正常局部刷新|关闭背光，Display Off + Sleep In；停止 TIM2 计数|
|UI|处理 LVGL，按定时器和事件运行|阻塞等待线程通知，不执行 LVGL handler，不轮询触摸|
|KEY|PA0 双边沿 EXTI，任务中 15ms 消抖|同一中断路径；首次有效按下只唤醒|
|触摸|正常输入轮询；PB9 IRQ 可提前通知|PB9 下降沿通知 UI 唤醒，直到手指松开前抑制控件输入|
|AHT21 与其他低频数据|约 500ms 采样|约 5 秒采样；醒后下一轮补采|
|MPU6050|每轮至少延时 100ms，处理抬腕与 FIFO|继续处理，保留当前抬腕和 DMP 计步路径|
|设置保存|变更后稳定 1 秒保存，失败间隔 5 秒重试|没有脏设置时一直阻塞；待保存时按期限醒来|
|看门狗和诊断|每秒运行|每秒运行，保持看门狗服务|
|CPU|无就绪任务时进入 Tickless Sleep|同左，任务活动减少后得到更长空闲窗口|

LCD Sleep Out 后等待 120ms，再执行 Display On；恢复页面失效和刷新，读取最新快照，完成一轮 LVGL 处理后恢复亮度。面板唤醒有这段等待，不能声称零延时。所有 LCD/LVGL 操作仍由 UITask 串行执行。

MPU 暂未切换到仅加速度低功耗模式，因为当前 DMP/陀螺仪/FIFO 工作方式需要保留；直接关闭它会破坏现有功能。触摸保留自动休眠与触摸中断，不调用需要复位唤醒的强制深睡接口。

## CubeMX 配置

`watch.ioc` 是配置来源，已通过本机 STM32CubeMX 6.15.0 命令行读取并在 `build/cubemx-power-check` 生成代码验证。

- FREERTOS 的 `USE_TICKLESS_IDLE` 设置为 `Built in functionality enabled`，值 1。
- PA0 KEY：`GPIO_EXTI0`，上拉、双边沿，EXTI0 优先级 5。
- PB9 TOUCH_INT：`GPIO_EXTI9`，上拉、下降沿，EXTI9_5 优先级 5。
- SYS 的 HAL timebase 保留 TIM11，供 HAL 初始化与启动阶段使用；SysTick 仍归 FreeRTOS。

GPIO 初始化来自本次 CubeMX 临时生成结果；主工程同步了相关中断入口、引脚定义与 FreeRTOS 设置。没有直接在主工程执行全量重新生成，避免覆盖已有应用和链接配置。

CubeMX 只能配置内核与引脚，无法表达 UI 资源归属、采样策略、保存期限和运行期时钟切换，这些放在应用代码中。`Key_Port_Init` 不再把 PA0 改回普通输入。

## Tickless 与 HAL 时基

使用工程现有 Cortex-M4F 端口的 `vPortSuppressTicksAndSleep`，不修改 FreeRTOS 中间件。没有就绪任务时，端口延长 SysTick 定时并补偿完整内核 tick。USER 定义的 pre-hook 先把预计空闲时间传给 `PreSleepProcessing`，由后者执行等待指令，随后才将宏参数置零，避免端口再次 WFI。post-hook 只记录诊断。普通 Sleep 清除 SLEEPDEEP，STOP 仅在进入期间设置，返回立即清除。

硬件初始化完成后，`AppPower_StartRuntimeClock` 停止 TIM11 毫秒中断与计数。应用覆盖 HAL 的弱 `HAL_GetTick`，用偏移量衔接启动阶段的 HAL 毫秒值与 RTOS tick。后续 HAL 超时、LVGL、设置保存和活动计时共享经过 Tickless 补偿的时间，避免再保留一个 1kHz 唤醒源。STOP 时钟恢复自身使用独立周期计数，不调用 HAL 超时接口。当前要求 RTOS tick 为 1000Hz，用编译期断言约束。

本工程 CMSIS 包装的 SysTick handler 会先读 CTRL，清除 COUNTFLAG；当前端口在唤醒后也要读取它判断是否到期。故在 FreeRTOSConfig 的 USER CODE 区启用自定义 SysTick handler，由 AppPower.c 直接调用内核 handler，不提前清标志。重新生成时必须保留该 USER CODE 区及 AppPower.c。

`HAL_GetTick` 在唤醒 ISR 内可能暂时落后于睡眠结束时刻，直到端口补偿结束；因此唤醒 ISR 只发送线程标志，不进行 I2C、绘图或依赖超时的操作。时间补偿有 tick 级量化误差，真实日历时间仍读取 RTC。

当前 CPU 100MHz，SysTick 24 位，内置端口单次可抑制约 167ms；即使任务等待 5 秒，也会分段醒来。RTC 约秒级中断、IMU 服务与喂狗仍在，不能将本轮结果说成 MCU 已连续深睡数秒。

`delay_ms` 在调度器运行的任务上下文中改为 `osDelay`，AHT21 的 80ms 转换等待等不再忙等。软件 I2C 的 `delay_us` 保留短时忙等，满足信号边沿间隔。

## 中断与消息一致性

EXTI 只设置专用线程标志，事件队列和页面操作留在任务中。线程标志可以记住“先发生通知、后开始等待”的情况，避免检查状态与阻塞之间丢失唤醒。KEY 的按下和释放均有通知，持续按住不重复发送导航。

应用事件仍保存在 AppEvent 中，额外线程标志只用于唤醒等待中的 UI，不替代业务事件位。传感器在息屏时继续更新快照，但不通知 UI；唤醒时 UI 无条件读取最新快照。

## 验证结果

- 11 月 16 日 Debug 固件构建并重新链接通过，已重编译所有引用诊断结构的源文件；RAM 91544 B / 128KiB，程序区 386812 B / 464KiB，向量区另 408 B。程序区占用约 81.41%，ELF 的 Flash 加载数据合计 387220 B。
- 原有核心、AHT21、抬腕模拟检查分别为 81、24、56 项，全部通过。
- 新增保存期限与采样策略 16 项检查，包括回绕、稳定期和失败退避。
- 对真实 FreeRTOS Cortex-M4F Tickless 端口执行 20 项模拟检查，覆盖取消进入、定时到期、提前 IRQ 和最大周期限制。
- 对实际 AppPower.c 执行 24 项模拟检查，覆盖 HAL 时基衔接、重复切换、回绕、中断通知及 Sleep 钩子。HAL/内核服务使用桩，WFI 在模拟中跳过，不代表真实硬件已完成验收。
- 11 月 16 日真实 AppPower.c 与真实 FreeRTOS 端口联合测试扩展至 123 项检查：约 90 ms 唤醒推进 90 tick、跨午夜、约 8 ms 提前按键、忽略 STOP、内核取消睡眠、RTC 配置失败回退、超期唤醒、连续 3000 次 STOP、PLL 延迟及 HSE 失效时恢复同频 HSI PLL 后停止进入 STOP。SysTick、RCC ready 与 DWT 为寄存器模型，实际时钟恢复 C 函数也参与测试；RTC/调度服务仍为桩。模型不能替代真实外部引脚验收。
- `scripts/verify_firmware.py` 检查 ELF 地址、向量、设置扇区隔离、任务栈及 CubeMX Tickless/EXTI 配置。

```text
python tests/run_sensor_driver.py <ARM GCC> tests/low_power_policy_test.c
python tests/run_tickless_port.py <ARM GCC>
python tests/run_power_runtime.py <ARM GCC>
python tests/run_stop_tickless.py <ARM GCC>
python scripts/verify_firmware.py
```

运行环境依赖仍为 `build/test-deps` 的 Unicorn 和 pyelftools。时基测试读取 Debug compile_commands 的 include 路径，先构建 Debug 工程再运行。

## 上板验收

2026-11-16：专用时钟恢复版本已通过 ST-Link 下载及校验。用户先确认息屏后短按 KEY 可以亮屏；补充 VS Code 调试保持配置后，用户进一步确认息屏不再复位、触摸和抬腕均可分别唤醒。提供的截图为 `watchdog_refresh_count=149`、`stop_entries=1227`、`stop_exits=1227`、`stop_last_elapsed_ms=20`。前三者为累计计数，最后一项是上一段 STOP/恢复耗时，允许上下变化，不能当累计计数。当前确认的是调试条件下的功能闭环；脱离调试后的长期稳定性、重复唤醒及电流测量仍待验收。

下载 `build/Debug/watch.elf`；该 ELF 保留设置分区，下载器仍应使用对应分段地址。不要用旧文件测试新改动。

1. 欢迎画面后主页正常，观察时间连续增加，KEY、菜单、滑动、点击正常。
2. 最后操作约 10 秒变暗、15 秒息屏；分别测试 KEY、触摸和抬腕唤醒，醒后主页时间追上 RTC。
3. 息屏后首次 KEY 只亮屏，再按进入菜单；触摸亮屏后松手，再点按钮，确认首次触摸不误操作。
4. 连续执行至少 30 次息屏与唤醒，检查无黑屏、异常复位、卡键、误触。短按不能被改成长按才能响应。
5. 醒后确认温湿度继续更新，设置修改并稳定至少 2 秒后复位，检查保存正常。
6. 调试观察 `runtime_tick_active=true`，`tickless_entries/exits` 持续增长；按键和触摸分别增长 `key_irq_count/touch_irq_count`。运行时 TIM11 的 CEN/UIE 应为零，TIM2 在息屏时 CEN 为零。
7. 息屏后约 5 秒观察一次 `environment_samples` 增长；`lcd_sleep_count/lcd_wake_count` 与休眠/唤醒次数相符。断点暂停 CPU 会影响时间观测，尽量用运行期内存刷新。
8. 11 月 16 日新增 `watchdog_refresh_count`，亮屏和息屏均应约每秒增加一次；`stop_last_elapsed_ms` 是上一段 STOP/恢复的 RTC 测量时长，可能因提前唤醒变小。`reset_flags` 只记录本次启动之前的复位原因，启动时随即清硬件标志，后续不会继续累积。`603979776 = 0x24000000` 包含 IWDG 位 `0x20000000`。重启会清零 RAM 中的输入计数，必须在失败后、自动重启前记录；重启后的 0 无法说明之前的 EXTI 是否到达。
9. `stop_clock_failures` 与 `stop_clock_fallbacks` 正常应保持零；若增加，`stop_clock_error` 的 1/2/3 分别表示源振荡器/PLL/切换超时。备用恢复成功后保留普通 Sleep，STOP 计数暂停，按键/触摸仍应可用。连接调试器测功能时可启用调试 STOP；测电流时必须脱离调试环境。

### VS Code 调试连接

`.vscode/launch.json` 在启动和重启调试时，通过 GDB 读改写 `DBGMCU_CR(0xE0042004)`，置位 `DBG_SLEEP|DBG_STOP`（0x3），保持低功耗期间调试访问；同时置位 `DBGMCU_APB1_FZ(0xE0042008)` 的 `DBG_IWDG_STOP`（0x1000），只在断点暂停 CPU 时冻结 IWDG。正常运行时看门狗仍由任务喂狗。配置使用本机 Cortex-Debug 1.12.1 支持的 `postLaunchCommands`、`postRestartCommands`，复位命令默认继承后者；无需重新编译固件。

未开启 STOP 调试保持时，进入低功耗可能使 GDB 报 `Target disconnected` 并结束会话，这条信息本身不证明 MCU 复位。调试保持会改变低功耗时钟行为和电流，只用于观察任务及唤醒；验证真实低功耗稳定性、测量电流时退出调试，断开下载器，并断电重新上电以清除调试保持设置。

PB9 是这轮新增使用的触摸中断输入，需要硬件上已连通 CST816 的 INT。若按键与抬腕都能唤醒但触摸不能，先看 `touch_irq_count`：不增长表示中断链路尚未成立；此时 UI 息屏后已不再通过 I2C 轮询触摸。2026-11-10 用户已实物确认触摸、按键、抬腕均能唤醒；后续 UI 精简版本仍需复测，见 `flash-optimization.md`。

本轮不输出电流和续航收益；完成硬件稳定性验收后，再在一致供电、调试连接条件下测亮屏、变暗、息屏 Sleep 的电流。比较数值时记录供电电压、传感器模式和是否连接下载器。

下载新 `build/Debug/watch.elf` 后，先确认息屏时 `stop_entries` 与 `stop_exits` 一起增长，再测试 KEY、触摸、抬腕唤醒；检查主页时间与温湿度持续更新，息屏至少 2 分钟不发生 IWDG 复位，并重复进行 30 轮按键和触摸唤醒。之后断开 ST-Link，用独立稳定 3.3 V 供电，分别记录亮屏、变暗、息屏电流并与普通 Sleep 版本基线比较。若仍出现约 30 秒复位，记录 `stop_entries/exits`、`tickless_entries/exits` 和复位标志；IWDG 最大分频与重装值对应约 32.8 秒超时，因此该时长提示调度/喂狗任务可能已停滞，但还需结合计数和复位标志确认。
