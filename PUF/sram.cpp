
/*
* Created by Abel Gomez 03/15/2020
* Adapted from Ade Setyawan code, available at: 
* https://github.com/Tribler/software-based-PUF
*/
#include "sram.h"

#define WRSR  0b00000001 //1
#define WRITE 0b00000010 //2
#define READ  0b00000011 //3
#define RDSR  0b00000101 //5
#define PAGE_MODE  0b10000000

#define SI_PIN RPI_GPIO_P1_19
#define SO_PIN RPI_GPIO_P1_21
#define CK_PIN RPI_GPIO_P1_23
#define CS_PIN RPI_GPIO_P1_16
#define PW_PIN RPI_GPIO_P1_22
#define HL_PIN RPI_GPIO_P1_18


SRAM::SRAM() {
    
}

uint32_t SRAM::get_max_ram() {
    return maxram;
}

uint32_t SRAM::get_max_page() {
    return maxpage;
}

void SRAM::turn_off() {
    bcm2835_spi_setChipSelectPolarity(BCM2835_SPI_CS0, HIGH);
    bcm2835_spi_end();
    
    bcm2835_gpio_fsel(CK_PIN, BCM2835_GPIO_FSEL_OUTP);
    bcm2835_gpio_fsel(SI_PIN, BCM2835_GPIO_FSEL_OUTP);
    bcm2835_gpio_fsel(SO_PIN, BCM2835_GPIO_FSEL_OUTP);
    bcm2835_gpio_fsel(CS_PIN, BCM2835_GPIO_FSEL_OUTP);

    bcm2835_gpio_write(PW_PIN, LOW);
    bcm2835_gpio_write(SI_PIN, LOW);
    bcm2835_gpio_write(SO_PIN, LOW);
    bcm2835_gpio_write(CK_PIN, LOW);
    bcm2835_gpio_write(CS_PIN, LOW);
    bcm2835_gpio_write(HL_PIN, LOW);

    bcm2835_close();
}

void SRAM::turn_on() {
    if (!bcm2835_init()) {
        printf("bcm2835_init failed. Are you running as root??\n");
        exit(1);
    }
    if (!bcm2835_spi_begin()) {
        printf("bcm2835_spi_begin failed. Are you running as root??\n");
        exit(1);
    }

    bcm2835_spi_setBitOrder(BCM2835_SPI_BIT_ORDER_MSBFIRST);
    bcm2835_spi_setDataMode(BCM2835_SPI_MODE0);
    bcm2835_spi_setClockDivider(BCM2835_SPI_CLOCK_DIVIDER_32);
    // bcm2835_spi_set_speed_hz(16000000);
    bcm2835_spi_chipSelect(BCM2835_SPI_CS_NONE);
    // bcm2835_spi_setChipSelectPolarity(BCM2835_SPI_CS0, HIGH);

    bcm2835_gpio_fsel(CS_PIN, BCM2835_GPIO_FSEL_OUTP);
    bcm2835_gpio_fsel(PW_PIN, BCM2835_GPIO_FSEL_OUTP);
    bcm2835_gpio_fsel(HL_PIN, BCM2835_GPIO_FSEL_OUTP);

    bcm2835_gpio_write(CS_PIN, HIGH);
    bcm2835_gpio_write(PW_PIN, HIGH);
    bcm2835_gpio_write(HL_PIN, HIGH);
}

void SRAM::write8(uint32_t address, uint8_t send_byte) {
    bcm2835_gpio_write(CS_PIN, LOW);
    bcm2835_spi_transfer(WRITE);
	bcm2835_spi_transfer((uint8_t) (address >> 16));
	bcm2835_spi_transfer((uint8_t) (address >> 8));
	bcm2835_spi_transfer((uint8_t) (address));
    bcm2835_spi_transfer(send_byte);
    bcm2835_gpio_write(CS_PIN, HIGH);
 }

uint8_t SRAM::read8(uint32_t address) {
    uint8_t recv_byte;

    bcm2835_gpio_write(CS_PIN, LOW);
    bcm2835_spi_transfer(READ);
	bcm2835_spi_transfer((uint8_t) (address >> 16));
	bcm2835_spi_transfer((uint8_t) (address >> 8) &0xFF);
	bcm2835_spi_transfer((uint8_t) (address));
    recv_byte = bcm2835_spi_transfer(0x00);
    bcm2835_gpio_write(CS_PIN, HIGH);

    return recv_byte;
}

void SRAM::write_byte(long address, uint8_t data) {
    write8(address, data);
}

uint8_t SRAM::read_byte(long location) {
    return read8(location);
}

void SRAM::write32(uint32_t address, uint8_t* buffer) {
    uint32_t i;

    // bcm2835_gpio_write(CS_PIN, LOW);
    // bcm2835_spi_transfer(WRSR);
    // bcm2835_spi_transfer(PAGE_MODE);
    // bcm2835_gpio_write(CS_PIN, HIGH);
    // bcm2835_spi_transfer(RDSR);
    bcm2835_gpio_write(CS_PIN, LOW);
    // bcm2835_spi_setChipSelectPolarity(BCM2835_SPI_CS0, LOW);
    bcm2835_spi_transfer(WRITE);
	bcm2835_spi_transfer((uint8_t) (address >> 16));
	bcm2835_spi_transfer((uint8_t) (address >> 8));
	bcm2835_spi_transfer((uint8_t) (address));
    for (i = 0; i < 32; i++) {
        bcm2835_spi_transfer(buffer[i]);
    }
    bcm2835_gpio_write(CS_PIN, HIGH);
    // bcm2835_spi_setChipSelectPolarity(BCM2835_SPI_CS0, HIGH);
}

void SRAM::read32(uint32_t address, uint8_t* buffer) {
    uint32_t i;
    uint8_t read_page;
    
    // bcm2835_gpio_write(CS_PIN, LOW);
    // bcm2835_spi_transfer(WRSR);
    // bcm2835_spi_transfer(PAGE_MODE);
    // bcm2835_gpio_write(CS_PIN, HIGH);
    // bcm2835_spi_transfer(RDSR);
    bcm2835_gpio_write(CS_PIN, LOW);
    // bcm2835_spi_setChipSelectPolarity(BCM2835_SPI_CS0, LOW);
    bcm2835_spi_transfer(READ);
	bcm2835_spi_transfer((uint8_t) (address >> 16));
	bcm2835_spi_transfer((uint8_t) (address >> 8));
	bcm2835_spi_transfer((uint8_t) (address));
    
    for (i = address*32; i < 32; i++) {
        read_page = bcm2835_spi_transfer(0x00);
        buffer[i] = read_page;
    }
    bcm2835_gpio_write(CS_PIN, HIGH);
    // bcm2835_spi_setChipSelectPolarity(BCM2835_SPI_CS0, HIGH);
}

void SRAM::write_page(long page, bool is_one) {
    uint8_t bufferFF[32] = {
        0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
        0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
        0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
        0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
    };
    uint8_t buffer00[32] = {
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
    };
    write32(page, is_one ? bufferFF : buffer00);
}

void SRAM::read_page(uint32_t page, uint8_t* result) {
    read32(page, result);
}

void SRAM::write_all_zero() { 
    uint8_t buffer00[32] = {
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    };
    for (int i = 0; i < maxpage; i++) {
        write32(i, buffer00);
    }
}

void SRAM::write_all_one() { 
    uint8_t bufferFF[32] = {
        0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
        0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
        0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
        0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
    };
    
    for (int i = 0; i < maxpage; i++) {
        write32(i, bufferFF);
    }
}









// void
// SRAM::set_pin_cs(uint8_t pin){
//   pin_cs = pin;
// }

// void
// SRAM::set_pin_hold(uint8_t pin){
//   pin_hold = pin;
// }

// void
// SRAM::set_pin_power(uint8_t pin){
//   pin_power = pin;
// }

// void
// SRAM::set_pin_mosi(uint8_t pin){
//   pin_mosi = pin;
// }

// void
// SRAM::set_pin_miso(uint8_t pin){
//   pin_miso = pin;
// }

// void
// SRAM::set_pin_sck(uint8_t pin){
//   pin_sck = pin;
// }
















