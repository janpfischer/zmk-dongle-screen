
/*
 * Copyright (c) 2024 The ZMK Contributors
 *
 * SPDX-License-Identifier: MIT
 */

#include <zephyr/kernel.h>
#include <stdio.h>

#include <zephyr/logging/log.h>
LOG_MODULE_DECLARE(zmk, CONFIG_ZMK_LOG_LEVEL);

#include <zmk/display.h>
#include <zmk/event_manager.h>
#include <zmk/events/ble_active_profile_changed.h>
#include <zmk/events/endpoint_changed.h>
#include <zmk/events/usb_conn_state_changed.h>
#include <zmk/usb.h>
#include <zmk/ble.h>
#include <zmk/endpoints.h>

#include "output_status.h"

static sys_slist_t widgets = SYS_SLIST_STATIC_INIT(&widgets);

/* Panel appearance */
#define STATUS_BOX_WIDTH    150
#define STATUS_BOX_HEIGHT   34

#define STATUS_BG_COLOR     0x244766
#define STATUS_BORDER_COLOR 0x79A8C7
#define STATUS_SHADOW_COLOR 0x5085AC

struct output_status_state
{
    struct zmk_endpoint_instance selected_endpoint;
    int active_profile_index;
    bool active_profile_connected;
    bool active_profile_bonded;
    bool usb_is_hid_ready;
};

static struct output_status_state get_state(const zmk_event_t *_eh)
{
    return (struct output_status_state){
        .selected_endpoint = zmk_endpoints_selected(),
        .active_profile_index = zmk_ble_active_profile_index(),
        .active_profile_connected = zmk_ble_active_profile_is_connected(),
        .active_profile_bonded = !zmk_ble_active_profile_is_open(),
        .usb_is_hid_ready = zmk_usb_is_hid_ready()
    };
}

static void set_status_symbol(
    struct zmk_widget_output_status *widget,
    struct output_status_state state)
{
    const char *ble_color = "ffffff";
    const char *usb_color = "ffffff";

    /* Preserve original USB status colors */
    if (!state.usb_is_hid_ready)
    {
        usb_color = "ff0000";
    }

    /* Preserve original BLE status colors */
    if (state.active_profile_connected)
    {
        ble_color = "00ff00";
    }
    else if (state.active_profile_bonded)
    {
        ble_color = "0000ff";
    }

    char transport_text[128] = {};

    /* Horizontal indicators with selected transport arrow */
    switch (state.selected_endpoint.transport)
    {
    case ZMK_TRANSPORT_USB:
        snprintf(
            transport_text,
            sizeof(transport_text),
            "#56BFD8 ># #%s USB# #91A5BD |# #%s BLE# #91A5BD |#",
            usb_color,
            ble_color
        );
        break;

    case ZMK_TRANSPORT_BLE:
        snprintf(
            transport_text,
            sizeof(transport_text),
            "#%s USB# #91A5BD |# #56BFD8 ># #%s BLE# #91A5BD |#",
            usb_color,
            ble_color
        );
        break;

    default:
        snprintf(
            transport_text,
            sizeof(transport_text),
            "#%s USB# #91A5BD |# #%s BLE# #91A5BD |#",
            usb_color,
            ble_color
        );
        break;
    }

    lv_label_set_text(widget->transport_label, transport_text);

    /* Active Bluetooth profile number */
    char ble_text[12];

    snprintf(
        ble_text,
        sizeof(ble_text),
        "%d",
        state.active_profile_index + 1
    );

    lv_label_set_text(widget->ble_label, ble_text);
}

static void output_status_update_cb(
    struct output_status_state state)
{
    struct zmk_widget_output_status *widget;

    SYS_SLIST_FOR_EACH_CONTAINER(&widgets, widget, node)
    {
        set_status_symbol(widget, state);
    }
}

ZMK_DISPLAY_WIDGET_LISTENER(
    widget_output_status,
    struct output_status_state,
    output_status_update_cb,
    get_state
)

ZMK_SUBSCRIPTION(widget_output_status, zmk_endpoint_changed);
ZMK_SUBSCRIPTION(widget_output_status, zmk_ble_active_profile_changed);
ZMK_SUBSCRIPTION(widget_output_status, zmk_usb_conn_state_changed);

int zmk_widget_output_status_init(
    struct zmk_widget_output_status *widget,
    lv_obj_t *parent)
{
    /* Transparent outer widget container */
    widget->obj = lv_obj_create(parent);
    lv_obj_set_size(widget->obj, 240, 60);

    lv_obj_set_style_bg_opa(
        widget->obj, LV_OPA_TRANSP, LV_PART_MAIN);

    lv_obj_set_style_border_width(
        widget->obj, 0, LV_PART_MAIN);

    lv_obj_set_style_pad_all(
        widget->obj, 0, LV_PART_MAIN);

    lv_obj_clear_flag(
        widget->obj, LV_OBJ_FLAG_SCROLLABLE);

    /* Create connection status panel */
    lv_obj_t *status_box = lv_obj_create(widget->obj);

    lv_obj_set_size(
        status_box, STATUS_BOX_WIDTH, STATUS_BOX_HEIGHT);

    lv_obj_align(
        status_box, LV_ALIGN_TOP_RIGHT, -10, 8);

    /* Solid blue panel background */
    lv_obj_set_style_bg_color(
        status_box,
        lv_color_hex(STATUS_BG_COLOR),
        LV_PART_MAIN);

    lv_obj_set_style_bg_opa(
        status_box, LV_OPA_COVER, LV_PART_MAIN);

    /* Thin blue border */
    lv_obj_set_style_border_color(
        status_box,
        lv_color_hex(STATUS_BORDER_COLOR),
        LV_PART_MAIN);

    lv_obj_set_style_border_width(
        status_box, 1, LV_PART_MAIN);

    /* Rounded corners */
    lv_obj_set_style_radius(
        status_box, 6, LV_PART_MAIN);

    /* Soft blue shadow */
    lv_obj_set_style_shadow_color(
        status_box,
        lv_color_hex(STATUS_SHADOW_COLOR),
        LV_PART_MAIN);

    lv_obj_set_style_shadow_width(
        status_box, 8, LV_PART_MAIN);

    lv_obj_set_style_shadow_opa(
        status_box, 90, LV_PART_MAIN);

    lv_obj_set_style_shadow_spread(
        status_box, 1, LV_PART_MAIN);

    lv_obj_set_style_shadow_ofs_x(
        status_box, 0, LV_PART_MAIN);

    lv_obj_set_style_shadow_ofs_y(
        status_box, 1, LV_PART_MAIN);

    /* Remove default padding and scrolling */
    lv_obj_set_style_pad_all(
        status_box, 0, LV_PART_MAIN);

    lv_obj_clear_flag(
        status_box, LV_OBJ_FLAG_SCROLLABLE);

    /* USB / BLE indicator label */
    widget->transport_label = lv_label_create(status_box);

    lv_label_set_recolor(
        widget->transport_label, true);

    lv_obj_set_style_text_color(
        widget->transport_label,
        lv_color_white(),
        LV_PART_MAIN);

    lv_obj_set_style_text_letter_space(
        widget->transport_label, 0, LV_PART_MAIN);

    lv_obj_align(
        widget->transport_label,
        LV_ALIGN_LEFT_MID, 8, 0);

    /* Bluetooth profile number label */
    widget->ble_label = lv_label_create(status_box);

    lv_obj_set_style_text_color(
        widget->ble_label,
        lv_color_white(),
        LV_PART_MAIN);

    lv_obj_set_style_text_letter_space(
        widget->ble_label, 0, LV_PART_MAIN);

    lv_obj_align(
        widget->ble_label,
        LV_ALIGN_RIGHT_MID, -8, 0);

    /* Register widget and initialize state updates */
    sys_slist_append(&widgets, &widget->node);

    widget_output_status_init();

    return 0;
}

lv_obj_t *zmk_widget_output_status_obj(
    struct zmk_widget_output_status *widget)
{
    return widget->obj;
}
