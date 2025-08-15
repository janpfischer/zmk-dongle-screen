/*
 * Copyright (c) 2024 The ZMK Contributors
 *
 * SPDX-License-Identifier: MIT
 */

#include <zephyr/kernel.h>
#include <zephyr/bluetooth/services/bas.h>

#include <zephyr/logging/log.h>
LOG_MODULE_DECLARE(zmk, CONFIG_ZMK_LOG_LEVEL);

#include <zmk/battery.h>
#include <zmk/split/central.h>
#include <zmk/display.h>
#include <zmk/events/battery_state_changed.h>
#include <zmk/events/usb_conn_state_changed.h>
#include <zmk/event_manager.h>
#include <zmk/usb.h>

#include "battery_status.h"
#include "../brightness.h"

#if IS_ENABLED(CONFIG_ZMK_DONGLE_DISPLAY_DONGLE_BATTERY)
    #define SOURCE_OFFSET 1
#else
    #define SOURCE_OFFSET 0
#endif

static sys_slist_t widgets = SYS_SLIST_STATIC_INIT(&widgets);

struct battery_state {
    uint8_t source;
    uint8_t level;
    bool usb_present;
};

struct battery_object {
    lv_obj_t *symbol;
    lv_obj_t *label;
} battery_objects[ZMK_SPLIT_CENTRAL_PERIPHERAL_COUNT + SOURCE_OFFSET];
    
static lv_color_t battery_image_buffer[ZMK_SPLIT_CENTRAL_PERIPHERAL_COUNT + SOURCE_OFFSET][102 * 5];

// Peripheral reconnection tracking
// ZMK sends battery events with level < 1 when peripherals disconnect
static int8_t last_battery_levels[ZMK_SPLIT_CENTRAL_PERIPHERAL_COUNT + SOURCE_OFFSET];

static void init_peripheral_tracking(void) {
    for (int i = 0; i < (ZMK_SPLIT_CENTRAL_PERIPHERAL_COUNT + SOURCE_OFFSET); i++) {
        last_battery_levels[i] = -1; // -1 indicates never seen before
    }
}

static bool is_peripheral_reconnecting(uint8_t source, uint8_t new_level) {
    if (source >= (ZMK_SPLIT_CENTRAL_PERIPHERAL_COUNT + SOURCE_OFFSET)) {
        return false;
    }
    
    int8_t previous_level = last_battery_levels[source];
    
    // Reconnection detected if:
    // 1. Previous level was < 1 (disconnected/unknown) AND
    // 2. New level is >= 1 (valid battery level)
    bool reconnecting = (previous_level < 1) && (new_level >= 1);
    
    if (reconnecting) {
        LOG_INF("Peripheral %d reconnection: %d%% -> %d%% (was %s)", 
                source, previous_level, new_level, 
                previous_level == -1 ? "never seen" : "disconnected");
    }
    
    return reconnecting;
}

static void draw_battery(lv_obj_t *canvas, uint8_t level, bool usb_present) {
    
    if (level < 1)
    {
        lv_canvas_fill_bg(canvas, lv_palette_main(LV_PALETTE_RED), LV_OPA_COVER);
    } else if (level <= 10) {
        lv_canvas_fill_bg(canvas, lv_palette_main(LV_PALETTE_YELLOW), LV_OPA_COVER);
    } else {
        lv_canvas_fill_bg(canvas, lv_color_white(), LV_OPA_COVER);
    }

    
    lv_draw_rect_dsc_t rect_fill_dsc;
    lv_draw_rect_dsc_init(&rect_fill_dsc);
    rect_fill_dsc.bg_color = lv_color_black();



    lv_canvas_set_px(canvas, 0, 0, lv_color_black());
    lv_canvas_set_px(canvas, 0, 4, lv_color_black());
    lv_canvas_set_px(canvas, 101, 0, lv_color_black());
    lv_canvas_set_px(canvas, 101, 4, lv_color_black());

    if (level <= 99 && level > 0)
    {
        lv_canvas_draw_rect(canvas, level, 1, 100 - level, 3, &rect_fill_dsc);
        lv_canvas_set_px(canvas, 100, 1, lv_color_black());
        lv_canvas_set_px(canvas, 100, 2, lv_color_black());
        lv_canvas_set_px(canvas, 100, 3, lv_color_black());
    }
    
}

static void set_battery_symbol(lv_obj_t *widget, struct battery_state state) {
    if (state.source >= ZMK_SPLIT_CENTRAL_PERIPHERAL_COUNT + SOURCE_OFFSET) {
        return;
    }
    
    // Check for reconnection using the existing battery level mechanism
    bool reconnecting = is_peripheral_reconnecting(state.source, state.level);
    
    // Update our tracking
    last_battery_levels[state.source] = state.level;


    // Wake screen on reconnection
    if (reconnecting) {
#if CONFIG_DONGLE_SCREEN_IDLE_TIMEOUT_S > 0    
        LOG_INF("Peripheral %d reconnected (battery: %d%%), requesting screen wake", 
                state.source, state.level);
        brightness_wake_screen_on_reconnect();
#else 
        LOG_INF("Peripheral %d reconnected (battery: %d%%)", 
                state.source, state.level);
#endif
    }


    LOG_DBG("source: %d, level: %d, usb: %d", state.source, state.level, state.usb_present);
    lv_obj_t *symbol = battery_objects[state.source].symbol;
    lv_obj_t *label = battery_objects[state.source].label;

    draw_battery(symbol, state.level, state.usb_present);
    
    if (state.level > 0) {
        lv_obj_set_style_text_color(label, lv_color_white(), 0);
        lv_label_set_text_fmt(label, "%4u", state.level);
    } else {
        lv_obj_set_style_text_color(label, lv_palette_main(LV_PALETTE_RED), 0);
        lv_label_set_text(label, "X");
    }

    if (state.level < 1)
    {
        lv_obj_set_style_text_color(label, lv_palette_main(LV_PALETTE_RED), 0);
        lv_label_set_text(label, "X");
    } else if (state.level <= 10) {
        lv_obj_set_style_text_color(label, lv_palette_main(LV_PALETTE_YELLOW), 0);
        lv_label_set_text_fmt(label, "%4u", state.level);
    } else {
        lv_obj_set_style_text_color(label, lv_color_white(), 0);
        lv_label_set_text_fmt(label, "%4u", state.level);
    }
    
    
    
    lv_obj_clear_flag(symbol, LV_OBJ_FLAG_HIDDEN);
    lv_obj_move_foreground(symbol);
    lv_obj_clear_flag(label, LV_OBJ_FLAG_HIDDEN);
    lv_obj_move_foreground(label);

}

void battery_status_update_cb(struct battery_state state) {
    struct zmk_widget_dongle_battery_status *widget;
    SYS_SLIST_FOR_EACH_CONTAINER(&widgets, widget, node) { set_battery_symbol(widget->obj, state); }
}

static struct battery_state peripheral_battery_status_get_state(const zmk_event_t *eh) {
    const struct zmk_peripheral_battery_state_changed *ev = as_zmk_peripheral_battery_state_changed(eh);
    return (struct battery_state){
        .source = ev->source + SOURCE_OFFSET,
        .level = ev->state_of_charge,
    };
}

static struct battery_state central_battery_status_get_state(const zmk_event_t *eh) {
    const struct zmk_battery_state_changed *ev = as_zmk_battery_state_changed(eh);
    return (struct battery_state) {
        .source = 0,
        .level = (ev != NULL) ? ev->state_of_charge : zmk_battery_state_of_charge(),
#if IS_ENABLED(CONFIG_USB_DEVICE_STACK)
        .usb_present = zmk_usb_is_powered(),
#endif /* IS_ENABLED(CONFIG_USB_DEVICE_STACK) */
    };
}

static struct battery_state battery_status_get_state(const zmk_event_t *eh) { 
    if (as_zmk_peripheral_battery_state_changed(eh) != NULL) {
        return peripheral_battery_status_get_state(eh);
    } else {
        return central_battery_status_get_state(eh);
    }
}

ZMK_DISPLAY_WIDGET_LISTENER(widget_dongle_battery_status, struct battery_state,
                            battery_status_update_cb, battery_status_get_state)

ZMK_SUBSCRIPTION(widget_dongle_battery_status, zmk_peripheral_battery_state_changed);

#if IS_ENABLED(CONFIG_ZMK_DONGLE_DISPLAY_DONGLE_BATTERY)
#if !IS_ENABLED(CONFIG_ZMK_SPLIT) || IS_ENABLED(CONFIG_ZMK_SPLIT_ROLE_CENTRAL)

ZMK_SUBSCRIPTION(widget_dongle_battery_status, zmk_battery_state_changed);
#if IS_ENABLED(CONFIG_USB_DEVICE_STACK)
ZMK_SUBSCRIPTION(widget_dongle_battery_status, zmk_usb_conn_state_changed);
#endif /* IS_ENABLED(CONFIG_USB_DEVICE_STACK) */
#endif /* !IS_ENABLED(CONFIG_ZMK_SPLIT) || IS_ENABLED(CONFIG_ZMK_SPLIT_ROLE_CENTRAL) */
#endif /* IS_ENABLED(CONFIG_ZMK_DONGLE_DISPLAY_DONGLE_BATTERY) */

int zmk_widget_dongle_battery_status_init(struct zmk_widget_dongle_battery_status *widget, lv_obj_t *parent) {
    widget->obj = lv_obj_create(parent);

    lv_obj_set_size(widget->obj, 240, 40);
    
    for (int i = 0; i < ZMK_SPLIT_CENTRAL_PERIPHERAL_COUNT + SOURCE_OFFSET; i++) {
        lv_obj_t *image_canvas = lv_canvas_create(widget->obj);
        lv_obj_t *battery_label = lv_label_create(widget->obj);

        lv_canvas_set_buffer(image_canvas, battery_image_buffer[i], 102, 5, LV_IMG_CF_TRUE_COLOR);

        lv_obj_align(image_canvas, LV_ALIGN_BOTTOM_MID, -60 +(i * 120), -8);
        lv_obj_align(battery_label, LV_ALIGN_TOP_MID, -60 +(i * 120), 0);

        lv_obj_add_flag(image_canvas, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(battery_label, LV_OBJ_FLAG_HIDDEN);
        
        battery_objects[i] = (struct battery_object){
            .symbol = image_canvas,
            .label = battery_label,
        };
    }

    sys_slist_append(&widgets, &widget->node);

    // Initialize peripheral tracking
    init_peripheral_tracking();

    widget_dongle_battery_status_init();

    return 0;
}

lv_obj_t *zmk_widget_dongle_battery_status_obj(struct zmk_widget_dongle_battery_status *widget) {
    return widget->obj;
}