
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

/* Panel dimensions */
#define STATUS_BOX_WIDTH    160
#define STATUS_BOX_HEIGHT   44

/* Panel colors */
#define STATUS_BG_COLOR      0x244766
#define STATUS_BORDER_COLOR  0x79A8C7
#define STATUS_SHADOW_COLOR  0x5085AC
#define STATUS_ACCENT_COLOR  0x56BFD8

/* Child indexes inside the status panel */
#define BLE_STATUS_CHILD_INDEX 2
#define TRIANGLE_CHILD_INDEX   5

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

/*
 * Create a small filled upward-pointing triangle.
 *
 * Five horizontal rows form a solid 9 x 5 pixel triangle:
 *
 *     #
 *    ###
 *   #####
 *  #######
 * #########
 *
 * This avoids requiring a Unicode triangle glyph.
 */
static lv_obj_t *create_selection_triangle(lv_obj_t *parent)
{
    lv_obj_t *triangle = lv_obj_create(parent);

    lv_obj_set_size(triangle, 9, 5);
    lv_obj_set_style_bg_opa(
        triangle, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_set_style_border_width(
        triangle, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_all(
        triangle, 0, LV_PART_MAIN);
    lv_obj_set_style_radius(
        triangle, 0, LV_PART_MAIN);

    lv_obj_clear_flag(
        triangle, LV_OBJ_FLAG_SCROLLABLE);

    for (int row = 0; row < 5; row++)
    {
        lv_obj_t *pixel_row = lv_obj_create(triangle);

        lv_obj_set_size(pixel_row, 1 + (row * 2), 1);
        lv_obj_set_pos(pixel_row, 4 - row, row);

        lv_obj_set_style_bg_color(
            pixel_row,
            lv_color_hex(STATUS_ACCENT_COLOR),
            LV_PART_MAIN);

        lv_obj_set_style_bg_opa(
            pixel_row, LV_OPA_COVER, LV_PART_MAIN);

        lv_obj_set_style_border_width(
            pixel_row, 0, LV_PART_MAIN);

        lv_obj_set_style_radius(
            pixel_row, 0, LV_PART_MAIN);

        lv_obj_set_style_pad_all(
            pixel_row, 0, LV_PART_MAIN);

        lv_obj_clear_flag(
            pixel_row, LV_OBJ_FLAG_SCROLLABLE);
    }

    return triangle;
}

static void set_status_symbol(
    struct zmk_widget_output_status *widget,
    struct output_status_state state)
{
    const char *usb_color = "ffffff";
    const char *ble_color = "ffffff";

    /* Original USB status colors */
    if (!state.usb_is_hid_ready)
    {
        usb_color = "ff0000";
    }

    /* Original BLE status colors */
    if (state.active_profile_connected)
    {
        ble_color = "00ff00";
    }
    else if (state.active_profile_bonded)
    {
        ble_color = "0000ff";
    }

    /*
     * The outer widget contains the status panel.
     * Inside the panel:
     * 0 = USB label
     * 1 = first separator
     * 2 = BLE label
     * 3 = second separator
     * 4 = Bluetooth profile number
     * 5 = selection triangle
     */
    lv_obj_t *status_box =
        lv_obj_get_child(widget->obj, 0);

    lv_obj_t *ble_status_label =
        lv_obj_get_child(
            status_box, BLE_STATUS_CHILD_INDEX);

    lv_obj_t *triangle =
        lv_obj_get_child(
            status_box, TRIANGLE_CHILD_INDEX);

    /* Update USB status */
    char usb_text[32];

    snprintf(
        usb_text,
        sizeof(usb_text),
        "#%s USB#",
        usb_color
    );

    lv_label_set_text(
        widget->transport_label, usb_text);

    /* Update BLE status */
    char ble_text[32];

    snprintf(
        ble_text,
        sizeof(ble_text),
        "#%s BLE#",
        ble_color
    );

    lv_label_set_text(
        ble_status_label, ble_text);

    /* Update active Bluetooth profile */
    char profile_text[12];

    snprintf(
        profile_text,
        sizeof(profile_text),
        "%d",
        state.active_profile_index + 1
    );

    lv_label_set_text(
        widget->ble_label, profile_text);

    /*
     * Recalculate label positions before moving
     * the triangle underneath the selected output.
     */
    lv_obj_update_layout(status_box);

    switch (state.selected_endpoint.transport)
    {
    case ZMK_TRANSPORT_USB:
        lv_obj_clear_flag(
            triangle, LV_OBJ_FLAG_HIDDEN);

        lv_obj_align_to(
            triangle,
            widget->transport_label,
            LV_ALIGN_OUT_BOTTOM_MID,
            0,
            2
        );
        break;

    case ZMK_TRANSPORT_BLE:
        lv_obj_clear_flag(
            triangle, LV_OBJ_FLAG_HIDDEN);

        lv_obj_align_to(
            triangle,
            ble_status_label,
            LV_ALIGN_OUT_BOTTOM_MID,
            0,
            2
        );
        break;

    default:
        lv_obj_add_flag(
            triangle, LV_OBJ_FLAG_HIDDEN);
        break;
    }
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

ZMK_SUBSCRIPTION(
    widget_output_status, zmk_endpoint_changed);

ZMK_SUBSCRIPTION(
    widget_output_status, zmk_ble_active_profile_changed);

ZMK_SUBSCRIPTION(
    widget_output_status, zmk_usb_conn_state_changed);

int zmk_widget_output_status_init(
    struct zmk_widget_output_status *widget,
    lv_obj_t *parent)
{
    /* Transparent outer widget */
    widget->obj = lv_obj_create(parent);

    lv_obj_set_size(widget->obj, 240, 64);

    lv_obj_set_style_bg_opa(
        widget->obj, LV_OPA_TRANSP, LV_PART_MAIN);

    lv_obj_set_style_border_width(
        widget->obj, 0, LV_PART_MAIN);

    lv_obj_set_style_pad_all(
        widget->obj, 0, LV_PART_MAIN);

    lv_obj_clear_flag(
        widget->obj, LV_OBJ_FLAG_SCROLLABLE);

    /* Main connection panel */
    lv_obj_t *status_box = lv_obj_create(widget->obj);

    lv_obj_set_size(
        status_box,
        STATUS_BOX_WIDTH,
        STATUS_BOX_HEIGHT
    );

    lv_obj_align(
        status_box,
        LV_ALIGN_TOP_RIGHT,
        -10,
        8
    );

    /* Panel background */
    lv_obj_set_style_bg_color(
        status_box,
        lv_color_hex(STATUS_BG_COLOR),
        LV_PART_MAIN);

    lv_obj_set_style_bg_opa(
        status_box, LV_OPA_COVER, LV_PART_MAIN);

    /* Border */
    lv_obj_set_style_border_color(
        status_box,
        lv_color_hex(STATUS_BORDER_COLOR),
        LV_PART_MAIN);

    lv_obj_set_style_border_width(
        status_box, 1, LV_PART_MAIN);

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

    lv_obj_set_style_pad_all(
        status_box, 0, LV_PART_MAIN);

    lv_obj_clear_flag(
        status_box, LV_OBJ_FLAG_SCROLLABLE);

    /*
     * Row layout:
     *
     * USB  |  BLE  |  3
     *
     * Triangle is positioned beneath
     * USB or BLE independently.
     */

    /* USB label - child 0 */
    widget->transport_label =
        lv_label_create(status_box);

    lv_label_set_recolor(
        widget->transport_label, true);

    lv_obj_align(
        widget->transport_label,
        LV_ALIGN_TOP_LEFT,
        13,
        7
    );

    /* First separator - child 1 */
    lv_obj_t *separator1 = lv_label_create(status_box);

    lv_label_set_text(separator1, "|");

    lv_obj_set_style_text_color(
        separator1,
        lv_color_hex(0x91A5BD),
        LV_PART_MAIN);

    lv_obj_align(
        separator1,
        LV_ALIGN_TOP_LEFT,
        54,
        7
    );

    /* BLE label - child 2 */
    lv_obj_t *ble_status_label =
        lv_label_create(status_box);

    lv_label_set_recolor(
        ble_status_label, true);

    lv_obj_align(
        ble_status_label,
        LV_ALIGN_TOP_LEFT,
        72,
        7
    );

    /* Second separator - child 3 */
    lv_obj_t *separator2 = lv_label_create(status_box);

    lv_label_set_text(separator2, "|");

    lv_obj_set_style_text_color(
        separator2,
        lv_color_hex(0x91A5BD),
        LV_PART_MAIN);

    lv_obj_align(
        separator2,
        LV_ALIGN_TOP_LEFT,
        113,
        7
    );

    /* Bluetooth profile number - child 4 */
    widget->ble_label = lv_label_create(status_box);

    lv_obj_set_style_text_color(
        widget->ble_label,
        lv_color_white(),
        LV_PART_MAIN);

    lv_obj_align(
        widget->ble_label,
        LV_ALIGN_TOP_LEFT,
        133,
        7
    );

    /* Filled selection triangle - child 5 */
    create_selection_triangle(status_box);

    /* Register widget */
    sys_slist_append(&widgets, &widget->node);

    widget_output_status_init();

    return 0;
}

lv_obj_t *zmk_widget_output_status_obj(
    struct zmk_widget_output_status *widget)
{
    return widget->obj;
}
