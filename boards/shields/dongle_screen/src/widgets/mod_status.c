
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zmk/hid.h>
#include <zmk/event_manager.h>
#include <zmk/events/hid_indicators_changed.h>
#include <lvgl.h>
#include <stdio.h>
#include "mod_status.h"
#include <fonts.h> // NerdFont

LOG_MODULE_DECLARE(zmk, CONFIG_ZMK_LOG_LEVEL);

#define LED_NLCK 0x01
#define LED_CLCK 0x02
#define LED_SLCK 0x04

// Spacing between modifier icons (pixels)
#define MOD_ICON_SPACING 5

static bool caps_lock_on = false;

static int mod_status_hid_indicators_listener(const zmk_event_t *eh)
{
    const struct zmk_hid_indicators_changed *ev =
        as_zmk_hid_indicators_changed(eh);

    if (!ev) {
        return 0;
    }

    caps_lock_on = (ev->indicators & LED_CLCK) != 0;
    return 0;
}

/* Register listener + subscription */
ZMK_LISTENER(widget_mod_status_hid_indicators,
             mod_status_hid_indicators_listener);
ZMK_SUBSCRIPTION(widget_mod_status_hid_indicators,
                 zmk_hid_indicators_changed);


static void update_mod_status(struct zmk_widget_mod_status *widget)
{
    uint8_t mods = zmk_hid_get_keyboard_report()->body.modifiers;
    char text[32] = "";
    int idx = 0;

    const char *syms[5];
    int n = 0;

    // CTRL
    if (mods & (MOD_LCTL | MOD_RCTL)) {
        syms[n++] = "󰘴";
    }

    // SHIFT
    if (mods & (MOD_LSFT | MOD_RSFT)) {
        syms[n++] = "󰘶"; // U+F0636
    }

    // ALT
    if (mods & (MOD_LALT | MOD_RALT)) {
        syms[n++] = "󰘵"; // U+F0635
    }

    // GUI / SYSTEM
    if (mods & (MOD_LGUI | MOD_RGUI)) {
        // Select icon based on system configuration
#if CONFIG_DONGLE_SCREEN_SYSTEM_ICON == 1
        syms[n++] = "󰌽";
#elif CONFIG_DONGLE_SCREEN_SYSTEM_ICON == 2
        syms[n++] = ""; // U+E62A
#else
        syms[n++] = "󰘳"; // U+F0633
#endif
    }

    // CAPS LOCK
    if (caps_lock_on) {
        syms[n++] = "󰘲"; // U+F0632
    }

    // Combine modifier icons without space characters
    for (int i = 0; i < n; ++i) {
        idx += snprintf(
            &text[idx],
            sizeof(text) - idx,
            "%s",
            syms[i]
        );
    }

    // Update the label
    lv_label_set_text(widget->label, idx ? text : "");

    // Re-center whenever the modifier combination changes
    lv_obj_align(
        widget->label,
        LV_ALIGN_CENTER,
        0,
        0
    );
}


static void mod_status_timer_cb(struct k_timer *timer)
{
    struct zmk_widget_mod_status *widget =
        k_timer_user_data_get(timer);

    update_mod_status(widget);
}

static struct k_timer mod_status_timer;


int zmk_widget_mod_status_init(
    struct zmk_widget_mod_status *widget,
    lv_obj_t *parent)
{
    // Create widget container
    widget->obj = lv_obj_create(parent);
    lv_obj_set_size(widget->obj, 180, 40);

    // Create modifier label
    widget->label = lv_label_create(widget->obj);

    // Set Nerd Font
    lv_obj_set_style_text_font(
        widget->label,
        &NerdFonts_Regular_40,
        LV_PART_MAIN
    );

    // Add spacing between icons without using spaces
    lv_obj_set_style_text_letter_space(
        widget->label,
        MOD_ICON_SPACING,
        LV_PART_MAIN
    );

    // Start with an empty label
    lv_label_set_text(widget->label, "");

    // Center the label in the widget
    lv_obj_align(
        widget->label,
        LV_ALIGN_CENTER,
        0,
        0
    );

    // Initialize update timer
    k_timer_init(
        &mod_status_timer,
        mod_status_timer_cb,
        NULL
    );

    k_timer_user_data_set(
        &mod_status_timer,
        widget
    );

    k_timer_start(
        &mod_status_timer,
        K_MSEC(100),
        K_MSEC(100)
    );

    return 0;
}


lv_obj_t *zmk_widget_mod_status_obj(
    struct zmk_widget_mod_status *widget)
{
    return widget->obj;
}
