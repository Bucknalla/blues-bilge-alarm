/*
 * Copyright (c) 2023 Alex Bucknall
 *
 * MIT License. Use of this source code is governed by licenses granted
 * by the copyright holder including that found in the LICENSE file.
 *
 * SPDX-License-Identifier: MIT
 */

#include <zephyr/drivers/gpio.h>
#include <zephyr/device.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/sys/util.h>
#include <string.h>
#include <zephyr/usb/usb_device.h>
#include <zephyr/drivers/uart.h>

// Include Notecard note-c library
#include <note.h>

// Notecard node-c helper methods
#include <note_c_hooks.h>

// Uncomment the define below and replace com.your-company:your-product-name
// with your ProductUID.
#define PRODUCT_UID "com.gmail.alex.bucknall:bilge_alarm"

#ifndef PRODUCT_UID
#define PRODUCT_UID "com.your-company:your-product-name"
#pragma GCC error                                                                                  \
	"PRODUCT_UID is not defined in this example. Please ensure your Notecard has a product identifier set before running this example or define it in code here. More details at https://bit.ly/product-uid"
#endif

#define DEBOUNCE_TIMEOUT_MS 500

#define LED0_NODE   DT_ALIAS(led0)
#define ALARM0_NODE DT_NODELABEL(alarm0)

#if DT_NODE_HAS_STATUS(LED0_NODE, okay)
static const struct gpio_dt_spec led = GPIO_DT_SPEC_GET(LED0_NODE, gpios);
#else
#error "Unsupported board: led0 devicetree alias is not defined"
#endif

#if DT_NODE_HAS_STATUS(ALARM0_NODE, okay)
static const struct gpio_dt_spec alarm = GPIO_DT_SPEC_GET(ALARM0_NODE, gpios);
#else
#error "Unsupported board: alarm0 devicetree alias is not defined"
#endif

LOG_MODULE_REGISTER(main, CONFIG_APP_LOG_LEVEL);

uint64_t debounce = 0;
struct gpio_callback alarm_cb_data;

static k_tid_t _system_thread = 0;

int send_notecard_request(uint8_t state)
{
	LOG_INF("Sending notecard request...");
	J *req = NoteNewRequest("note.add");
	if (req) {
		// Set the sync flag to true to immediately send the request to Notehub
		JAddBoolToObject(req, "sync", true);

		J *body = JAddObjectToObject(req, "body");
		JAddIntToObject(body, "ALARM", state);

		if (!NoteRequest(req)) {
			LOG_ERR("Failed to submit Note to Notecard.");
			return -1;
		} else {
			return 0;
		}
	} else {
		LOG_ERR("Failed to allocate memory.");
		return -1;
	}
}

void alarm_callback(const struct device *dev, struct gpio_callback *cb, uint32_t data)
{
	uint64_t now = k_uptime_get();
	if ((now - debounce) > DEBOUNCE_TIMEOUT_MS) {
		k_wakeup(_system_thread);
	}
	debounce = now;
}

int main(void)
{
	int ret;

	LOG_INF("Initializing bilge alarm...");

	_system_thread = k_current_get();

	ret = gpio_is_ready_dt(&alarm);
	if (ret < 0) {
		LOG_ERR("Failed to init Alarm.");
		return ret;
	}

	ret = gpio_is_ready_dt(&led);
	if (ret < 0) {
		LOG_ERR("Failed to init LED.");
		return ret;
	}

	// initialize note-c hooks
	NoteSetUserAgent((char *)"note-zephyr");
	NoteSetFnDefault(malloc, free, platform_delay, platform_millis);
	NoteSetFnDebugOutput(note_log_print);
	NoteSetFnI2C(NOTE_I2C_ADDR_DEFAULT, NOTE_I2C_MAX_DEFAULT, note_i2c_reset, note_i2c_transmit,
		     note_i2c_receive);

	// send a Notecard hub.set using note-c
	J *req = NoteNewRequest("hub.set");
	if (req) {
		JAddStringToObject(req, "product", PRODUCT_UID);
		JAddStringToObject(req, "mode", "minimum");
		JAddStringToObject(req, "sn", "bilge-alarm");
		if (!NoteRequest(req)) {
			LOG_ERR("Failed to configure Notecard.");
			return -1;
		}
	} else {
		LOG_ERR("Failed to allocate memory.");
		return -1;
	}

	// configure the led as output
	ret = gpio_pin_configure_dt(&led, GPIO_OUTPUT);
	if (ret < 0) {
		LOG_ERR("Failed to configure LED.");
		return ret;
	}

	// configure the alarm pin as input
	ret = gpio_pin_configure_dt(&alarm, GPIO_INPUT);
	if (ret < 0) {
		LOG_ERR("Failed to configure alarm.");
		return ret;
	}

	// configure the interrupt on alarm press (pin goes from low to high)
	gpio_pin_interrupt_configure_dt(&alarm, GPIO_INT_EDGE_BOTH);

	// setup the alarm press callback
	gpio_init_callback(&alarm_cb_data, alarm_callback, BIT(alarm.pin));
	gpio_add_callback(alarm.port, &alarm_cb_data);

	LOG_INF("Waiting for alarm...");
	while (true) {
		// suspend thread until the alarm is triggered
		k_sleep(K_FOREVER);

		uint32_t state = gpio_pin_get_dt(&alarm);
		LOG_INF("Alarm state changed to %d!", state);

		ret = gpio_pin_set_dt(&led, state);
		if (ret < 0) {
			LOG_ERR("Failed to set LED state.");
		}

		ret = send_notecard_request(state);
		if (ret < 0) {
			LOG_ERR("Failed to send notecard request.");
		}
	}
}
