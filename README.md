STM32 Blue Pill Bare-Metal Telemetry

Bare-metal telemetry firmware for the STM32F103C8T6 Blue Pill, written in C using direct memory-mapped register access without STM32 HAL or CMSIS peripheral drivers.

The project reads internal MCU measurements and an external analog input, then displays the values on an SSD1306 OLED.

FEATURES

Direct register-level STM32 programming
72 MHz system clock configuration
SysTick 1 ms timebase
ADC1 configuration and calibration
Internal VREFINT measurement for VDDA estimation
Internal temperature sensor reading
PA2 analog voltage measurement
Software bit-banged I2C
SSD1306 OLED display
PC13 LED heartbeat
Periodic telemetry update

SYSTEM ARCHITECTURE

STM32F103C8T6
|
|-- ADC1
| |-- VREFINT
| |-- Temperature Sensor
| -- PA2 Analog Input | |-- Software I2C | -- SSD1306 OLED
|
-- PC13 -- Heartbeat LED

PIN MAP

PA2 - External analog / current-sense input
PB6 - I2C SCL
PB7 - I2C SDA
PC13 - Onboard LED

TELEMETRY

VDDA

The internal VREFINT reference is used to estimate the MCU supply voltage.

VDDA ≈ (1.20 V × 4095) / VREFINT_ADC

Temperature

The STM32 internal temperature sensor is read through ADC channel 16 and converted to an approximate temperature value.

PA2 Input

PA2 is read through ADC channel 2 and converted to voltage:

Voltage = ADC × VDDA / 4095

The firmware currently displays this as a raw voltage. A real current value requires external shunt/amplifier hardware and calibration.

OLED DISPLAY

An SSD1306 OLED is controlled using software-generated I2C signals.

PB6 → SCL
PB7 → SDA

OLED address: 0x3C

The display shows:

VOLT
CURR
TEMP

TIMING

SysTick provides a 1 ms system timebase.

Telemetry is refreshed every 250 ms.

The PC13 heartbeat LED toggles every 1 second.

FIRMWARE FLOW

Reset
↓
Clock Initialization
↓
SysTick Initialization
↓
GPIO Initialization
↓
ADC Initialization
↓
OLED Initialization
↓
Main Loop
├── Heartbeat LED
└── Read Sensors
├── VREFINT
├── PA2
└── Temperature
↓
Update OLED

PROJECT STRUCTURE

stm32-bluepill-baremetal-telemetry/

Src/

main.c
syscalls.c
sysmem.c

Startup/

.settings/

.cproject

.project

STM32F103C8TX_FLASH.ld

.gitignore

README.md

LICENSE

EMBEDDED CONCEPTS

This project demonstrates:

Memory-mapped registers
volatile
Bit manipulation
RCC and clock configuration
GPIO configuration
ADC configuration and calibration
SysTick interrupt
Software I2C
OLED communication
Embedded data conversion
Bare-metal firmware structure

LIMITATIONS

PA2 is currently a voltage measurement, not a calibrated current measurement.
External shunt/amplifier hardware is required for real current measurement.
Internal temperature is an approximate MCU temperature indication.
No protection thresholds are implemented.
Software I2C is used instead of the STM32 hardware I2C peripheral.

STATUS

Working on the STM32F103C8T6 Blue Pill:

ADC measurements
Internal temperature reading
VDDA estimation
OLED display
PC13 heartbeat LED

DEVELOPMENT ENVIRONMENT

STM32CubeIDE
ARM GCC
C
STM32F103C8T6
ARM Cortex-M3

LICENSE

MIT License
