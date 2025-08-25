# ZMK Dongle Screen YADS (Yet another Dongle Screen)

This project provides a Zephyr module for a dongle display shield based on the ST7789V display and the Seeeduino XAIO BLE microcontroller and the LVGL graphics library.  
The display can take advantage of a ambient light sensor to dim and brighten the display automatically.  
It offers various widgets for current output, displaying layer, mod, WPM, and battery status, as well as brightness adjustments via keyboard, automatic dimming after inactivity, and a customizable status screen for ZMK-based keyboards.

**This project is inspired by [prospector-zmk-module](https://github.com/carrefinho/prospector-zmk-module) and [zmk-dongle-display](https://github.com/englmaxi/zmk-dongle-display). Thanks for your awesome work!**

## Demo

![Sample Screen of zmk-dongle-screen](/docs/images/screen.jpg)

<https://github.com/user-attachments/assets/86c33af6-d83e-4e2a-9766-fc8836e896f1>

### Brightness changes with ambient light sensor and screen toggle

https://github.com/user-attachments/assets/3379f79c-af90-4763-8ba5-8a8f34fd66cf

## Building a dongle

To build a dongle yourself you can use the build guide by **carrefinho** ([prospector project](https://github.com/carrefinho/prospector)).  
nice!nano v2 supported. [Wiring guide](/docs/nice_nano_wire_guide.md).  
This repository only contains a module and no build guides or suggestions.

## Widgets Overview

This module provides several widgets to visualize the current state of your ZMK-based keyboard:

- **Output Widget**  
  Indicates the current output state of the keyboard (USB or BLE profiles). The currently used interface (USB or BLE) is indicated with an arrow.
  - **USB:**
    - **White:** USB HID is ready and active (dongle is connected to a computer and working as a keyboard).
    - **Red:** USB HID is not ready (dongle is powered, e.g. via wall plug or power bank, but not connected to a computer).
  - **BLE:**  
    For the currently selected Bluetooth profile (the number is shown in the next line):
    - **Green:** Connected (active BLE connection established)
    - **Blue:** Bonded (device is paired, but not currently connected)
    - **White:** Profile is free (no device paired or connected for this profile)

- **Layer Widget**  
  Displays the currently active keyboard layer. Useful for quickly identifying which layer is active.

- **Mod Widget**  
  Shows the status of modifier keys (e.g., Shift, Ctrl, Alt, GUI). Indicates which modifiers are currently pressed.

- **WPM Widget**  
  Displays the current words per minute (WPM) typing speed in real time.

- **Battery Widget**  
  Shows the battery level of the dongle and/or the keyboard, if supported.

## General Features

- **Custom Status Screen**  
  Combine and arrange widgets as you like for a fully customizable status display. (Code changes and recompiling are needed for this.)

- **Deactivate Screen Modules via configuration**  
  If you don't need a specific module to be shown (like WPM) you can simple disable them via configuration. No code changes are needed for this.

- **Ambient light sensor adjustment**
  This module supports ambient light sensors. Tested is the `Adafruit APDS9960` sensor.  
  Using the sensor allows to adjust the lightning to the ambient light level. This can be modified by the `Brightness Control` keys to apply a positive or negative modifier.  
  If you want to use this feature you'll have to enable it via configuration. Please refer to the configuration overview below.

- **Toggle the display via Keyboard**  
  Toggle the display off and on via keyboard shortcut. By default F22 is mapped to this. You'll just have to assign this in your keyboard keymap.  
  When the display is turned off via toggle and the idle timeout is reached the display will turn on once a new activity is recognized.

- **Brightness Control**  
  Adjust the display brightness via keyboard shortcuts. By default, F23 and F24 are mapped to this. You'll just have to assign this in your keyboard keymap.

- **Configurable Display Orientation**  
  Set the screen orientation to match your keyboard or desk setup (horizontal or vertical). Additionally, the screen can be flipped to match the orientation of the display in your casing.

- **Idle Timeout**  
  Automatically turns off or dims the display after a configurable period of inactivity (no keystrokes). It automatically turns on when the first keystroke is detected again.  
  The idle timeout can be set in seconds. If set to `0`, the display will never dim or turn off automatically.  
  When the idle timeout is reached, the display brightness will be set to 0.  
  When activity resumes, the brightness will be restored to the last value (up to `DONGLE_SCREEN_MAX_BRIGHTNESS`).  

## Installation

**ZMK version compatability**
YADS needs at least ZMK version `0.3.0` or revision `147c340c6e8d377304acfdd64dc86cf83ebdfef2` to be build. 

1. This guide assumes that you have already implemented a basic dongle setup as described [here](https://zmk.dev/docs/development/hardware-integration/dongle).
2. Once this is done, add this repository to your `west.yaml`.  
   Example:

   ```yaml
   manifest:
     remotes:
       - name: zmkfirmware
         url-base: https://github.com/zmkfirmware
       - name: janpfischer
         url-base: https://github.com/janpfischer
     projects:
       - name: zmk
         remote: zmkfirmware
         revision: main #or at least 147c340c6e8d377304acfdd64dc86cf83ebdfef2 or 0.3.0
         import: app/west.yml
       - name: zmk-dongle-screen
         remote: janpfischer
         revision: main
     self:
       path: config
   ```

   Note: If you want to pin the release of `zmk-dongle-screen` or `zmk` in general you can update the `revision` to use a tag or commit SHA.
  
   Example for using `zmk-dongle-screen` version 0.0.1:

   ```yaml
   - name: zmk-dongle-screen
     remote: janpfischer
     revision: 0.0.1
   ```

3. The shield must be included in your build configuration for the dongle you set up in step 1.  
   Example `build.yaml` snippet:

   ```yaml
   include:
     - board: seeeduino_xiao_ble
       shield: [YOUR_CONFIGURED_DONGLE] dongle_screen
       #cmake-args: -DCONFIG_LOG_PROCESS_THREAD_STARTUP_DELAY_MS=8000 #optional if logging is enabled
       #snippet: zmk-usb-logging #only enable for debugging
       artifact-name: dongle-screen
   ```

4. Keyboard splits must be configured as peripherals.  
   Example `build.yaml` snippet:

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

5. Adjust the desired configuration options in your `[YOUR_CONFIGURED_DONGLE].conf` (see table below).

### Configuration sample

A sample `build.yaml` based on `seeeduino_xiao_ble` boards for the keyboard and the dongle including a `settings_reset` firmware could look like this:

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
    cmake-args: -DCONFIG_LOG_PROCESS_THREAD_STARTUP_DELAY_MS=8000
    snippet: zmk-usb-logging
    artifact-name: totem-dongle-screen
  - board: seeeduino_xiao_ble
    shield: settings_reset
    artifact-name: totem-settings-reset
```

## Configuration Options

| Name                                                    | Type | Default | Description                                                                                                                                                      |
| -------------------------------------------             | ---- | ------- | ---------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| `CONFIG_DONGLE_SCREEN_HORIZONTAL`                              | bool | y       | Orientation of the screen. By default, it is horizontal (laying on the side).                                                                                    |
| `CONFIG_DONGLE_SCREEN_FLIPPED`                                 | bool | n       | Should the screen orientation be flipped in horizontal or vertical orientation?                                                                                  |
| `CONFIG_DONGLE_SCREEN_AMBIENT_LIGHT`                           | bool | n       | If enabled, the ambient light sensor will be used to automatically adjust screen brightness.                                                                     |
| `CONFIG_DONGLE_SCREEN_AMBIENT_LIGHT_EVALUATION_INTERVAL_MS`    | int  | 1000    | The interval how often the ambient light level should be evaluated.                                                                                              |
| `CONFIG_DONGLE_SCREEN_AMBIENT_LIGHT_MIN_RAW_VALUE`             | int  | 0       | Depending on the position and if the sensor is behind transparent plastic or not the sensor readings can be vary. Behind plastic the default value is proven good. If your ambient light changes are not too reactive you might change this. |
| `CONFIG_DONGLE_SCREEN_AMBIENT_LIGHT_MAX_RAW_VALUE`             | int  | 100     | Depending on the position and if the sensor is behind transparent plastic or not the sensor readings can be vary. Behind plastic the default value is proven good. If your ambient light changes are not too reactive you might change this. |
| `CONFIG_DONGLE_SCREEN_IDLE_TIMEOUT_S`                          | int  | 600     | Screen idle timeout in seconds (0 = never off). Time in seconds after which the screen turns off when idle.                                                      |
| `CONFIG_DONGLE_SCREEN_MAX_BRIGHTNESS`                          | int  | 80      | Maximum screen brightness (1-100). This is the brightness used when the dongle is powered on and the maximum used by the dimmer.                                 |
| `CONFIG_DONGLE_SCREEN_MIN_BRIGHTNESS`                          | int  | 1       | Minimum screen brightness (1-99). This is the brightness used as a minimum value for brightness adjustments with the modifier keys and the ambient light sensor. |
| `CONFIG_DONGLE_SCREEN_BRIGHTNESS_MODIFIER`                     | int  | 0       | The modifier to start the dongle with. Useful if you found a modifier comfortable for you. Espacially for ambient light. Otherwise no need to change.            |
| `CONFIG_DONGLE_SCREEN_TOGGLE_KEYCODE`                          | int  | 113     | Keycode that toggles the screen off and on (default: F22).                                                                                                       |
| `CONFIG_DONGLE_SCREEN_BRIGHTNESS_KEYBOARD_CONTROL`             | bool | y       | Allows controlling the screen brightness via keyboard (e.g., F23/F24).                                                                                           |
| `CONFIG_DONGLE_SCREEN_BRIGHTNESS_UP_KEYCODE`                   | int  | 115     | Keycode for increasing screen brightness (default: F24).                                                                                                         |
| `CONFIG_DONGLE_SCREEN_BRIGHTNESS_DOWN_KEYCODE`                 | int  | 114     | Keycode for decreasing screen brightness (default: F23).                                                                                                         |
| `CONFIG_DONGLE_SCREEN_BRIGHTNESS_STEP`                         | int  | 10      | Step for brightness adjustment with keyboard. How much brightness (range MIN_BRIGHTNESS to MAX_BRIGHTNESS) should be applied per keystroke.                      |
| `CONFIG_DONGLE_SCREEN_WPM_ACTIVE`                              | bool | y       | If the WPM Widget should be active or not.                                                                                                                       |
| `CONFIG_DONGLE_SCREEN_MODIFIER_ACTIVE`                         | bool | y       | If the Modifier Widget should be active or not.                                                                                                                  |
| `CONFIG_DONGLE_SCREEN_LAYER_ACTIVE`                            | bool | y       | If the Layer Widget should be active or not.                                                                                                                     |
| `CONFIG_DONGLE_SCREEN_OUTPUT_ACTIVE`                           | bool | y       | If the Output Widget should be active or not.                                                                                                                    |
| `CONFIG_DONGLE_SCREEN_BATTERY_ACTIVE`                          | bool | y       | If the Battery Widget should be active or not.                                                                                                                   |
| `CONFIG_DONGLE_SCREEN_AMBIENT_LIGHT_TEST`                      | bool | n       | If enabled, the ambient light sensor will be mocked to adjust screen brightness.                                                                                 |

## Example Configuration (`prj.conf`)

```conf
CONFIG_DONGLE_SCREEN_HORIZONTAL=y
CONFIG_DONGLE_SCREEN_FLIPPED=n
CONFIG_DONGLE_SCREEN_AMBIENT_LIGHT=y
CONFIG_DONGLE_SCREEN_IDLE_TIMEOUT_S=300
CONFIG_DONGLE_SCREEN_MAX_BRIGHTNESS=90
CONFIG_DONGLE_SCREEN_MIN_BRIGHTNESS=10
CONFIG_DONGLE_SCREEN_BRIGHTNESS_KEYBOARD_CONTROL=y
CONFIG_DONGLE_SCREEN_BRIGHTNESS_UP_KEYCODE=115
CONFIG_DONGLE_SCREEN_BRIGHTNESS_DOWN_KEYCODE=114
CONFIG_DONGLE_SCREEN_BRIGHTNESS_STEP=5
```

## Pairing

The battery widget assigns the battery indicators from left to right, based on the sequence in which the keyboard halves are paired to the dongle.

For split keyboards, it is essential to pair the left half first after flashing the dongle, followed by the right half. This ensures the correct mapping of battery status indicators and avoids swapped displays in the widget.

The recommended procedure is as follows:

1. Switch off both keyboard halves.
2. Flash the dongle
3. Disconnect the dongle
4. Flash the left half
5. Flash the right half
6. Reconnect the dongle
7. Switch on the left half and wait until the battery indicator appears on the dongle
8. Switch on the right half

### Reset Dongle

If the dongle has already been paired with both keyboard halves and the battery widget displays swapped indicators (i.e., the left battery indicator refers to the right keyboard half), a full reset of the dongle is required.

To achieve this, an appropriate configuration for the specific microcontroller must be added to the `build.yaml` in order to generate a `settings_reset-[microcontroller-name]-zmk.uf2` image. This image enables the complete removal of all stored pairing data from the dongle.

```yaml
  include:
...
  - board: seeeduino_xiao_ble
    shield: settings_reset

  - board: nice_nano_v2
    shield: settings_reset
...
```

After flashing the reset file, the pairing process should be repeated in the sequence described above to ensure correct mapping of the battery indicators.

## Development

If you want to develop new features or change the layout of the screen you'll have to clone this repo and build it on your own.  
Refer to the [ZMK Local toolchain](https://zmk.dev/docs/development/local-toolchain/build-flash) documentation for this.

A command for building locally _can_ look something like this:

```
west build -p -s /workspaces/zmk/app -d "/workspaces/zmk-build-output/totem_dongle" -b "seeeduino_xiao_ble" -S zmk-usb-logging -- -DZMK_CONFIG=/workspaces/zmk-config/config -DSHIELD="totem_dongle dongle_screen" -DZMK_EXTRA_MODULES=/workspaces/zmk-modules/zmk-dongle-screen/
```

_Note: a matching entry for `-DSHIELD` must already be present in your `build.yaml` in your configuration, which is given as the `-DZMK_CONFIG` argument._

## License

MIT License

---

_This project is part of the ZMK community and licensed under the MIT License._
