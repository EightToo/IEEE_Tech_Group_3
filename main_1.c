/*
 * The MIT License (MIT)
 *
 * Copyright (c) 2019 Ha Thach (tinyusb.org)
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 *
 */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "hardware/gpio.h"
#include "bsp/board_api.h"
#include "tusb.h"

#include "usb_descriptors.h"

// Buttons and the relating pins
#define BUTTON_A_PIN 0
#define BUTTON_B_PIN 1
#define BUTTON_X_PIN 2
#define BUTTON_Y_PIN 3
#define DPAD_UP_PIN 4
#define DPAD_DOWN_PIN 5
#define DPAD_LEFT_PIN 6
#define DPAD_RIGHT_PIN 7
#define BUTTON_LB_PIN 8
#define BUTTON_RB_PIN 9
#define BUTTON_SELECT_PIN 10
#define BUTTON_START_PIN 11

// Array for easy reading
uint8_t button_pins[12] = {
    BUTTON_A_PIN, BUTTON_B_PIN, BUTTON_X_PIN, BUTTON_Y_PIN,
    DPAD_UP_PIN, DPAD_DOWN_PIN, DPAD_LEFT_PIN, DPAD_RIGHT_PIN,
    BUTTON_LB_PIN, BUTTON_RB_PIN, BUTTON_SELECT_PIN, BUTTON_START_PIN };

//--------------------------------------------------------------------+
// MACRO CONSTANT TYPEDEF PROTYPES
//--------------------------------------------------------------------+

/* Blink pattern
 * - 250 ms  : device not mounted
 * - 1000 ms : device mounted
 * - 2500 ms : device is suspended
 */
enum  {
  BLINK_NOT_MOUNTED = 250,
  BLINK_MOUNTED = 1000,
  BLINK_SUSPENDED = 2500,
};

static uint32_t blink_interval_ms = BLINK_NOT_MOUNTED;

void led_blinking_task(void);
void hid_task(void);

/*------------- MAIN -------------*/
int main(void)
{
  board_init();

  // init device stack on configured roothub port
  tud_init(BOARD_TUD_RHPORT);

  // initialize all buttons
    for(int i=0; i<12; i++){
      gpio_init(button_pins[i]);
      gpio_set_dir(button_pins[i], GPIO_IN);
      gpio_pull_up(button_pins[i]); // or pull_up depending on wiring
}

  if (board_init_after_tusb) {
    board_init_after_tusb();
  }

  while (1)
  {
    tud_task(); // tinyusb device task
    led_blinking_task();

    hid_task();
  }
}

//--------------------------------------------------------------------+
// Device callbacks
//--------------------------------------------------------------------+

// Invoked when device is mounted
void tud_mount_cb(void)
{
  blink_interval_ms = BLINK_MOUNTED;
}

// Invoked when device is unmounted
void tud_umount_cb(void)
{
  blink_interval_ms = BLINK_NOT_MOUNTED;
}

// Invoked when usb bus is suspended
// remote_wakeup_en : if host allow us  to perform remote wakeup
// Within 7ms, device must draw an average of current less than 2.5 mA from bus
void tud_suspend_cb(bool remote_wakeup_en)
{
  (void) remote_wakeup_en;
  blink_interval_ms = BLINK_SUSPENDED;
}

// Invoked when usb bus is resumed
void tud_resume_cb(void)
{
  blink_interval_ms = tud_mounted() ? BLINK_MOUNTED : BLINK_NOT_MOUNTED;
}

//--------------------------------------------------------------------+
// USB HID
//--------------------------------------------------------------------+

static void send_hid_report(uint8_t report_id, uint32_t btn_mask)
{
    if (!tud_hid_ready()) return;

    hid_gamepad_report_t report = {0};

    // D-Pad
    if (btn_mask & (1 << DPAD_UP_PIN)) report.hat = GAMEPAD_HAT_UP;
    else if (btn_mask & (1 << DPAD_DOWN_PIN)) report.hat = GAMEPAD_HAT_DOWN;
    else if (btn_mask & (1 << DPAD_LEFT_PIN)) report.hat = GAMEPAD_HAT_LEFT;
    else if (btn_mask & (1 << DPAD_RIGHT_PIN)) report.hat = GAMEPAD_HAT_RIGHT;
    else report.hat = GAMEPAD_HAT_CENTERED;

    // Buttons (raw bits)
    report.buttons = 0;
    if (btn_mask & (1 << BUTTON_A_PIN)) report.buttons |= (1 << 0);
    if (btn_mask & (1 << BUTTON_B_PIN)) report.buttons |= (1 << 1);
    if (btn_mask & (1 << BUTTON_X_PIN)) report.buttons |= (1 << 2);
    if (btn_mask & (1 << BUTTON_Y_PIN)) report.buttons |= (1 << 3);
    if (btn_mask & (1 << BUTTON_LB_PIN)) report.buttons |= (1 << 4);
    if (btn_mask & (1 << BUTTON_RB_PIN)) report.buttons |= (1 << 5);
    if (btn_mask & (1 << BUTTON_SELECT_PIN)) report.buttons |= (1 << 6);
    if (btn_mask & (1 << BUTTON_START_PIN)) report.buttons |= (1 << 7);

    tud_hid_report(REPORT_ID_GAMEPAD, &report, sizeof(report));
}

// Every 10ms, we will sent 1 report for each HID profile (keyboard, mouse etc ..)
// tud_hid_report_complete_cb() is used to send the next report after previous one is complete
void hid_task(void)
{
    const uint32_t interval_ms = 10;
    static uint32_t start_ms = 0;

    if (board_millis() - start_ms < interval_ms) return;
    start_ms += interval_ms;

    uint32_t btn_mask = 0;
    for (int i = 0; i < 12; i++)
    {
        if (!gpio_get(button_pins[i])) btn_mask |= (1 << i);
    }

    // Remote wakeup
    if (tud_suspended())
    {
        tud_remote_wakeup();
    }
    else
    {
        send_hid_report(REPORT_ID_GAMEPAD, btn_mask);
    }
}

// Invoked when sent REPORT successfully to host
// Application can use this to send the next report
// Note: For composite reports, report[0] is report ID
void tud_hid_report_complete_cb(uint8_t instance, uint8_t const* report, uint16_t len)
{
  (void) instance;
  (void) len;

  uint8_t next_report_id = report[0] + 1u;

  if (next_report_id < REPORT_ID_COUNT)
  {
    send_hid_report(next_report_id, board_button_read());
  }
}

// Invoked when received GET_REPORT control request
// Application must fill buffer report's content and return its length.
// Return zero will cause the stack to STALL request
uint16_t tud_hid_get_report_cb(uint8_t instance, uint8_t report_id, hid_report_type_t report_type, uint8_t* buffer, uint16_t reqlen)
{
  // TODO not Implemented
  (void) instance;
  (void) report_id;
  (void) report_type;
  (void) buffer;
  (void) reqlen;

  return 0;
}

// Invoked when received SET_REPORT control request or
// received data on OUT endpoint ( Report ID = 0, Type = 0 )
void tud_hid_set_report_cb(uint8_t instance, uint8_t report_id, hid_report_type_t report_type, uint8_t const* buffer, uint16_t bufsize)
{
  (void) instance;

  if (report_type == HID_REPORT_TYPE_OUTPUT)
  {
    // Set keyboard LED e.g Capslock, Numlock etc...
    if (report_id == REPORT_ID_KEYBOARD)
    {
      // bufsize should be (at least) 1
      if ( bufsize < 1 ) return;

      uint8_t const kbd_leds = buffer[0];

      if (kbd_leds & KEYBOARD_LED_CAPSLOCK)
      {
        // Capslock On: disable blink, turn led on
        blink_interval_ms = 0;
        board_led_write(true);
      }else
      {
        // Caplocks Off: back to normal blink
        board_led_write(false);
        blink_interval_ms = BLINK_MOUNTED;
      }
    }
  }
}

//--------------------------------------------------------------------+
// BLINKING TASK
//--------------------------------------------------------------------+
void led_blinking_task(void)
{
  static uint32_t start_ms = 0;
  static bool led_state = false;

  // blink is disabled
  if (!blink_interval_ms) return;

  // Blink every interval ms
  if ( board_millis() - start_ms < blink_interval_ms) return; // not enough time
  start_ms += blink_interval_ms;

  board_led_write(led_state);
  led_state = 1 - led_state; // toggle
}
