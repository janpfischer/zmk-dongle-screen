/*
 * Copyright (c) 2024 The ZMK Contributors
 *
 * SPDX-License-Identifier: MIT
 */

#include <zephyr/kernel.h>

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
        .selected_endpoint = zmk_endpoints_selected(),                     // 0 = USB , 1 = BLE
        .active_profile_index = zmk_ble_active_profile_index(),            // 0-3 BLE profiles
        .active_profile_connected = zmk_ble_active_profile_is_connected(), // 0 = not connected, 1 = connected
        .active_profile_bonded = !zmk_ble_active_profile_is_open(),        // 0 =  BLE not bonded, 1 = bonded
        .usb_is_hid_ready = zmk_usb_is_hid_ready()};                       // 0 = not ready, 1 = ready
}

static void set_status_symbol(struct zmk_widget_output_status *widget, struct output_status_state state)
{
    char connection_text[32] = {};
    lv_color_t text_color;
    
    switch (state.selected_endpoint.transport)
    {
    case ZMK_TRANSPORT_USB:
        snprintf(connection_text, sizeof(connection_text), "USB");
        // USB: White if ready, Red if not ready
        if (state.usb_is_hid_ready) {
            text_color = lv_color_hex(0xFFFFFF);
        } else {
            text_color = lv_color_hex(0xFF0000);
        }
        break;
        
    case ZMK_TRANSPORT_BLE:
        snprintf(connection_text, sizeof(connection_text), "BLUETOOTH");
        // BLE: Green if connected, Blue if bonded but not connected, White if not bonded
        if (state.active_profile_connected) {
            text_color = lv_color_hex(0x4ADE80);  // Green - connected
        } else if (state.active_profile_bonded) {
            text_color = lv_color_hex(0x60A5FA);  // Blue - bonded but not connected
        } else {
            text_color = lv_color_hex(0xFFFFFF);  // White - not bonded
        }
        break;
        
    default:
        snprintf(connection_text, sizeof(connection_text), "UNKNOWN");
        text_color = lv_color_hex(0xFF0000);
        break;
    }

    lv_label_set_text(widget->connection_label, connection_text);
    lv_obj_set_style_text_color(widget->connection_label, text_color, 0);
}

static void output_status_update_cb(struct output_status_state state)
{
    struct zmk_widget_output_status *widget;
    SYS_SLIST_FOR_EACH_CONTAINER(&widgets, widget, node)
    {
        set_status_symbol(widget, state);
    }
}

ZMK_DISPLAY_WIDGET_LISTENER(widget_output_status, struct output_status_state,
                            output_status_update_cb, get_state)
ZMK_SUBSCRIPTION(widget_output_status, zmk_endpoint_changed);
ZMK_SUBSCRIPTION(widget_output_status, zmk_ble_active_profile_changed);
ZMK_SUBSCRIPTION(widget_output_status, zmk_usb_conn_state_changed);

int zmk_widget_output_status_init(struct zmk_widget_output_status *widget, lv_obj_t *parent)
{
    widget->obj = lv_obj_create(parent);
    lv_obj_set_size(widget->obj, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
    lv_obj_set_style_bg_opa(widget->obj, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(widget->obj, 0, 0);
    lv_obj_set_style_pad_all(widget->obj, 0, 0);

    widget->connection_label = lv_label_create(widget->obj);
    lv_obj_set_style_text_font(widget->connection_label, &lv_font_montserrat_10, 0);
    lv_label_set_text(widget->connection_label, "---");

    sys_slist_append(&widgets, &widget->node);

    widget_output_status_init();
    return 0;
}

lv_obj_t *zmk_widget_output_status_obj(struct zmk_widget_output_status *widget)
{
    return widget->obj;
}