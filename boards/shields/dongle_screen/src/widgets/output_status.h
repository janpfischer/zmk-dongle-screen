/*
 * Copyright (c) 2024 The ZMK Contributors
 *
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include <lvgl.h>
#include <zephyr/kernel.h>

// output_status.h
struct zmk_widget_output_status
{
    lv_obj_t *obj;
    lv_obj_t *transport_label;
    lv_obj_t *ble_label;
    sys_snode_t node;
};

int zmk_widget_output_status_init(struct zmk_widget_output_status *widget, lv_obj_t *parent);
lv_obj_t *zmk_widget_output_status_obj(struct zmk_widget_output_status *widget);