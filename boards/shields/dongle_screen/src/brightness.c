#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/drivers/sensor.h>
#include <zephyr/drivers/led.h>
#include <zephyr/logging/log.h>
#include <zmk/event_manager.h>
#include <zmk/events/keycode_state_changed.h>
#include <zmk/events/layer_state_changed.h>
#include <math.h>

#include <stdlib.h>

int random0to100()
{
    return rand() % 101; // 0 to 100
}

LOG_MODULE_DECLARE(zmk, CONFIG_ZMK_LOG_LEVEL);

#if CONFIG_DONGLE_SCREEN_MIN_BRIGHTNESS > CONFIG_DONGLE_SCREEN_MAX_BRIGHTNESS
#error "DONGLE_SCREEN_MIN_BRIGHTNESS must be less than or equal to DONGLE_SCREEN_MAX_BRIGHTNESS!"
#endif

#if CONFIG_DONGLE_SCREEN_AMBIENT_LIGHT && (CONFIG_DONGLE_SCREEN_AMBIENT_LIGHT_MIN_BRIGHTNESS > CONFIG_DONGLE_SCREEN_MAX_BRIGHTNESS)
#error "DONGLE_SCREEN_AMBIENT_LIGHT_MIN_BRIGHTNESS must be less than or equal to DONGLE_SCREEN_MAX_BRIGHTNESS!"
#endif

#define BRIGHTNESS_STEP 1
#define BRIGHTNESS_DELAY_MS 2
#define BRIGHTNESS_FADE_DURATION_MS 500
#define SCREEN_IDLE_TIMEOUT_MS (CONFIG_DONGLE_SCREEN_IDLE_TIMEOUT_S * 1000)

static const struct device *pwm_leds_dev = DEVICE_DT_GET_ONE(pwm_leds);
#define DISP_BL DT_NODE_CHILD_IDX(DT_NODELABEL(disp_bl))

static int64_t last_activity = 0;
static uint8_t max_brightness = CONFIG_DONGLE_SCREEN_MAX_BRIGHTNESS;
static uint8_t min_brightness = CONFIG_DONGLE_SCREEN_MIN_BRIGHTNESS;
static int8_t current_brightness = CONFIG_DONGLE_SCREEN_MAX_BRIGHTNESS;

#if IS_ENABLED(CONFIG_DONGLE_SCREEN_AMBIENT_LIGHT)
static uint8_t ambient_min_brightness = CONFIG_DONGLE_SCREEN_AMBIENT_LIGHT_MIN_BRIGHTNESS;
#endif

#ifndef M_PI //No M_Pi in math.h
    #define M_PI 3.14159265358979323846
#endif

static int8_t brightness_modifier = 0;

static uint8_t clamp_brightness(int8_t value)
{
    if (value > max_brightness)
    {
        return max_brightness;
        LOG_WRN("CLAMPED: Screen brightness %d would be over %d", value, max_brightness);
    }
    if (value < min_brightness)
    {
        LOG_WRN("CLAMPED: Screen brightness %d would be under %d", value, min_brightness);
        return min_brightness;
    }
    return value;
}
static void apply_brightness(uint8_t value)
{
    led_set_brightness(pwm_leds_dev, DISP_BL, value);
    LOG_INF("Screen brightness set to %d", value);
}

static void fade_to_brightness(uint8_t from, uint8_t to)
{
    if (from == to)
    {
        apply_brightness(to);
        return;
    }

    const int step_delay = BRIGHTNESS_DELAY_MS;
    const int abs_diff = abs(to - from);                        // Total number of brightness steps in the fade

    /*
    Adjust the duration of the fade depending on how small the change is:
        - Small changes -> longer fade
        - For larger changes -> cap to avoid long transitions
    */
    int dynamic_duration = abs_diff < 4 ? 1000 : BRIGHTNESS_FADE_DURATION_MS;

    // Clamp duration to always be between 500ms and 1000ms
    if (dynamic_duration < 500)  dynamic_duration = 500;        // Minimum fade duration
    if (dynamic_duration > 1000) dynamic_duration = 1000;       // Maximum fade duration

    const int steps = dynamic_duration / step_delay;            // Total number of animation steps

    float diff = to - from;
    float tmp_brightness = 0.0f;
    uint8_t last_applied = 255;                                 // Keeps track of last value sent to avoid redundant updates. 225, first "rounded != last_applied" will always true

    for (int i = 0; i <= steps; i++) {
        float t = (float)i / steps;                             // Normalized time value: 0.0 at start, 1.0 at end

        /*
         Cosine easing (ease-in-out): (1 - cos(t * π)) / 2
            - Starts slow, accelerates in the middle, slows down again
            - Produces a smooth S-curve transitio flat > steep > flat
        */
        float eased = (1.0f - cosf(t * M_PI)) / 2.0f;
        tmp_brightness = from + diff * eased;                   // Interpolate the brightness using eased values
        uint8_t rounded = (uint8_t)(tmp_brightness + 0.5f);     // Convert float brightness to nearest integer

        // Only apply brightness if it actually changed - avoids redundant LED updates
        if (rounded != last_applied) {
            apply_brightness(rounded);
            last_applied = rounded;
        } 
        k_msleep(step_delay);
    }
    apply_brightness(to);                                       // Ensure the final brightness is applied to the end value
}

/*

static void fade_to_brightness(uint8_t from, uint8_t to)
{
    if (from == to)
    {
        apply_brightness(to);
        return;
    }
    if (from < to)
    {
        for (uint8_t b = from; b < to; b += BRIGHTNESS_STEP)
        {
            apply_brightness(b);
            k_msleep(BRIGHTNESS_DELAY_MS);
        }
    }
    else
    {
        for (int b = from; b > to; b -= BRIGHTNESS_STEP)
        {
            apply_brightness(b);
            k_msleep(BRIGHTNESS_DELAY_MS);
        }
    }
    apply_brightness(to);
}
*/
/*
static void fade_to_brightness(uint8_t from, uint8_t to)
{
    if (from == to)
    {
        LOG_INF("No fading needed. Brightness already at %d.", to);
        apply_brightness(to);
        return;
    }

    const int step_delay_ms = BRIGHTNESS_DELAY_MS;
    const int total_steps = BRIGHTNESS_FADE_DURATION_MS / step_delay_ms;

    float current = (float)from;
    float step = (float)(to - from) / total_steps;

    LOG_INF("Starting fade from %d to %d over %d ms (%d steps, step size: %.4f)",
            from, to, BRIGHTNESS_FADE_DURATION_MS, total_steps, step);


    for (int i = 0; i < total_steps; i++)
    {
        uint8_t brightness = (uint8_t)(current + 0.5f);
        LOG_DBG("Fade step %3d/%3d: setting brightness to %3d (raw: %.2f)",
                i + 1, total_steps, brightness, current);
        apply_brightness(brightness);
        current += step;
        k_msleep(step_delay_ms);
    }

    LOG_INF("Finalizing fade by applying exact target brightness: %d", to);
    apply_brightness(to);
}
*/

void set_screen_brightness(uint8_t value, bool ambient)
{
    int8_t new_brightness = clamp_brightness(value);

    // calculate how much the new_brightness must be increased if the result of new_brightness and brightness_modifier is less than min_brightness when ambient is false and is less than ambient_min_brightness when ambient is true
    if (ambient && (new_brightness + brightness_modifier <= ambient_min_brightness))
    {
        int8_t raw_brightness = new_brightness;
        new_brightness += ambient_min_brightness - (new_brightness + brightness_modifier);
        LOG_DBG("Ambient brightness (%d) + modifier (%d) (=%d) is less than or equal to ambient_min_brightness (%d), adjusting new_brightness by +%d to result in = %d.",
                raw_brightness, brightness_modifier, raw_brightness + brightness_modifier, ambient_min_brightness, new_brightness, new_brightness + brightness_modifier);
    }

    fade_to_brightness(clamp_brightness(current_brightness + brightness_modifier), clamp_brightness(new_brightness + brightness_modifier));
    current_brightness = new_brightness;
}

#if CONFIG_DONGLE_SCREEN_IDLE_TIMEOUT_S > 0 || CONFIG_DONGLE_SCREEN_BRIGHTNESS_KEYBOARD_CONTROL
// --- Brightness logic ---
static bool screen_on = true;
// --- Screen on/off ---

static void screen_set_on(bool on)
{
    if (on && !screen_on)
    {
        // TODO: Decide what to do when current_brightness + brightness_modifier is less or equals than min_brightness
        // Currently it can be that the screen is only turned on after the next ambient reading
        LOG_DBG("Current brightness: %d, modifier: %d", current_brightness, brightness_modifier);

        // temp
        if (current_brightness + brightness_modifier <= min_brightness)
        {
            LOG_DBG("Current brightness (%d) + modifier (%d) = %d is less than or equal to min_brightness (%d), adjusting modifier by +%d to result in = %d.",
                    current_brightness, brightness_modifier, current_brightness + brightness_modifier, min_brightness, CONFIG_DONGLE_SCREEN_BRIGHTNESS_STEP, current_brightness + brightness_modifier + CONFIG_DONGLE_SCREEN_BRIGHTNESS_STEP);
            brightness_modifier += CONFIG_DONGLE_SCREEN_BRIGHTNESS_STEP;
        }

        //

        fade_to_brightness(min_brightness, clamp_brightness(current_brightness + brightness_modifier));
        screen_on = true;
        LOG_INF("Screen on (smooth)");
    }
    else if (!on && screen_on)
    {
        fade_to_brightness(clamp_brightness(current_brightness + brightness_modifier), min_brightness);
        screen_on = false;
        LOG_INF("Screen off (smooth)");
    }
}

#endif

// --- Idle thread ---

#if CONFIG_DONGLE_SCREEN_IDLE_TIMEOUT_S > 0

void screen_idle_thread(void)
{
    while (1)
    {
        if (screen_on)
        {
            int64_t now = k_uptime_get();
            int64_t elapsed = now - last_activity;
            int64_t remaining = SCREEN_IDLE_TIMEOUT_MS - elapsed;

            if (remaining <= 0)
            {
                screen_set_on(false);
                // After turning off, sleep until next activity (key event will wake screen)
                k_sleep(K_FOREVER);
            }
            else
            {
                // Sleep exactly as long as needed until timeout or next key event
                k_sleep(K_MSEC(remaining));
            }
        }
        else
        {
            // If Screen is off, sleep forever (will be interrupted by key event)
            k_sleep(K_FOREVER);
        }
    }
}

K_THREAD_DEFINE(screen_idle_tid, 512, screen_idle_thread, NULL, NULL, NULL, 7, 0, 0);

#endif // CONFIG_DONGLE_SCREEN_IDLE_TIMEOUT_S > 0

// --- Brightness control via keyboard ---

#if CONFIG_DONGLE_SCREEN_BRIGHTNESS_KEYBOARD_CONTROL

static void increase_brightness(void)
{
    LOG_DBG("Current brightness: %d, current modifier: %d", current_brightness, brightness_modifier);

    int16_t next = (int16_t)current_brightness + brightness_modifier + CONFIG_DONGLE_SCREEN_BRIGHTNESS_STEP;
    LOG_DBG("Next brightness would be: %d Maximum brightness is: %d", next, max_brightness);

    if (next <= max_brightness)
    {
        brightness_modifier += CONFIG_DONGLE_SCREEN_BRIGHTNESS_STEP;
        LOG_DBG("New Brightness modifier: %d", brightness_modifier);
        set_screen_brightness(current_brightness, false);
    }
    else
    {
        LOG_WRN("Brightness modifier would be too high, calculating possible value.");
        // calculate how much the brightness_modifier can be increased by using next and max_brightness
        int16_t increase_possible = max_brightness - (int16_t)current_brightness - brightness_modifier;
        if (increase_possible > 0)
        {
            LOG_DBG("Brightness modifier can be increased by %d", increase_possible);

            brightness_modifier += increase_possible;
            LOG_DBG("Brightness modifier increased to %d", brightness_modifier);
            set_screen_brightness(current_brightness, false);
        }
        else
        {
            LOG_DBG("Brightness modifier cannot be increased further.");
        }
    }
}

static void decrease_brightness(void)
{
    LOG_DBG("Current brightness: %d, current modifier: %d", current_brightness, brightness_modifier);
    int16_t next = (int16_t)current_brightness + brightness_modifier - CONFIG_DONGLE_SCREEN_BRIGHTNESS_STEP;
    LOG_DBG("Next brightness would be: %d Minimum brightness is: %d", next, min_brightness);
    if (next > min_brightness)
    {
        brightness_modifier -= CONFIG_DONGLE_SCREEN_BRIGHTNESS_STEP;
        LOG_DBG("New Brightness modifier: %d", brightness_modifier);
        set_screen_brightness(current_brightness, false);
    }
    else
    {
        LOG_WRN("Brightness modifier would be too low, calculating possible value.");
        // calculate how much the brightness_modifier can be decreased by using next and min_brightness
        int16_t decrease_possible = (int16_t)current_brightness + brightness_modifier - min_brightness;
        if (decrease_possible > 0)
        {
            LOG_DBG("Brightness modifier can be decreased by %d", decrease_possible);

            brightness_modifier -= decrease_possible;
            LOG_DBG("Brightness modifier decreased to %d", brightness_modifier);
            set_screen_brightness(current_brightness, false);
        }
        else
        {
            LOG_DBG("Brightness modifier cannot be decreased further.");
        }
    }
}

#endif // CONFIG_DONGLE_SCREEN_BRIGHTNESS_KEYBOARD_CONTROL

#if CONFIG_DONGLE_SCREEN_IDLE_TIMEOUT_S > 0 || CONFIG_DONGLE_SCREEN_BRIGHTNESS_KEYBOARD_CONTROL

// --- Key event listener ---

static int key_listener(const zmk_event_t *eh)
{
    const struct zmk_keycode_state_changed *ev = as_zmk_keycode_state_changed(eh);
    if (ev && ev->state)
    { // Only on key down
        LOG_DBG("Key pressed: keycode=%d", ev->keycode);

#if CONFIG_DONGLE_SCREEN_BRIGHTNESS_KEYBOARD_CONTROL
        if (ev->keycode == CONFIG_DONGLE_SCREEN_BRIGHTNESS_UP_KEYCODE)
        {
            LOG_INF("Brightness UP key recognized!");
            increase_brightness();
            return 0;
        }
        else if (ev->keycode == CONFIG_DONGLE_SCREEN_BRIGHTNESS_DOWN_KEYCODE)
        {
            LOG_INF("Brightness DOWN key recognized!");
            decrease_brightness();
            return 0;
        }
#endif
    }

#if CONFIG_DONGLE_SCREEN_IDLE_TIMEOUT_S > 0
    last_activity = k_uptime_get();
    if (!screen_on)
    {
        screen_set_on(true);
        k_wakeup(screen_idle_tid);
    }
#else
    // Without idle thread: just turn on screen
    if (!screen_on)
    {
        screen_set_on(true);
    }
#endif
    return 0;
}

ZMK_LISTENER(screen_idle, key_listener);
ZMK_SUBSCRIPTION(screen_idle, zmk_keycode_state_changed);
ZMK_SUBSCRIPTION(screen_idle, zmk_layer_state_changed);

#endif

#if IS_ENABLED(CONFIG_DONGLE_SCREEN_AMBIENT_LIGHT)

#define AMBIENT_LIGHT_SENSOR_NODE DT_INST(0, avago_apds9960)
static const struct device *ambient_sensor = DEVICE_DT_GET(AMBIENT_LIGHT_SENSOR_NODE);

// Passe diese Werte nach deinen Messungen an!
const int32_t min_sensor = 0;
const int32_t max_sensor = 100; // TODO: Find real values!

static uint8_t ambient_to_brightness(int32_t sensor_value)
{
    if (sensor_value < min_sensor)
        sensor_value = min_sensor;
    if (sensor_value > max_sensor)
        sensor_value = max_sensor;
    uint8_t brightness = min_brightness +
                         ((sensor_value - min_sensor) * (max_brightness - min_brightness)) /
                             (max_sensor - min_sensor);
    return clamp_brightness(brightness);
}

static void ambient_light_thread(void)
{
    struct sensor_value val;
    uint8_t last_brightness = 0xFF; // Invalid initial value to force first update

    while (1)
    {

#ifndef CONFIG_DONGLE_SCREEN_AMBIENT_LIGHT_TEST

        if (!device_is_ready(ambient_sensor))
        {
            LOG_ERR("Ambient light sensor not ready!");
            // TODO: DELETE ME ONLY FOR TESTING!
            /*if (screen_on)
            {
                uint8_t new_brightness = 5;
                set_screen_brightness(new_brightness, true);
                last_brightness = new_brightness;
            }*/
            k_sleep(K_SECONDS(5));
            continue;
        }
        int rc = sensor_sample_fetch(ambient_sensor);
        if (rc == 0)
        {
            rc = sensor_channel_get(ambient_sensor, SENSOR_CHAN_LIGHT, &val);
            if (rc == 0)
            {
#else
        k_sleep(K_SECONDS(10));
        val.val1 = random0to100();

#endif
                LOG_DBG("APDS9960 raw: %d", val.val1);
                uint8_t new_brightness = ambient_to_brightness(val.val1);
                if (abs(new_brightness - last_brightness) > 5)
                {
                    // TODO: revisit this threshold
                    // TODO: Do I still need this when the if in set_screen_brightness() is there?
                    if (ambient_min_brightness > new_brightness + brightness_modifier)
                    {
                        LOG_DBG("Brightness (%d) incl. modifier (%d) (=%d) would be lower than ambient minimum setting (%d). Raw sensor value is: %d",
                                new_brightness, brightness_modifier, new_brightness + brightness_modifier, ambient_min_brightness, val.val1);

                        // new_brightness = ambient_min_brightness;
                    }
                    else
                    {
                        LOG_DBG("Ambient light: %d (raw) -> brightness %d + modifier %d = %d", val.val1, new_brightness, brightness_modifier, new_brightness + brightness_modifier);
                    }

                    if (screen_on)
                    {
                        set_screen_brightness(new_brightness, true);
                    }
                    else
                    {
                        // If the screen is off, just set the brightness variable
                        // to have the current ambient brightness when the screen is turned on again
                        current_brightness = new_brightness;
                    }
                    last_brightness = new_brightness;
                }
#ifndef CONFIG_DONGLE_SCREEN_AMBIENT_LIGHT_TEST
            }
        }
#endif                                                                              // CONFIG_DONGLE_SCREEN_AMBIENT_LIGHT_TEST
        k_sleep(K_MSEC(CONFIG_DONGLE_SCREEN_AMBIENT_LIGHT_EVALUATION_INTERVAL_MS)); // Adjust interval as needed
    }
}

K_THREAD_DEFINE(ambient_light_tid, 512, ambient_light_thread, NULL, NULL, NULL, 7, 0, 0);

#endif // CONFIG_DONGLE_SCREEN_AMBIENT_LIGHT

// --- Initialization ---

static int init_fixed_brightness(void)
{
    set_screen_brightness(current_brightness, false);
    last_activity = k_uptime_get();
#if CONFIG_DONGLE_SCREEN_IDLE_TIMEOUT_S > 0
    // Wake up the idle thread at boot
    k_wakeup(screen_idle_tid);
#else
    LOG_INF("Screen idle timeout disabled");
#endif
    return 0;
}

SYS_INIT(init_fixed_brightness, APPLICATION, CONFIG_APPLICATION_INIT_PRIORITY);