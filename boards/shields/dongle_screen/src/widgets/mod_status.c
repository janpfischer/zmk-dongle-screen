#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zmk/hid.h>
#include <lvgl.h>
#include "mod_status.h"
#include <fonts.h> // <-- Wichtig für LV_FONT_DECLARE

LOG_MODULE_DECLARE(zmk, CONFIG_ZMK_LOG_LEVEL);

static void update_mod_status(struct zmk_widget_mod_status *widget)
{
    uint8_t mods = zmk_hid_get_keyboard_report()->body.modifiers;
    char text[32] = "";
    int idx = 0;

    // Temporäre Puffer für Symbole
    char *syms[4];
    int n = 0;

    if (mods & (MOD_LCTL | MOD_RCTL))
        syms[n++] = "󰘴";
    if (mods & (MOD_LSFT | MOD_RSFT))
        syms[n++] = "󰘶"; // U+F0636
    if (mods & (MOD_LALT | MOD_RALT))
        syms[n++] = "󰘵"; // U+F0635
    if (mods & (MOD_LGUI | MOD_RGUI))
    // set next syms according to CONFIG_DONGLE_SCREEN_SYSTEM (0,1,2)
#if CONFIG_DONGLE_SCREEN_SYSTEM_ICON == 1
        syms[n++] = "󰌽"; // U+DF3D
#elif CONFIG_DONGLE_SCREEN_SYSTEM_ICON == 2
        syms[n++] = ""; // U+E62A
#else
        syms[n++] = "󰘳"; // U+F0633
#endif

    for (int i = 0; i < n; ++i)
    {
        if (i > 0)
            idx += snprintf(&text[idx], sizeof(text) - idx, " ");
        idx += snprintf(&text[idx], sizeof(text) - idx, "%s", syms[i]);
    }

    lv_label_set_text(widget->label, idx ? text : "");
}

static void mod_status_timer_cb(struct k_timer *timer)
{
    struct zmk_widget_mod_status *widget = k_timer_user_data_get(timer);
    update_mod_status(widget);
}

static struct k_timer mod_status_timer;

int zmk_widget_mod_status_init(struct zmk_widget_mod_status *widget, lv_obj_t *parent)
{
    widget->obj = lv_obj_create(parent);
    lv_obj_set_size(widget->obj, 180, 40);

    widget->label = lv_label_create(widget->obj);
    lv_obj_align(widget->label, LV_ALIGN_CENTER, 0, 0);
    lv_label_set_text(widget->label, "-");
    lv_obj_set_style_text_font(widget->label, &NerdFonts_Regular_40, 0); // <-- NerdFont setzen

    k_timer_init(&mod_status_timer, mod_status_timer_cb, NULL);
    k_timer_user_data_set(&mod_status_timer, widget);
    k_timer_start(&mod_status_timer, K_MSEC(100), K_MSEC(100));

    return 0;
}

lv_obj_t *zmk_widget_mod_status_obj(struct zmk_widget_mod_status *widget)
{
    return widget->obj;
}
