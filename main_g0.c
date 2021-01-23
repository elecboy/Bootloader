/*
 * STM32G0 board support for the bootloader.
 *
 */
#include "hw_config.h"

#include <stdlib.h>
#include <libopencm3/stm32/rcc.h>
#include <libopencm3/stm32/gpio.h>
#include <libopencm3/stm32/flash.h>
#include <libopencm3/stm32/usart.h>
#include <libopencm3/stm32/pwr.h>
#include <libopencm3/cm3/systick.h>

#include "bl.h"

#include "bl_version.h"

#define UDID_START      0x1FFF7590UL

// address of MCU IDCODE
#define DBGMCU_IDCODE		0x40015800

volatile uint32_t IAPParam[4] __attribute__((section(".ParamArea")));

#ifdef INTERFACE_USART
# define BOARD_INTERFACE_CONFIG		(void *)BOARD_USART
#else
# define BOARD_INTERFACE_CONFIG		NULL
#endif

/* board definition */
struct boardinfo board_info = {
	.board_type	= BOARD_TYPE,
	.board_rev	= 1,
	.fw_size	= APP_SIZE_MAX,

	.systick_mhz	= OSC_FREQ,
};

static void board_init(void);

static void
board_init(void)
{
    rcc_periph_clock_enable(RCC_GPIOA);
	gpio_mode_setup(GPIOA, GPIO_MODE_OUTPUT,
		      GPIO_OTYPE_OD, GPIO4);
	gpio_clear(GPIOA, GPIO4);

	/* initialise LEDs */
    rcc_periph_clock_enable(BOARD_CLOCK_LEDS);

	gpio_mode_setup(BOARD_PORT_LEDS, GPIO_MODE_OUTPUT,
		      GPIO_OTYPE_OD, BOARD_PIN_LED_BOOTLOADER | BOARD_PIN_LED_ACTIVITY);
	BOARD_LED_ON(
		BOARD_PORT_LEDS,
		BOARD_PIN_LED_BOOTLOADER | BOARD_PIN_LED_ACTIVITY);

	/* if we have one, enable the force-bootloader pin */
#ifdef BOARD_FORCE_BL_PIN
	rcc_periph_clock_enable(BOARD_FORCE_BL_CLOCK);

	gpio_set(BOARD_FORCE_BL_PORT, BOARD_FORCE_BL_PIN);
	gpio_mode_setup(BOARD_FORCE_BL_PORT, GPIO_MODE_INPUT,
		      BOARD_FORCE_BL_PULL, BOARD_FORCE_BL_PIN);
#endif

	/* enable the backup registers */
//	rcc_peripheral_enable_clock(&RCC_APB1ENR, RCC_APB1ENR_PWREN);

#ifdef INTERFACE_USART
	/* configure usart pins */
	rcc_periph_clock_enable(BOARD_USART_PIN_CLOCK);
	gpio_mode_setup(BOARD_PORT_USART, GPIO_MODE_AF, GPIO_PUPD_NONE, BOARD_PIN_TX | BOARD_PIN_RX);
	/* Setup USART1 TX pin as alternate function. */
	gpio_set_af(BOARD_PORT_USART, GPIO_AF1, BOARD_PIN_TX | BOARD_PIN_RX);

	/* configure USART clock */
	rcc_periph_clock_enable(BOARD_USART_CLOCK);
#endif
#ifdef INTERFACE_I2C
# error I2C GPIO config not handled yet
#endif
}

void
board_deinit(void)
{
	/* deinitialise LEDs */
	gpio_mode_setup(BOARD_PORT_LEDS, GPIO_MODE_INPUT,
		      GPIO_MODE_INPUT, BOARD_PIN_LED_BOOTLOADER | BOARD_PIN_LED_ACTIVITY);

	/* if we have one, disable the force-bootloader pin */
#ifdef BOARD_FORCE_BL_PIN
	gpio_mode_setup(BOARD_FORCE_BL_PORT, GPIO_MODE_INPUT,
		      GPIO_MODE_INPUT, BOARD_FORCE_BL_PIN);
	gpio_clear(BOARD_FORCE_BL_PORT, BOARD_FORCE_BL_PIN);
#endif

	/* disable the backup registers */
//	rcc_peripheral_disable_clock(&RCC_APB1ENR, RCC_APB1ENR_PWREN);

#if INTERFACE_USART
	/* configure usart pins */
	gpio_mode_setup(BOARD_PORT_USART, GPIO_MODE_INPUT,
		      GPIO_PUPD_NONE, BOARD_PIN_TX | BOARD_PIN_RX);

	/* disable USART peripheral clock */
	rcc_periph_clock_disable(BOARD_USART_CLOCK);
#endif
#ifdef INTERFACE_I2C
# error I2C GPIO config not handled yet
#endif

	/* reset the APB2 peripheral clocks */
	RCC_APBENR2 = 0x00000000; // XXX Magic reset number from STM32F0x reference manual
}

uint32_t
board_get_devices(void)
{
	return BOOT_DEVICES_SELECTION;
}

/**
  * @brief  Initializes the RCC clock configuration.
  *
  * @param  clock_setup : The clock configuration to set
  */
static inline void
clock_init(void)
{
#if INTERFACE_USB
	rcc_clock_setup_in_hsi_out_48mhz();
#else
	rcc_clock_setup(&rcc_clock_config[RCC_CLOCK_CONFIG_HSI_16MHZ]);
#endif
}

/**
  * @brief  Resets the RCC clock configuration to the default reset state.
  * @note   The default reset state of the clock configuration is given below:
  *            - HSI ON and used as system clock source
  *            - HSE, PLL and PLLI2S OFF
  *            - AHB, APB1 and APB2 prescaler set to 1.
  *            - CSS, MCO1 and MCO2 OFF
  *            - All interrupts disabled
  * @note   This function doesn't modify the configuration of the
  *            - Peripheral clocks
  *            - LSI, LSE and RTC clocks
  */
void
clock_deinit(void)
{
	/* Enable internal high-speed oscillator. */
	rcc_osc_on(RCC_HSI);
	rcc_wait_for_osc_ready(RCC_HSI);

	/* Reset the RCC_CFGR register */
	RCC_CFGR = 0x000000;

	/* Stop the HSE, CSS, PLL, PLLI2S, PLLSAI */
	rcc_osc_off(RCC_HSE);
	rcc_osc_off(RCC_PLL);
	rcc_css_disable();

	/* Reset the HSEBYP bit */
	rcc_osc_bypass_disable(RCC_HSE);

	/* Reset the CIR register */
	RCC_CIER = 0x000000;
}

uint32_t
flash_func_sector_size(unsigned sector)
{
	if (sector < BOARD_FLASH_SECTORS) {
		return FLASH_SECTOR_SIZE;
	}

	return 0;
}

void
flash_func_erase_sector(unsigned sector)
{
	if (sector < BOARD_FLASH_SECTORS) {
		flash_erase_page(APP_START_PAGE+sector);
	}
}

void
flash_func_write_double_word(uint32_t address, uint64_t dword)
{
	flash_program_double_word(address + APP_LOAD_ADDRESS, dword);
}

uint32_t
flash_func_read_word(uint32_t address)
{
	return *(uint32_t *)(address + APP_LOAD_ADDRESS);
}

uint64_t
flash_func_read_double_word(uint32_t address)
{
	return *(uint64_t *)(address + APP_LOAD_ADDRESS);
}

uint32_t
flash_func_read_otp(uint32_t address)
{
	return 0;
}

uint32_t get_mcu_id(void)
{
	return *(uint32_t *)DBGMCU_IDCODE;
}

// See F4 version for future enhancement for full decoding

int get_mcu_desc(int max, uint8_t *revstr)
{
	const char none[] = "STM32G031G8U6";
	int i;

	for (i = 0; none[i] && i < max - 1; i++) {
		revstr[i] = none[i];
	}

	return i;
}

int check_silicon(void)
{
	return 0;
}

uint32_t
flash_func_read_sn(uint32_t address)
{
	// read a byte out from unique chip ID area
	// it's 12 bytes, or 3 words.
	return *(uint32_t *)(address + UDID_START);
}

void
led_on(unsigned led)
{
	switch (led) {
	case LED_ACTIVITY:
		BOARD_LED_ON(BOARD_PORT_LEDS, BOARD_PIN_LED_ACTIVITY);
		break;

	case LED_BOOTLOADER:
		BOARD_LED_ON(BOARD_PORT_LEDS, BOARD_PIN_LED_BOOTLOADER);
		break;
	}
}

void
led_off(unsigned led)
{
	switch (led) {
	case LED_ACTIVITY:
		BOARD_LED_OFF(BOARD_PORT_LEDS, BOARD_PIN_LED_ACTIVITY);
		break;

	case LED_BOOTLOADER:
		BOARD_LED_OFF(BOARD_PORT_LEDS, BOARD_PIN_LED_BOOTLOADER);
		break;
	}
}

void
led_toggle(unsigned led)
{
	switch (led) {
	case LED_ACTIVITY:
		gpio_toggle(BOARD_PORT_LEDS, BOARD_PIN_LED_ACTIVITY);
		break;

	case LED_BOOTLOADER:
		gpio_toggle(BOARD_PORT_LEDS, BOARD_PIN_LED_BOOTLOADER);
		break;
	}
}

static bool
should_wait(void)
{
	bool result = false;
	rcc_periph_clock_enable(RCC_PWR);
	pwr_disable_backup_domain_write_protect();

	if (IAPParam[1] == BL_WAIT_MAGIC) {
		result = true;
	}
	IAPParam[0] = BL_VERSION_CODE;
    pwr_enable_backup_domain_write_protect();
    rcc_periph_clock_disable(RCC_PWR);

	return result;
}

int
main(void)
{
	unsigned timeout = 0;

	/* do board-specific initialisation */
	board_init();

#if defined(INTERFACE_USART) || defined (INTERFACE_USB)
	/* XXX sniff for a USART connection to decide whether to wait in the bootloader? */
	timeout = BOOTLOADER_DELAY;
    //timeout = 0;
#endif

#ifdef INTERFACE_I2C
# error I2C bootloader detection logic not implemented
#endif

	/* if the app left a cookie saying we should wait, then wait */
	if (should_wait()) {
		timeout = BOOTLOADER_DELAY;
	}

#ifdef BOARD_FORCE_BL_PIN

	/* if the force-BL pin state matches the state of the pin, wait in the bootloader forever */
	if (BOARD_FORCE_BL_VALUE == gpio_get(BOARD_FORCE_BL_PORT, BOARD_FORCE_BL_PIN)) {
		timeout = 0xffffffff;
	}

#endif

	/* look for the magic wait-in-bootloader value in backup register zero */


	/* if we aren't expected to wait in the bootloader, try to boot immediately */
	if (timeout == 0) {
		/* try to boot immediately */
        clock_init();
        cinit(BOARD_INTERFACE_CONFIG, USART);
		jump_to_app();

		/* if we returned, there is no app; go to the bootloader and stay there */
		timeout = 0;
	}
	/* configure the clock for bootloader activity */
	clock_init();

	/* start the interface */
	cinit(BOARD_INTERFACE_CONFIG, USART);

	while (1) {
		/* run the bootloader, possibly coming back after the timeout */
		bootloader(timeout);

		/* look to see if we can boot the app */
		jump_to_app();

		/* boot failed; stay in the bootloader forever next time */
		timeout = 0;
	}
}