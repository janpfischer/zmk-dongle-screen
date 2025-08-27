/*
 * Copyright (c) 2024 The ZMK Contributors
 *
 * SPDX-License-Identifier: MIT
 */

#pragma once

/**
 * @brief Wake the screen when a peripheral reconnects
 * Called by battery widget when it detects a peripheral reconnection
 */
void brightness_wake_screen_on_reconnect(void);