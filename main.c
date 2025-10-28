#include <stdlib.h>
#include <stdio.h>
#include "hardware/gpio.h"
#include "bsp/board_api.h"
#include "tusb.h"
#include "usb_descriptors.h"

// Button pin mapping
#define BUTTON_A_PIN       0
#define BUTTON_B_PIN       1
#define BUTTON_X_PIN       2
#define BUTTON_Y_PIN       3
#define DPAD_UP_PIN        4
#define DPAD_DOWN_PIN      5
#define DPAD_LEFT_PIN      6
#define DPAD_RIGHT_PIN     7
#define BUTTON_LB_PIN      8
#define BUTTON_RB_PIN      9
#define BUTTON_SELECT_PIN  10
#define BUTTON_START_PIN   11

uint8_t button_pins[12] = {
    BUTTON_A_PIN, BUTTON_B_PIN, BUTTON_X_PIN, BUTTON_Y_PIN,
    DPAD_UP_PIN, DPAD_DOWN_PIN, DPAD_LEFT_PIN, DPAD_RIGHT_PIN,
    BUTTON_LB_PIN, BUTTON_RB_PIN, BUTTON_SELECT_PIN, BUTTON_START_PIN
};

// Blink intervals
enum {
    BLINK_NOT_MOUNTED = 250,
    BLINK_MOUNTED     = 1000,
    BLINK_SUSPENDED   = 2500
};

static uint32_t blink_interval_ms = BLINK_NOT_MOUNTED;

void led_blinking_task(void);
void hid_task(void);

int main(void)
{
    board_init();
    tud_init(BOARD_TUD_RHPORT);

    // Initialize buttons
    for (int i = 0; i < 12; i++) {
        gpio_init(button_pins[i]);
        gpio_set_dir(button_pins[i], GPIO_IN);
        gpio_pull_up(button_pins[i]);
    }

    if (board_init_after_tusb) {
        board_init_after_tusb();
    }

    while (1) {
        tud_task();
        led_blinking_task();
        hid_task();
    }
}

// Device callbacks
void tud_mount_cb(void) { blink_interval_ms = BLINK_MOUNTED; }
void tud_umount_cb(void) { blink_interval_ms = BLINK_NOT_MOUNTED; }
void tud_suspend_cb(bool remote_wakeup_en) { (void)remote_wakeup_en; blink_interval_ms = BLINK_SUSPENDED; }
void tud_resume_cb(void) { blink_interval_ms = tud_mounted() ? BLINK_MOUNTED : BLINK_NOT_MOUNTED; }

// Send HID report
static void send_hid_report(uint32_t btn_mask)
{
    if (!tud_hid_ready()) return;

    hid_gamepad_report_t report = {0};

    // D-Pad
    bool up    = btn_mask & (1 << DPAD_UP_PIN);
    bool down  = btn_mask & (1 << DPAD_DOWN_PIN);
    bool left  = btn_mask & (1 << DPAD_LEFT_PIN);
    bool right = btn_mask & (1 << DPAD_RIGHT_PIN);

    if (up && right)       report.hat = GAMEPAD_HAT_UP_RIGHT;
    else if (up && left)   report.hat = GAMEPAD_HAT_UP_LEFT;
    else if (down && right) report.hat = GAMEPAD_HAT_DOWN_RIGHT;
    else if (down && left)  report.hat = GAMEPAD_HAT_DOWN_LEFT;
    else if (up)            report.hat = GAMEPAD_HAT_UP;
    else if (down)          report.hat = GAMEPAD_HAT_DOWN;
    else if (left)          report.hat = GAMEPAD_HAT_LEFT;
    else if (right)         report.hat = GAMEPAD_HAT_RIGHT;
    else                    report.hat = GAMEPAD_HAT_CENTERED;

    // Buttons
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

// HID task
void hid_task(void)
{
    static uint32_t last_ms = 0;
    const uint32_t interval_ms = 10;
    if (board_millis() - last_ms < interval_ms) return;
    last_ms += interval_ms;

    uint32_t btn_mask = 0;
    for (int i = 0; i < 12; i++) {
        if (!gpio_get(button_pins[i])) btn_mask |= (1 << i); // Active low
    }

    if (tud_suspended() && btn_mask) tud_remote_wakeup();

    send_hid_report(btn_mask);
}

// Report complete callback
void tud_hid_report_complete_cb(uint8_t instance, uint8_t const* report, uint16_t len)
{
    (void)instance;
    (void)report;
    (void)len;
    // No need to resend, hid_task handles polling
}

// LED blinking
void led_blinking_task(void)
{
    static uint32_t last_ms = 0;
    static bool led_state = false;
    if (!blink_interval_ms) return;
    if (board_millis() - last_ms < blink_interval_ms) return;
    last_ms += blink_interval_ms;
    board_led_write(led_state);
    led_state = !led_state;
}
