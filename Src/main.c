#include <stdint.h>

// ---------------- Register defines ----------------

#define RCC_BASE     0x40021000UL
#define RCC_CR       (*(volatile uint32_t *)(RCC_BASE + 0x00))
#define RCC_CFGR     (*(volatile uint32_t *)(RCC_BASE + 0x04))
#define RCC_APB2ENR  (*(volatile uint32_t *)(RCC_BASE + 0x18))

#define FLASH_BASE   0x40022000UL
#define FLASH_ACR    (*(volatile uint32_t *)(FLASH_BASE + 0x00))

#define GPIOB_BASE   0x40010C00UL
#define GPIOB_CRL    (*(volatile uint32_t *)(GPIOB_BASE + 0x00))
#define GPIOB_ODR    (*(volatile uint32_t *)(GPIOB_BASE + 0x0C))

#define GPIOC_BASE   0x40011000UL
#define GPIOC_CRH    (*(volatile uint32_t *)(GPIOC_BASE + 0x04))
#define GPIOC_ODR    (*(volatile uint32_t *)(GPIOC_BASE + 0x0C))

#define ADC1_BASE    0x40012400UL
#define ADC1_SR      (*(volatile uint32_t *)(ADC1_BASE + 0x00))
#define ADC1_CR2     (*(volatile uint32_t *)(ADC1_BASE + 0x08))
#define ADC1_SMPR1   (*(volatile uint32_t *)(ADC1_BASE + 0x0C))
#define ADC1_SMPR2   (*(volatile uint32_t *)(ADC1_BASE + 0x10))
#define ADC1_SQR1    (*(volatile uint32_t *)(ADC1_BASE + 0x2C))
#define ADC1_SQR3    (*(volatile uint32_t *)(ADC1_BASE + 0x34))
#define ADC1_DR      (*(volatile uint32_t *)(ADC1_BASE + 0x4C))

#define SYSTICK_CTRL (*(volatile uint32_t *)(0xE000E010))
#define SYSTICK_LOAD (*(volatile uint32_t *)(0xE000E014))
#define SYSTICK_VAL  (*(volatile uint32_t *)(0xE000E018))

#define LED_PIN  13

// ---------------- Globals ----------------

static volatile uint32_t ms_ticks = 0;

void SysTick_Handler(void)
{
    ms_ticks++;
}

static uint32_t millis(void)
{
    return ms_ticks;
}

static void delay(volatile uint32_t count)
{
    while (count--) __asm volatile ("nop");
}

static void delay_ms(uint32_t ms)
{
    uint32_t start = millis();
    while ((millis() - start) < ms);
}

// ---------------- Clock setup (72MHz) ----------------

static void clock_init(void)
{
    FLASH_ACR = (1 << 4) | (2 << 0);

    RCC_CR |= (1 << 16);            // HSE on
    while (!(RCC_CR & (1 << 17)));  // wait ready

    RCC_CFGR |= (7 << 18);          // PLL x9
    RCC_CFGR |= (1 << 16);          // PLL src = HSE
    RCC_CFGR |= (4 << 8);           // APB1 /2

    RCC_CR |= (1 << 24);            // PLL on
    while (!(RCC_CR & (1 << 25)));  // wait lock

    RCC_CFGR |= (2 << 0);           // switch to PLL
    while (((RCC_CFGR >> 2) & 3) != 2);
}

static void systick_init(void)
{
    SYSTICK_LOAD = 72000 - 1;   // 1ms tick @ 72MHz
    SYSTICK_VAL = 0;
    SYSTICK_CTRL = (1 << 2) | (1 << 1) | (1 << 0);
}

// ---------------- GPIO setup ----------------

static void gpio_init(void)
{
    RCC_APB2ENR |= (1 << 3) | (1 << 4) | (1 << 0); // GPIOB, GPIOC, AFIO

    GPIOB_CRL = 0;
    GPIOB_CRL |= (0x7 << (6 * 4)); // PB6 open-drain (I2C SCL)
    GPIOB_CRL |= (0x7 << (7 * 4)); // PB7 open-drain (I2C SDA)

    GPIOC_CRH = 0;
    GPIOC_CRH |= (0x2 << ((13 - 8) * 4)); // PC13 push-pull output

    GPIOB_ODR |= (1 << 6) | (1 << 7); // idle high
    GPIOC_ODR |= (1 << LED_PIN);      // LED off (active low)
}

// ---------------- LED heartbeat ----------------

static void led_blink(void)
{
    static uint32_t last = 0;
    if (millis() - last >= 1000)
    {
        GPIOC_ODR ^= (1 << LED_PIN);
        last = millis();
    }
}

// ---------------- I2C bit-bang ----------------

#define SCL_H() (GPIOB_ODR |= (1 << 6))
#define SCL_L() (GPIOB_ODR &= ~(1 << 6))
#define SDA_H() (GPIOB_ODR |= (1 << 7))
#define SDA_L() (GPIOB_ODR &= ~(1 << 7))

static void i2c_delay(void) { delay(100); }

static void i2c_start(void)
{
    SDA_H(); SCL_H(); i2c_delay();
    SDA_L(); i2c_delay();
    SCL_L();
}

static void i2c_stop(void)
{
    SDA_L(); i2c_delay();
    SCL_H(); i2c_delay();
    SDA_H(); i2c_delay();
}

static void i2c_write(uint8_t data)
{
    for (uint8_t i = 0; i < 8; i++)
    {
        if (data & 0x80) SDA_H(); else SDA_L();
        i2c_delay();
        SCL_H(); i2c_delay();
        SCL_L(); i2c_delay();
        data <<= 1;
    }
    SDA_H(); i2c_delay();
    SCL_H(); i2c_delay();
    SCL_L();
}

// ---------------- SSD1306 OLED ----------------

#define OLED_ADDR 0x3C

static void oled_cmd(uint8_t c)
{
    i2c_start();
    i2c_write(OLED_ADDR << 1);
    i2c_write(0x00);
    i2c_write(c);
    i2c_stop();
}

static void oled_data(uint8_t d)
{
    i2c_start();
    i2c_write(OLED_ADDR << 1);
    i2c_write(0x40);
    i2c_write(d);
    i2c_stop();
}

static void oled_init(void)
{
    delay_ms(50);
    uint8_t init_cmds[] = {
        0xAE, 0x20, 0x00, 0xB0, 0xC8, 0x00, 0x10, 0x40,
        0x81, 0x7F, 0xA1, 0xA6, 0xA8, 0x3F, 0xA4,
        0xD3, 0x00, 0xD5, 0x80, 0xD9, 0xF1, 0xDA, 0x12,
        0xDB, 0x40, 0x8D, 0x14, 0xAF
    };
    for (uint8_t i = 0; i < sizeof(init_cmds); i++)
        oled_cmd(init_cmds[i]);
}

static void oled_clear(void)
{
    for (uint8_t page = 0; page < 8; page++)
    {
        oled_cmd(0xB0 + page);
        oled_cmd(0x00);
        oled_cmd(0x10);
        for (uint8_t col = 0; col < 128; col++)
            oled_data(0x00);
    }
}

static void oled_set_cursor(uint8_t page, uint8_t col)
{
    oled_cmd(0xB0 + page);
    oled_cmd(col & 0x0F);
    oled_cmd(0x10 | (col >> 4));
}

// ---------------- 5x7 font (only chars we need) ----------------

static void oled_char(char c)
{
    static const uint8_t blank[] = {0x00,0x00,0x00,0x00,0x00};
    static const uint8_t d0[] = {0x3E,0x51,0x49,0x45,0x3E};
    static const uint8_t d1[] = {0x00,0x42,0x7F,0x40,0x00};
    static const uint8_t d2[] = {0x42,0x61,0x51,0x49,0x46};
    static const uint8_t d3[] = {0x21,0x41,0x45,0x4B,0x31};
    static const uint8_t d4[] = {0x18,0x14,0x12,0x7F,0x10};
    static const uint8_t d5[] = {0x27,0x45,0x45,0x45,0x39};
    static const uint8_t d6[] = {0x3C,0x4A,0x49,0x49,0x30};
    static const uint8_t d7[] = {0x01,0x71,0x09,0x05,0x03};
    static const uint8_t d8[] = {0x36,0x49,0x49,0x49,0x36};
    static const uint8_t d9[] = {0x06,0x49,0x49,0x29,0x1E};
    static const uint8_t cA[] = {0x7E,0x11,0x11,0x11,0x7E};
    static const uint8_t cB[] = {0x7F,0x49,0x49,0x49,0x36};
    static const uint8_t cC[] = {0x3E,0x41,0x41,0x41,0x22};
    static const uint8_t cE[] = {0x7F,0x49,0x49,0x49,0x41};
    static const uint8_t cI[] = {0x00,0x41,0x7F,0x41,0x00};
    static const uint8_t cL[] = {0x7F,0x40,0x40,0x40,0x40};
    static const uint8_t cM[] = {0x7F,0x02,0x04,0x02,0x7F};
    static const uint8_t cN[] = {0x7F,0x04,0x08,0x10,0x7F};
    static const uint8_t cO[] = {0x3E,0x41,0x41,0x41,0x3E};
    static const uint8_t cP[] = {0x7F,0x09,0x09,0x09,0x06};
    static const uint8_t cR[] = {0x7F,0x09,0x19,0x29,0x46};
    static const uint8_t cS[] = {0x26,0x49,0x49,0x49,0x32};
    static const uint8_t cT[] = {0x01,0x01,0x7F,0x01,0x01};
    static const uint8_t cU[] = {0x3F,0x40,0x40,0x40,0x3F};
    static const uint8_t cV[] = {0x1F,0x20,0x40,0x20,0x1F};
    static const uint8_t cY[] = {0x03,0x04,0x78,0x04,0x03};
    static const uint8_t dot[] = {0x00,0x60,0x60,0x00,0x00};
    static const uint8_t minus[] = {0x08,0x08,0x08,0x08,0x08};

    const uint8_t *f;
    switch (c)
    {
        case '0': f = d0; break;
        case '1': f = d1; break;
        case '2': f = d2; break;
        case '3': f = d3; break;
        case '4': f = d4; break;
        case '5': f = d5; break;
        case '6': f = d6; break;
        case '7': f = d7; break;
        case '8': f = d8; break;
        case '9': f = d9; break;
        case 'A': f = cA; break;
        case 'B': f = cB; break;
        case 'C': f = cC; break;
        case 'E': f = cE; break;
        case 'I': f = cI; break;
        case 'L': f = cL; break;
        case 'M': f = cM; break;
        case 'N': f = cN; break;
        case 'O': f = cO; break;
        case 'P': f = cP; break;
        case 'R': f = cR; break;
        case 'S': f = cS; break;
        case 'T': f = cT; break;
        case 'U': f = cU; break;
        case 'V': f = cV; break;
        case 'Y': f = cY; break;
        case '.': f = dot; break;
        case '-': f = minus; break;
        default:  f = blank; break;
    }

    for (uint8_t i = 0; i < 5; i++) oled_data(f[i]);
    oled_data(0x00);
}

static void oled_print(const char *s)
{
    while (*s) oled_char(*s++);
}

static void oled_print_at(uint8_t page, uint8_t col, const char *s)
{
    oled_set_cursor(page, col);
    // pad with spaces to 10 chars so old digits don't stick around
    uint8_t n = 0;
    while (*s) { oled_char(*s++); n++; }
    while (n < 10) { oled_char(' '); n++; }
}

// ---------------- ADC ----------------

static void adc_init(void)
{
    RCC_APB2ENR |= (1 << 9); // ADC1 clock

    RCC_CFGR &= ~(3 << 14);
    RCC_CFGR |= (2 << 14); // ADC clk = 72MHz/6 = 12MHz

    ADC1_CR2 |= (1 << 23); // enable temp sensor + vrefint

    ADC1_SMPR2 |= (7 << 6);   // channel 2 long sample time
    ADC1_SMPR1 |= (7 << 18) | (7 << 21); // channel 16, 17 long sample

    ADC1_CR2 |= (1 << 0); // ADC on
    delay(10000);

    ADC1_CR2 |= (1 << 3); // reset calibration
    while (ADC1_CR2 & (1 << 3));

    ADC1_CR2 |= (1 << 2); // calibrate
    while (ADC1_CR2 & (1 << 2));
}

static uint16_t adc_read(uint8_t channel)
{
    ADC1_SQR1 = 0;
    ADC1_SQR3 = channel;

    ADC1_CR2 |= (7 << 17); // software trigger select
    ADC1_CR2 |= (1 << 20); // ext trigger enable
    ADC1_CR2 |= (1 << 22); // start conversion

    while (!(ADC1_SR & (1 << 1)));
    return (uint16_t)ADC1_DR;
}

// ---------------- Sensor reads ----------------

// internal ref is ~1.20V, use it to work out real supply voltage
static uint32_t read_vdda_mv(void)
{
    uint16_t vref_raw = adc_read(17);
    if (vref_raw == 0) return 3300;
    return (1200UL * 4095UL) / vref_raw;
}

// current sense pin, PA2 - raw voltage only, needs shunt/amp hardware
// to actually mean amps. left as-is (not converted to real current).
static uint32_t read_current_mv(uint32_t vdda_mv)
{
    uint16_t raw = adc_read(2);
    return (raw * vdda_mv) / 4095;
}

// internal temp sensor, formula from datasheet
static int32_t read_temp_x10(uint32_t vdda_mv)
{
    uint16_t raw = adc_read(16);
    uint32_t sensor_mv = (raw * vdda_mv) / 4095;
    return 250 + ((int32_t)(1430 - sensor_mv) * 100) / 43;
}

// ---------------- Number to string helpers ----------------

static void mv_to_str(uint32_t mv, char *out)
{
    uint32_t whole = mv / 1000;
    uint32_t frac = mv % 1000;
    out[0] = '0' + (whole % 10);
    out[1] = '.';
    out[2] = '0' + (frac / 100);
    out[3] = '0' + (frac / 10) % 10;
    out[4] = '0' + (frac % 10);
    out[5] = '\0';
}

static void temp_to_str(int32_t t, char *out)
{
    uint8_t i = 0;
    if (t < 0) { out[i++] = '-'; t = -t; }
    uint32_t whole = t / 10;
    uint32_t frac = t % 10;
    if (whole >= 10) out[i++] = '0' + (whole / 10) % 10;
    out[i++] = '0' + (whole % 10);
    out[i++] = '.';
    out[i++] = '0' + frac;
    out[i] = '\0';
}

// ---------------- Main ----------------

int main(void)
{
    clock_init();
    systick_init();
    gpio_init();
    adc_init();
    oled_init();

    oled_clear();
    oled_set_cursor(0, 0);
    oled_print("STM32 SENSITIVITY");
    oled_set_cursor(2, 0);
    oled_print("VOLT:");
    oled_set_cursor(4, 0);
    oled_print("CURR:");
    oled_set_cursor(6, 0);
    oled_print("TEMP:");

    uint32_t last_update = 0;
    char buf[12];

    while (1)
    {
        led_blink();

        if (millis() - last_update >= 250)
        {
            last_update = millis();

            uint32_t vdda_mv = read_vdda_mv();
            uint32_t curr_mv = read_current_mv(vdda_mv);
            int32_t  temp_x10 = read_temp_x10(vdda_mv);

            mv_to_str(vdda_mv, buf);
            oled_print_at(2, 36, buf);

            mv_to_str(curr_mv, buf);
            oled_print_at(4, 36, buf);

            temp_to_str(temp_x10, buf);
            oled_print_at(6, 36, buf);
        }
    }
}
