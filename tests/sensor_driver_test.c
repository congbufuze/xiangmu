/* Exercise the actual AHT21 driver with an emulated I2C slave. */
#include <stdint.h>
#define __AHT21_H
typedef struct { int IIC_SDA_PORT, IIC_SCL_PORT, IIC_SDA_PIN, IIC_SCL_PIN; } iic_bus_t;
#define GPIOA 0
#define GPIOC 2
#define GPIO_PIN_8 256
#define GPIO_PIN_9 512
#define __HAL_RCC_GPIOA_CLK_ENABLE() ((void)0)
#define __HAL_RCC_GPIOC_CLK_ENABLE() ((void)0)
static uint8_t input[64], output[64];
static unsigned in_at, out_at, ack_at, nack_at, stops;
static void IICStart(iic_bus_t *b) { (void)b; }
static void IICStop(iic_bus_t *b) { (void)b; ++stops; }
static void IICInit(iic_bus_t *b) { (void)b; }
static void IICSendByte(iic_bus_t *b, uint8_t v) { (void)b; if (out_at < 64) output[out_at++] = v; }
static uint8_t IICWaitAck(iic_bus_t *b) { (void)b; return ++ack_at == nack_at; }
static uint8_t IICReceiveByte(iic_bus_t *b) { (void)b; return input[in_at++ % 64]; }
static void IICSendAck(iic_bus_t *b) { (void)b; }
static void IICSendNotAck(iic_bus_t *b) { (void)b; }
static void delay_ms(unsigned ms) { (void)ms; }
#include "../BSP/AHT21/AHT21.c"
volatile uint32_t tests_passed;
#define CHECK(x) do { if (!(x)) return __LINE__; ++tests_passed; } while (0)
static void reset(void)
{
    in_at = out_at = ack_at = stops = 0; nack_at = 1000;
    for (unsigned i = 0; i < 64; ++i) input[i] = output[i] = 0;
}
static void sample(uint32_t h, uint32_t t)
{
    reset(); input[0] = 0x08; input[1] = 0x08;
    input[2] = h >> 12; input[3] = h >> 4;
    input[4] = (h << 4) | (t >> 16); input[5] = t >> 8; input[6] = t;
    unsigned crc = 255;
    for (unsigned i = 1; i < 7; ++i) {
        crc ^= input[i];
        for (unsigned b = 0; b < 8; ++b) crc = ((crc << 1) ^ ((crc & 128) ? 0x31 : 0)) & 255;
    }
    input[7] = crc;
}
int test_main(void)
{
    float h = -1, t = -1;
    sample(524288, 393216);
    CHECK(AHT_Read(&h, &t) == 0); CHECK(h == 50); CHECK(t == 25);
    CHECK(output[0] == 0x70 && output[1] == 0xAC && output[2] == 0x33 && output[3] == 0);
    sample(0, 209715);
    CHECK(AHT_Read(&h, &t) == 0); CHECK(t < -9.99f && t > -10.01f); CHECK(h == 0);
    sample(1048575, 707789);
    CHECK(AHT_Read(&h, &t) == 0); CHECK(h > 99.9f && h <= 100); CHECK(t > 84.9f && t < 85.1f);
    sample(0, 0); input[7] ^= 1; h = 123; t = 456;
    CHECK(AHT_Read(&h, &t) != 0); CHECK(h == 123 && t == 456);
    sample(0, 0); nack_at = 1;
    CHECK(AHT_Read(&h, &t) != 0); CHECK(stops == 1); CHECK(in_at == 0);
    sample(0, 0); nack_at = 3;
    CHECK(AHT_Read(&h, &t) != 0); CHECK(in_at == 0);
    reset(); for (unsigned i = 0; i < 64; ++i) input[i] = 0x88;
    CHECK(AHT_Read(&h, &t) != 0); CHECK(in_at == 20);
    reset(); nack_at = 1;
    CHECK(AHT_Init() != 0);
    reset(); input[0] = input[1] = 0x08;
    CHECK(AHT_Init() == 0); CHECK(out_at == 2);
    reset(); input[0] = 0; input[1] = 8;
    CHECK(AHT_Init() == 0);
    CHECK(output[1] == 0x70 && output[2] == 0xBE && output[3] == 8 && output[4] == 0);
    return 0;
}
