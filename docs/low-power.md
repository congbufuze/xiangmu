# 低功耗管理第一步与第二步

更新日期：2026 年 11 月 10 日。本轮完成外设与任务节能，以及 FreeRTOS 内置 Tickless + 普通 MCU Sleep。代码和模拟检查完成，真实电流、唤醒稳定性仍需下载到实物验证。STOP、RTC 补偿的深度休眠、MPU 运动中断方案属于后续阶段。

## 状态行为

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

使用工程现有 Cortex-M4F 端口的 `vPortSuppressTicksAndSleep`，不修改 FreeRTOS 中间件。没有就绪任务时，端口延长 SysTick 定时并补偿完整内核 tick。CubeMX 模板会把传入 pre-hook 的参数设为零，因此 `PreSleepProcessing` 自己执行 `DSB/WFI/ISB`；post-hook 只记录诊断。SLEEPDEEP 保持清零。

硬件初始化完成后，`AppPower_StartRuntimeClock` 停止 TIM11 毫秒中断与计数。应用覆盖 HAL 的弱 `HAL_GetTick`，用偏移量衔接启动阶段的 HAL 毫秒值与 RTOS tick。后续 HAL 超时、LVGL、设置保存和活动计时共享经过 Tickless 补偿的时间，避免再保留一个 1kHz 唤醒源。当前要求 RTOS tick 为 1000Hz，用编译期断言约束。

本工程 CMSIS 包装的 SysTick handler 会先读 CTRL，清除 COUNTFLAG；当前端口在唤醒后也要读取它判断是否到期。故在 FreeRTOSConfig 的 USER CODE 区启用自定义 SysTick handler，由 AppPower.c 直接调用内核 handler，不提前清标志。重新生成时必须保留该 USER CODE 区及 AppPower.c。

`HAL_GetTick` 在唤醒 ISR 内可能暂时落后于睡眠结束时刻，直到端口补偿结束；因此唤醒 ISR 只发送线程标志，不进行 I2C、绘图或依赖超时的操作。时间补偿有 tick 级量化误差，真实日历时间仍读取 RTC。

当前 CPU 100MHz，SysTick 24 位，内置端口单次可抑制约 167ms；即使任务等待 5 秒，也会分段醒来。RTC 约秒级中断、IMU 服务与喂狗仍在，不能将本轮结果说成 MCU 已连续深睡数秒。

`delay_ms` 在调度器运行的任务上下文中改为 `osDelay`，AHT21 的 80ms 转换等待等不再忙等。软件 I2C 的 `delay_us` 保留短时忙等，满足信号边沿间隔。

## 中断与消息一致性

EXTI 只设置专用线程标志，事件队列和页面操作留在任务中。线程标志可以记住“先发生通知、后开始等待”的情况，避免检查状态与阻塞之间丢失唤醒。KEY 的按下和释放均有通知，持续按住不重复发送导航。

应用事件仍保存在 AppEvent 中，额外线程标志只用于唤醒等待中的 UI，不替代业务事件位。传感器在息屏时继续更新快照，但不通知 UI；唤醒时 UI 无条件读取最新快照。

## 验证结果

- Debug 固件构建通过；RAM 92056 B / 128KiB，程序区 460764 B / 464KiB，向量区另 408 B。程序区占用约 96.98%。
- 原有核心、AHT21、抬腕模拟检查分别为 81、24、56 项，全部通过。
- 新增保存期限与采样策略 16 项检查，包括回绕、稳定期和失败退避。
- 对真实 FreeRTOS Cortex-M4F Tickless 端口执行 20 项模拟检查，覆盖取消进入、定时到期、提前 IRQ 和最大周期限制。
- 对实际 AppPower.c 执行 24 项模拟检查，覆盖 HAL 时基衔接、重复切换、回绕、中断通知及 Sleep 钩子。HAL/内核服务使用桩，WFI 在模拟中跳过，不代表真实硬件已完成验收。
- `scripts/verify_firmware.py` 检查 ELF 地址、向量、设置扇区隔离、任务栈及 CubeMX Tickless/EXTI 配置。

```text
python tests/run_sensor_driver.py <ARM GCC> tests/low_power_policy_test.c
python tests/run_tickless_port.py <ARM GCC>
python tests/run_power_runtime.py <ARM GCC>
python scripts/verify_firmware.py
```

运行环境依赖仍为 `build/test-deps` 的 Unicorn 和 pyelftools。时基测试读取 Debug compile_commands 的 include 路径，先构建 Debug 工程再运行。

## 上板验收

下载 `build/Debug/watch.elf`；该 ELF 保留设置分区，下载器仍应使用对应分段地址。不要用旧文件测试新改动。

1. 欢迎画面后主页正常，观察时间连续增加，KEY、菜单、滑动、点击正常。
2. 最后操作约 10 秒变暗、15 秒息屏；分别测试 KEY、触摸和抬腕唤醒，醒后主页时间追上 RTC。
3. 息屏后首次 KEY 只亮屏，再按进入菜单；触摸亮屏后松手，再点按钮，确认首次触摸不误操作。
4. 连续执行至少 30 次息屏与唤醒，检查无黑屏、异常复位、卡键、误触。短按不能被改成长按才能响应。
5. 醒后确认温湿度继续更新，设置修改并稳定至少 2 秒后复位，检查保存正常。
6. 调试观察 `runtime_tick_active=true`，`tickless_entries/exits` 持续增长；按键和触摸分别增长 `key_irq_count/touch_irq_count`。运行时 TIM11 的 CEN/UIE 应为零，TIM2 在息屏时 CEN 为零。
7. 息屏后约 5 秒观察一次 `environment_samples` 增长；`lcd_sleep_count/lcd_wake_count` 与休眠/唤醒次数相符。断点暂停 CPU 会影响时间观测，尽量用运行期内存刷新。

PB9 是这轮新增使用的触摸中断输入，需要硬件上已连通 CST816 的 INT。若按键与抬腕都能唤醒但触摸不能，先看 `touch_irq_count`：不增长表示中断链路尚未成立；此时 UI 息屏后已不再通过 I2C 轮询触摸。2026-11-10 用户已实物确认触摸、按键、抬腕均能唤醒；后续 UI 精简版本仍需复测，见 `flash-optimization.md`。

本轮不输出电流和续航收益；完成硬件稳定性验收后，再在一致供电、调试连接条件下测亮屏、变暗、息屏 Sleep 的电流。比较数值时记录供电电压、传感器模式和是否连接下载器。
