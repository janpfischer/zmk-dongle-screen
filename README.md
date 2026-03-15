# ZMK Dongle Screen YADS (Yet another Dongle Screen), with color modifiers

This project provides a Zephyr module for a dongle display shield based on the ST7789V display, the Seeeduino XIAO BLE (or nice!nano v2) microcontroller, and the LVGL graphics library.

**This is a fork of the wonderful YADS [zmk-dongle-screen][https://github.com/janpfischer/zmk-dongle-screen]. All credit goes there.**

It offers color-coded widgets for output status, layer, modifiers, WPM, and battery — as well as keyboard-controlled brightness, idle auto-off, and a fully customizable layout.

This assumes you already know about dongles.

## Widgets Overview

All widgets can be individually enabled or disabled via configuration (no code changes needed).

### Output Widget

Shows the current output state (USB or BLE). The active interface is marked with a `>` arrow in the same color as the label.

- **USB:**
  - **Purple:** USB HID is ready — the dongle is connected to a computer and working as a keyboard.
  - **Red:** USB HID is not ready — the dongle is powered but not connected to a computer.
- **BLE:**
  - **Blue:** Always shown in blue (profile number displayed on the next line).

The `>` arrow matches the color of the active transport.

### Layer Widget

Displays the currently active keyboard layer name (or index if unnamed).

- **White:** Base layer (index 0)
- **Yellow:** Any other layer

### Mod Widget

Shows which modifier keys are currently held (Shift, Ctrl, Alt, GUI). The GUI icon can be configured for macOS, Linux, or Windows.

### WPM Widget

Displays real-time typing speed (words per minute). A **WPM** label appears above the numeric counter, color-coded by speed:

- **Green:** High speed (≥ 100 WPM by default)
- **Yellow:** Mid speed (≥ 50 WPM by default)
- **Red:** Low speed (< 50 WPM)

Thresholds and colors are fully configurable.

### Battery Widget

Shows the battery level of each connected keyboard half (and optionally the dongle itself). Color reflects charge level:

- **Green:** ≥ 85%
- **Yellow:** 70–84%
- **Orange:** 40–69%
- **Red:** < 40%
- **Red X:** Disconnected peripheral

All thresholds are configurable.

## General Features

- **Custom Status Screen** — Combine and arrange widgets freely. Layout changes require a code edit and rebuild.
- **Deactivate Widgets via Config** — Disable any widget (WPM, battery, layer, etc.) with a single config line.
- **Ambient Light Sensor** — Automatically adjusts brightness based on ambient light. Tested with the `Adafruit APDS9960`. Enable via configuration.
- **Screen Toggle via Keyboard** — Toggle the display off/on (default: F22). When turned off, it automatically wakes on the next keystroke after the idle timeout expires.
- **Brightness Control** — Adjust brightness via keyboard shortcuts (default: F23 / F24).
- **Configurable Orientation** — Horizontal or vertical, with an optional flip to match your enclosure.
- **Idle Timeout** — Automatically turns off the display after a configurable period of inactivity. Turns back on at the next keystroke. Set to `0` to disable.
- **Color Customization** — All widget colors and threshold values are Kconfig options, overridable in your `.conf` file without touching C source code.

## Installation

**ZMK version compatibility:** YADS requires ZMK `0.3.0`. I haven't done any work on anything else yet.,

1. This guide assumes you have already implemented a basic dongle setup as described [here](https://zmk.dev/docs/development/hardware-integration/dongle).

2. Add this repository to your `west.yaml`:

   ```yaml
   manifest:
     remotes:
       - name: zmkfirmware
         url-base: https://github.com/zmkfirmware
       - name: albertclee
         url-base: https://github.com/alee0729
     projects:
       - name: zmk
         remote: zmkfirmware
         revision: 0.3.0
         import: app/west.yml
       - name: zmk-dongle-screen
         remote: albertclee
         revision: main
     self:
       path: config
   ```

3. Add the shield to your dongle's `build.yaml`:

   ```yaml
   include:
     - board: seeeduino_xiao_ble
       shield: [YOUR_CONFIGURED_DONGLE] dongle_screen
       artifact-name: dongle-screen
   ```

4. Configure keyboard halves as peripherals:

   ```yaml
   include:
     - board: seeeduino_xiao_ble
       shield: split_left
       cmake-args: -DCONFIG_ZMK_SPLIT=y -DCONFIG_ZMK_SPLIT_ROLE_CENTRAL=n
       artifact-name: split-dongle-left
     - board: seeeduino_xiao_ble
       shield: split_right
       cmake-args: -DCONFIG_ZMK_SPLIT=y -DCONFIG_ZMK_SPLIT_ROLE_CENTRAL=n
       artifact-name: split-dongle-right
   ```

5. Set your desired options in `[YOUR_CONFIGURED_DONGLE].conf` (see table below).

### Full build.yaml example

```yaml
include:
  - board: seeeduino_xiao_ble
    shield: totem_left
    cmake-args: -DCONFIG_ZMK_SPLIT=y -DCONFIG_ZMK_SPLIT_ROLE_CENTRAL=n
    artifact-name: totem-dongle-left
  - board: seeeduino_xiao_ble
    shield: totem_right
    cmake-args: -DCONFIG_ZMK_SPLIT=y -DCONFIG_ZMK_SPLIT_ROLE_CENTRAL=n
    artifact-name: totem-dongle-right
  - board: seeeduino_xiao_ble
    shield: totem_dongle dongle_screen
    artifact-name: totem-dongle-screen
  - board: seeeduino_xiao_ble
    shield: settings_reset
    artifact-name: totem-settings-reset
```

## Configuration Options

All options can be set in your dongle's `.conf` file. Colors are specified as RGB hex values (e.g. `0xff0000` for red).

### Display & Orientation

| Name | Type | Default | Description |
|------|------|---------|-------------|
| `CONFIG_DONGLE_SCREEN_HORIZONTAL` | bool | `y` | Horizontal screen orientation (landscape). |
| `CONFIG_DONGLE_SCREEN_FLIPPED` | bool | `n` | Flip the screen orientation. |

### Widgets

| Name | Type | Default | Description |
|------|------|---------|-------------|
| `CONFIG_DONGLE_SCREEN_WPM_ACTIVE` | bool | `y` | Enable the WPM widget. |
| `CONFIG_DONGLE_SCREEN_MODIFIER_ACTIVE` | bool | `y` | Enable the modifier key widget. |
| `CONFIG_DONGLE_SCREEN_LAYER_ACTIVE` | bool | `y` | Enable the layer widget. |
| `CONFIG_DONGLE_SCREEN_OUTPUT_ACTIVE` | bool | `y` | Enable the USB/BLE output widget. |
| `CONFIG_DONGLE_SCREEN_BATTERY_ACTIVE` | bool | `y` | Enable the battery widget. |
| `CONFIG_DONGLE_SCREEN_SYSTEM_ICON` | int | `0` | GUI key icon: `0` = macOS, `1` = Linux, `2` = Windows. |

### WPM Colors & Thresholds

| Name | Type | Default | Description |
|------|------|---------|-------------|
| `CONFIG_DONGLE_SCREEN_WPM_HIGH_THRESHOLD` | int | `100` | WPM ≥ this value shows the high color. |
| `CONFIG_DONGLE_SCREEN_WPM_MID_THRESHOLD` | int | `50` | WPM ≥ this value shows the mid color. |
| `CONFIG_DONGLE_SCREEN_WPM_COLOR_HIGH` | hex | `0x00ff00` | WPM title color for high speed (default: green). |
| `CONFIG_DONGLE_SCREEN_WPM_COLOR_MID` | hex | `0xffff00` | WPM title color for mid speed (default: yellow). |
| `CONFIG_DONGLE_SCREEN_WPM_COLOR_LOW` | hex | `0xff0000` | WPM title color for low speed (default: red). |

### Output (USB/BLE) Colors

| Name | Type | Default | Description |
|------|------|---------|-------------|
| `CONFIG_DONGLE_SCREEN_USB_COLOR_CONNECTED` | hex | `0x800080` | USB label color when HID-ready (default: purple). |
| `CONFIG_DONGLE_SCREEN_USB_COLOR_DISCONNECTED` | hex | `0xff0000` | USB label color when not ready (default: red). |
| `CONFIG_DONGLE_SCREEN_BLE_COLOR` | hex | `0x0000ff` | BLE label color (default: blue). |

### Layer Colors

| Name | Type | Default | Description |
|------|------|---------|-------------|
| `CONFIG_DONGLE_SCREEN_LAYER_BASE_COLOR` | hex | `0xffffff` | Layer label color for the base layer (index 0, default: white). |
| `CONFIG_DONGLE_SCREEN_LAYER_OTHER_COLOR` | hex | `0xffff00` | Layer label color for all other layers (default: yellow). |

### Battery Color Thresholds

| Name | Type | Default | Description |
|------|------|---------|-------------|
| `CONFIG_DONGLE_SCREEN_BATTERY_GREEN_THRESHOLD` | int | `85` | Battery level (%) at or above which the indicator is green. |
| `CONFIG_DONGLE_SCREEN_BATTERY_YELLOW_THRESHOLD` | int | `70` | Battery level (%) at or above which the indicator is yellow. |
| `CONFIG_DONGLE_SCREEN_BATTERY_ORANGE_THRESHOLD` | int | `40` | Battery level (%) at or above which the indicator is orange. Below this is red. Disconnected shows a red X. |

### Brightness

| Name | Type | Default | Description |
|------|------|---------|-------------|
| `CONFIG_DONGLE_SCREEN_MAX_BRIGHTNESS` | int | `80` | Maximum brightness (1–100). Used at power-on and as the dimmer ceiling. |
| `CONFIG_DONGLE_SCREEN_MIN_BRIGHTNESS` | int | `1` | Minimum brightness (1–99). Floor for keyboard and sensor adjustments. |
| `CONFIG_DONGLE_SCREEN_DEFAULT_BRIGHTNESS` | int | `MAX` | Startup brightness. Must be between MIN and MAX. |
| `CONFIG_DONGLE_SCREEN_BRIGHTNESS_KEYBOARD_CONTROL` | bool | `y` | Allow brightness control via keyboard. |
| `CONFIG_DONGLE_SCREEN_BRIGHTNESS_UP_KEYCODE` | int | `115` | Keycode to increase brightness (default: F24). |
| `CONFIG_DONGLE_SCREEN_BRIGHTNESS_DOWN_KEYCODE` | int | `114` | Keycode to decrease brightness (default: F23). |
| `CONFIG_DONGLE_SCREEN_BRIGHTNESS_STEP` | int | `10` | Brightness change per keypress. |
| `CONFIG_DONGLE_SCREEN_BRIGHTNESS_MODIFIER` | int | `0` | Startup brightness offset (useful with ambient light sensor). |
| `CONFIG_DONGLE_SCREEN_TOGGLE_KEYCODE` | int | `113` | Keycode to toggle the display on/off (default: F22). |
| `CONFIG_DONGLE_SCREEN_IDLE_TIMEOUT_S` | int | `600` | Seconds of inactivity before the display turns off. `0` = never. |

### Ambient Light Sensor

| Name | Type | Default | Description |
|------|------|---------|-------------|
| `CONFIG_DONGLE_SCREEN_AMBIENT_LIGHT` | bool | `n` | Enable ambient light sensor (tested: Adafruit APDS9960). |
| `CONFIG_DONGLE_SCREEN_AMBIENT_LIGHT_EVALUATION_INTERVAL_MS` | int | `1000` | How often to read the sensor (ms). |
| `CONFIG_DONGLE_SCREEN_AMBIENT_LIGHT_MIN_RAW_VALUE` | int | `0` | Minimum raw sensor value to use (adjust for enclosure/plastic). |
| `CONFIG_DONGLE_SCREEN_AMBIENT_LIGHT_MAX_RAW_VALUE` | int | `100` | Maximum raw sensor value to use. |
| `CONFIG_DONGLE_SCREEN_AMBIENT_LIGHT_TEST` | bool | `n` | Mock the sensor for testing without hardware. |

## Example Configuration

```conf
# Orientation
CONFIG_DONGLE_SCREEN_HORIZONTAL=y
CONFIG_DONGLE_SCREEN_FLIPPED=n

# Brightness
CONFIG_DONGLE_SCREEN_MAX_BRIGHTNESS=90
CONFIG_DONGLE_SCREEN_MIN_BRIGHTNESS=10
CONFIG_DONGLE_SCREEN_DEFAULT_BRIGHTNESS=30
CONFIG_DONGLE_SCREEN_IDLE_TIMEOUT_S=300
CONFIG_DONGLE_SCREEN_BRIGHTNESS_STEP=5

# Ambient light
CONFIG_DONGLE_SCREEN_AMBIENT_LIGHT=y

# WPM — custom thresholds (optional)
CONFIG_DONGLE_SCREEN_WPM_HIGH_THRESHOLD=120
CONFIG_DONGLE_SCREEN_WPM_MID_THRESHOLD=60

# Battery — custom thresholds (optional)
CONFIG_DONGLE_SCREEN_BATTERY_GREEN_THRESHOLD=90
CONFIG_DONGLE_SCREEN_BATTERY_YELLOW_THRESHOLD=60
CONFIG_DONGLE_SCREEN_BATTERY_ORANGE_THRESHOLD=30

# Colors — override any default (optional)
CONFIG_DONGLE_SCREEN_BLE_COLOR=0x4488ff
CONFIG_DONGLE_SCREEN_USB_COLOR_CONNECTED=0xaa00aa
```

## Pairing

The battery widget assigns indicators left-to-right based on the order in which keyboard halves are paired. For split keyboards, always pair the **left half first**.

Recommended pairing sequence:

1. Switch off both keyboard halves.
2. Flash the dongle.
3. Disconnect the dongle.
4. Flash the left half.
5. Flash the right half.
6. Reconnect the dongle.
7. Power on the left half and wait for its battery indicator to appear on the dongle.
8. Power on the right half.

### Reset Dongle

If the battery indicators are swapped (left shows right side and vice versa), you need to clear the dongle's pairing data. Add a `settings_reset` entry to your `build.yaml`:

```yaml
include:
  - board: seeeduino_xiao_ble
    shield: settings_reset

  - board: nice_nano_v2
    shield: settings_reset
```

Flash the resulting `settings_reset-[board]-zmk.uf2`, then repeat the pairing sequence above.

## Development

To develop new features or change the screen layout, clone this repo and build locally. Refer to the [ZMK Local Toolchain](https://zmk.dev/docs/development/local-toolchain/build-flash) docs.

Example build command:

```
west build -p -s /workspaces/zmk/app \
  -d /workspaces/zmk-build-output/totem_dongle \
  -b seeeduino_xiao_ble \
  -S zmk-usb-logging \
  -- \
  -DZMK_CONFIG=/workspaces/zmk-config/config \
  -DSHIELD="totem_dongle dongle_screen" \
  -DZMK_EXTRA_MODULES=/workspaces/zmk-modules/zmk-dongle-screen/
```

_Note: a matching `SHIELD` entry must already exist in your `build.yaml`._

## Star History

[![Star History Chart](https://api.star-history.com/svg?repos=janpfischer/zmk-dongle-screen&type=date&legend=top-left)](https://www.star-history.com/#janpfischer/zmk-dongle-screen&type=date&legend=top-left)

## License

MIT License

---

_This project is part of the ZMK community and licensed under the MIT License._
