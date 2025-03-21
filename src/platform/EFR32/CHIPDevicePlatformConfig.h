/*
 *
 *    Copyright (c) 2020 Project CHIP Authors
 *    Copyright (c) 2019 Nest Labs, Inc.
 *
 *    Licensed under the Apache License, Version 2.0 (the "License");
 *    you may not use this file except in compliance with the License.
 *    You may obtain a copy of the License at
 *
 *        http://www.apache.org/licenses/LICENSE-2.0
 *
 *    Unless required by applicable law or agreed to in writing, software
 *    distributed under the License is distributed on an "AS IS" BASIS,
 *    WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *    See the License for the specific language governing permissions and
 *    limitations under the License.
 */

/**
 *    @file
 *          Platform-specific configuration overrides for the Chip Device Layer
 *          on EFR32 platforms using the Silicon Labs SDK.
 */

#pragma once

// ==================== Platform Adaptations ====================

#define CHIP_DEVICE_CONFIG_EFR32_NVM3_ERROR_MIN 0xB00000
#define CHIP_DEVICE_CONFIG_EFR32_BLE_ERROR_MIN 0xC00000

#define CHIP_DEVICE_CONFIG_ENABLE_WIFI_AP 0

#if defined(SL_WIFI)
#define CHIP_DEVICE_CONFIG_ENABLE_WIFI_STATION 1
#else
#define CHIP_DEVICE_CONFIG_ENABLE_WIFI_STATION 0
#if CHIP_ENABLE_OPENTHREAD

#define CHIP_DEVICE_CONFIG_ENABLE_THREAD 1
#define CHIP_DEVICE_CONFIG_ENABLE_THREAD_SRP_CLIENT 1
#define CHIP_DEVICE_CONFIG_ENABLE_THREAD_DNS_CLIENT 1
#define CHIP_DEVICE_CONFIG_ENABLE_THREAD_COMMISSIONABLE_DISCOVERY 1
#endif /* CHIP_ENABLE_OPENTHREAD */
#endif /* defined(SL_WIFI) */

#define CHIP_DEVICE_CONFIG_ENABLE_CHIPOBLE 1

#define CHIP_DEVICE_CONFIG_ENABLE_CHIP_TIME_SERVICE_TIME_SYNC 0

#if defined(RS911X_WIFI)

#if defined(WIFI_IPV4_DISABLED)
#define CHIP_DEVICE_CONFIG_ENABLE_IPV4 0
#define CHIP_DEVICE_CONFIG_ENABLE_IPV6 1
#else
#define CHIP_DEVICE_CONFIG_ENABLE_IPV4 1
#define CHIP_DEVICE_CONFIG_ENABLE_IPV6 1
#endif

#endif

// ========== Platform-specific Configuration =========

// These are configuration options that are unique to the EFR32 platform.
// These can be overridden by the application as needed.

// ========== Platform-specific Configuration Overrides =========

#ifndef CHIP_DEVICE_CONFIG_BLE_LL_TASK_PRIORITY
#define CHIP_DEVICE_CONFIG_BLE_LL_TASK_PRIORITY (configTIMER_TASK_PRIORITY - 1)
#endif // CHIP_DEVICE_CONFIG_BLE_LL_TASK_PRIORITY

#ifndef CHIP_DEVICE_CONFIG_BLE_STACK_TASK_PRIORITY
#define CHIP_DEVICE_CONFIG_BLE_STACK_TASK_PRIORITY (CHIP_DEVICE_CONFIG_BLE_LL_TASK_PRIORITY - 1)
#endif // CHIP_DEVICE_CONFIG_BLE_STACK_TASK_PRIORITY

#ifndef CHIP_DEVICE_CONFIG_BLE_APP_TASK_PRIORITY
#define CHIP_DEVICE_CONFIG_BLE_APP_TASK_PRIORITY (CHIP_DEVICE_CONFIG_BLE_STACK_TASK_PRIORITY - 1)
#endif // CHIP_DEVICE_CONFIG_BLE_STACK_TASK_PRIORITY

#ifndef CHIP_DEVICE_CONFIG_BLE_APP_TASK_STACK_SIZE
#define CHIP_DEVICE_CONFIG_BLE_APP_TASK_STACK_SIZE 1536
#endif // CHIP_DEVICE_CONFIG_BLE_APP_TASK_STACK_SIZE

#ifndef CHIP_DEVICE_CONFIG_CHIP_TASK_STACK_SIZE
#define CHIP_DEVICE_CONFIG_CHIP_TASK_STACK_SIZE (6 * 1024)
#endif // CHIP_DEVICE_CONFIG_CHIP_TASK_STACK_SIZE

#ifndef CHIP_DEVICE_CONFIG_THREAD_TASK_STACK_SIZE
#if defined(EFR32MG21)
#define CHIP_DEVICE_CONFIG_THREAD_TASK_STACK_SIZE (2 * 1024)
#else
#define CHIP_DEVICE_CONFIG_THREAD_TASK_STACK_SIZE (8 * 1024)
#endif
#endif // CHIP_DEVICE_CONFIG_THREAD_TASK_STACK_SIZE

#define CHIP_DEVICE_CONFIG_ENABLE_WIFI_TELEMETRY 0
#define CHIP_DEVICE_CONFIG_ENABLE_THREAD_TELEMETRY 0
#define CHIP_DEVICE_CONFIG_ENABLE_THREAD_TELEMETRY_FULL 0

#ifndef CHIP_DEVICE_CONFIG_BLE_APP_TASK_NAME
#define CHIP_DEVICE_CONFIG_BLE_APP_TASK_NAME "BLE_EVENT"
#endif // CHIP_DEVICE_CONFIG_BLE_APP_TASK_NAME
#define CHIP_DEVICE_CONFIG_LOG_PROVISIONING_HASH 0

#define CHIP_DEVICE_CONFIG_MAX_EVENT_QUEUE_SIZE 25
