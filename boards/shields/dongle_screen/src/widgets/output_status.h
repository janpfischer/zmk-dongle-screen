/*
 * Copyright (c) 2024 The ZMK Contributors
 *
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include <lvgl.h>
#include <zephyr/kernel.h>

#define LV_FONT_MONTSERRAT_10 1


// output_status.h
struct zmk_widget_output_status {
    sys_snode_t node;
    lv_obj_t *obj;
    lv_obj_t *connection_label;  // Changed from transport_label and ble_label
};

int zmk_widget_output_status_init(struct zmk_widget_output_status *widget, lv_obj_t *parent);
lv_obj_t *zmk_widget_output_status_obj(struct zmk_widget_output_status *widget);


