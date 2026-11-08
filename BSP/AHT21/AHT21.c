#include "AHT21.h"

iic_bus_t AHT_bus = {
    .IIC_SDA_PORT = GPIOC, .IIC_SCL_PORT = GPIOA,
    .IIC_SDA_PIN = GPIO_PIN_9, .IIC_SCL_PIN = GPIO_PIN_8,
};

static uint8_t send_command(const uint8_t *bytes, uint8_t length)
{
    IICStart(&AHT_bus);
    IICSendByte(&AHT_bus, 0x70);
    if (IICWaitAck(&AHT_bus)) { IICStop(&AHT_bus); return 1; }
    for (uint8_t i = 0; i < length; ++i) {
        IICSendByte(&AHT_bus, bytes[i]);
        if (IICWaitAck(&AHT_bus)) { IICStop(&AHT_bus); return 1; }
    }
    IICStop(&AHT_bus);
    return 0;
}

static uint8_t read_bytes(uint8_t *bytes, uint8_t length)
{
    IICStart(&AHT_bus);
    IICSendByte(&AHT_bus, 0x71);
    if (IICWaitAck(&AHT_bus)) { IICStop(&AHT_bus); return 1; }
    for (uint8_t i = 0; i < length; ++i) {
        bytes[i] = IICReceiveByte(&AHT_bus);
        if (i + 1U < length) IICSendAck(&AHT_bus);
        else IICSendNotAck(&AHT_bus);
    }
    IICStop(&AHT_bus);
    return 0;
}

uint8_t AHT_Read_Status(void)
{
    uint8_t status = 0xFF;
    (void)read_bytes(&status, 1);
    return status;
}

uint8_t AHT_Read_Cal_Enable(void)
{
    return (AHT_Read_Status() & 0x88U) == 0x08U;
}

void AHT_Reset(void)
{
    const uint8_t command = 0xBA;
    (void)send_command(&command, 1);
    delay_ms(20);
}

uint8_t AHT_Init(void)
{
    __HAL_RCC_GPIOA_CLK_ENABLE();
    __HAL_RCC_GPIOC_CLK_ENABLE();
    IICInit(&AHT_bus);
    delay_ms(40);
    uint8_t status;
    if (read_bytes(&status, 1)) return 1;
    if ((status & 0x08U) == 0U) {
        const uint8_t command[] = {0xBE, 0x08, 0x00};
        if (send_command(command, sizeof(command))) return 1;
        delay_ms(10);
    }
    for (uint8_t i = 0; i < 20; ++i) {
        if (read_bytes(&status, 1)) return 1;
        if ((status & 0x88U) == 0x08U) return 0;
        delay_ms(10);
    }
    return 1;
}

uint8_t AHT_Read(float *humi, float *temp)
{
    const uint8_t command[] = {0xAC, 0x33, 0x00};
    uint8_t data[7], status;
    if (!humi || !temp || send_command(command, sizeof(command))) return 1;
    delay_ms(80);
    for (uint8_t i = 0; ; ++i) {
        if (read_bytes(&status, 1)) return 1;
        if ((status & 0x80U) == 0U) break;
        if (i >= 19) return 1;
        delay_ms(5);
    }
    if (read_bytes(data, sizeof(data)) || (data[0] & 0x88U) != 0x08U) return 1;
    uint8_t crc = 0xFF;
    for (uint8_t i = 0; i < 6; ++i) {
        crc ^= data[i];
        for (uint8_t bit = 0; bit < 8; ++bit)
            crc = (crc & 0x80U) ? (uint8_t)((crc << 1) ^ 0x31U) : (uint8_t)(crc << 1);
    }
    if (crc != data[6]) return 1;
    uint32_t raw_h = ((uint32_t)data[1] << 12) | ((uint32_t)data[2] << 4) | (data[3] >> 4);
    uint32_t raw_t = ((uint32_t)(data[3] & 0x0F) << 16) | ((uint32_t)data[4] << 8) | data[5];
    *humi = raw_h * (100.0f / 1048576.0f);
    *temp = raw_t * (200.0f / 1048576.0f) - 50.0f;
    return 0;
}
