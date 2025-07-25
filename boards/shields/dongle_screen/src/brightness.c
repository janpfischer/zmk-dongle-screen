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

static int8_t brightness_modifier = 0;

static bool off_through_modifier = false; // Used to track if the screen was turned off through the brightness modifier

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




// Threaded fade logic
// Contains starting and target brightness levels to be animated
struct fade_request_t {
    uint8_t from;           // Starting brightness level
    uint8_t to;             // Target brightness level
};

#define FADE_QUEUE_SIZE 4

// Message queue used to send fade requests to the fade handler thread.
// It holds up to 4 fade_request_t elements and ensures brightness updates are handled sequentially.
K_MSGQ_DEFINE(fade_msgq, sizeof(struct fade_request_t), FADE_QUEUE_SIZE, 4);

// Cubic ease-in-out function to smooth the interpolation curve.
// Provides a natural "S-curve" animation effect: starts slow, accelerates, then slows again.
// Helps avoid abrupt changes in perceived brightness.
// delete old cpu heavy cons calculation
static float ease_in_out(float t) {
    if (t < 0.5f) return 4.0f * t * t * t;
    float f = -2.0f * t + 2.0f;
    return 1.0f - (f * f * f) / 2.0f;
}

// Dedicated thread responsible for handling all fade animations.
// Receives fade requests from the queue and applies brightness changes over time using easing.
void fade_thread(void) {
    struct fade_request_t req;

    while (1) {
        // Wait indefinitely for the next fade request to arrive in the queue
        if (k_msgq_get(&fade_msgq, &req, K_FOREVER) == 0) {

            // Skip animation entirely if brightness difference is too small
            if (req.from == req.to || abs(req.to - req.from) <= 1) {
                apply_brightness(req.to);
                continue;
            }

            // Calculate brightness difference and use it to determine number of steps
            int diff = abs(req.to - req.from);
            int steps = CLAMP(diff * 2, 6, 32); // More steps for smoother fades over large differences

            // Set total animation time: scale with difference but clamp between 500ms and 1000ms
            int total_duration_ms = CLAMP(diff * 20, 500, 1000); // 20ms per level as baseline
            int delay_us = (total_duration_ms * 1000) / steps;   // Delay between steps in microseconds

            uint8_t last_applied = 255; // Used to prevent redundant LED updates to save performance

            // Interpolate brightness across 'steps' frames using easing
            for (int i = 0; i <= steps; i++) {
                float t = (float)i / steps;                // Normalized time in [0, 1]
                float eased = ease_in_out(t);              // Eased time for smoother progression
                float interpolated = req.from + (req.to - req.from) * eased; // Interpolated value
                uint8_t brightness = (uint8_t)(interpolated + 0.5f);         // Rounded to nearest integer

                // Only send update if brightness actually changed
                if (brightness != last_applied) {
                    apply_brightness(brightness);
                    last_applied = brightness;
                }

                k_usleep(delay_us); // Sleep before next step to pace the fade
            }

            // safeguard to ensure the target value is set at the end
            if (last_applied != req.to) {
                apply_brightness(req.to);
            }
        }
    }
}

// Launch the fade thread with 768 bytes of stack, medium priority (6)
// 512 was too small for logging, math (float, int), small loop, few stack-local variables
// 768 is just a guess, optimization is possible, probably
K_THREAD_DEFINE(fade_tid, 768, fade_thread, NULL, NULL, NULL, 6, 0, 0);

// Function to submit a brightness fade request
// Ensures that only the most recent fade request is applied by purging the queue first for changes in between animations
static void fade_to_brightness(uint8_t from, uint8_t to)
{
    struct fade_request_t req = { .from = from, .to = to };
    k_msgq_purge(&fade_msgq);          // Clear any pending fades to avoid outdated transitions
    k_msgq_put(&fade_msgq, &req, K_NO_WAIT); // Submit the new fade request without blocking
}






void set_screen_brightness(uint8_t value, bool ambient)
{
    int8_t new_brightness = clamp_brightness(value);

#if CONFIG_DONGLE_SCREEN_AMBIENT_LIGHT
    // calculate how much the new_brightness must be increased if the result of new_brightness and brightness_modifier is less than min_brightness when ambient is false and is less than ambient_min_brightness when ambient is true
    if (ambient && (new_brightness + brightness_modifier <= ambient_min_brightness))
    {
        int8_t raw_brightness = new_brightness;
        new_brightness += ambient_min_brightness - (new_brightness + brightness_modifier);
        LOG_DBG("Ambient brightness (%d) + modifier (%d) (=%d) is less than or equal to ambient_min_brightness (%d), adjusting new_brightness by +%d to result in = %d.",
                raw_brightness, brightness_modifier, raw_brightness + brightness_modifier, ambient_min_brightness, new_brightness, new_brightness + brightness_modifier);
    }
    else if (ambient && (new_brightness + brightness_modifier > max_brightness))
    {
        int8_t raw_brightness = new_brightness;
        new_brightness -= (new_brightness + brightness_modifier) - max_brightness;
        LOG_DBG("Ambient brightness (%d) + modifier (%d) (=%d) is more than max_brightness (%d), adjusting new_brightness by -%d to result in = %d.",
                raw_brightness, brightness_modifier, raw_brightness + brightness_modifier, max_brightness, raw_brightness - new_brightness, new_brightness + brightness_modifier);
    }
#endif

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
        off_through_modifier = false; // Reset the flag, because the screen is turned on again
        LOG_INF("Screen on (smooth)");
    }
    else if (!on && screen_on)
    {
        fade_to_brightness(clamp_brightness(current_brightness + brightness_modifier), min_brightness);
        screen_on = false;
        LOG_INF("Screen off (smooth)");
    }
    else
    {
        LOG_DBG("Screen state is already %s, no action taken.", on ? "on" : "off");
    }
}

#endif

// --- Idle thread ---

#if CONFIG_DONGLE_SCREEN_IDLE_TIMEOUT_S > 0

void screen_idle_thread(void)
{
    while (1)
    {
        // Thread should run even if the screen is off, but only if the screen is off through the modifier
        if (screen_on || (!screen_on && off_through_modifier))
        {
            int64_t now = k_uptime_get();
            int64_t elapsed = now - last_activity;
            int64_t remaining = SCREEN_IDLE_TIMEOUT_MS - elapsed;

            if (remaining <= 0)
            {
                screen_set_on(false);
                off_through_modifier = false; // Reset the flag, because the screen is turned off
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

    // TODO: Decide if change:
    // If the brightness_modifier is so small that the display remains off because current_brightness + brightness_modifier <= min_brightness,
    // then the display should still be turned on.
    // This is to ensure that the display is turned on when the brightness is increased, even if the modifier is small enough to keep it off.

    if ((current_brightness + brightness_modifier > min_brightness) && off_through_modifier)
    {

        LOG_WRN("Current brightness (%d) + modifier (%d) = %d is more than min_brightness (%d), setting screen on.",
                current_brightness, brightness_modifier, current_brightness + brightness_modifier, min_brightness);
        screen_set_on(true);
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

    if (current_brightness + brightness_modifier <= min_brightness)
    {

        LOG_WRN("Current brightness (%d) + modifier (%d) = %d is less than or equal to min_brightness (%d), setting screen off.",
                current_brightness, brightness_modifier, current_brightness + brightness_modifier, min_brightness);
        off_through_modifier = true; // Track that the screen was turned off through the
        screen_set_on(false);
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
        else if (ev->keycode == CONFIG_DONGLE_SCREEN_TOGGLE_KEYCODE)
        {
            LOG_INF("Toggle screen key recognized!");
            // Toggle screen on/off
            if (screen_on)
            {
                off_through_modifier = true; // Track that the screen was turned off through the toggle key
                screen_set_on(false);
            }
            else
            {
                screen_set_on(true);
            }
            return 0;
        }

#endif
    }

#if CONFIG_DONGLE_SCREEN_IDLE_TIMEOUT_S > 0
    last_activity = k_uptime_get();
    if (!screen_on && !off_through_modifier)
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
                    else if (new_brightness + brightness_modifier > max_brightness)
                    {
                        LOG_DBG("Brightness (%d) incl. modifier (%d) (=%d) would be higher than maximum setting (%d). Raw sensor value is: %d",
                                new_brightness, brightness_modifier, new_brightness + brightness_modifier, max_brightness, val.val1);

                        // new_brightness = max_brightness;
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