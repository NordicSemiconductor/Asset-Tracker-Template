#
# Copyright (c) 2024 Nordic Semiconductor ASA
#
# SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
#

cmake_minimum_required(VERSION 3.20.0)

find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project("Asset Tracker Template")

# Include files that are common for all modules
add_subdirectory(src/common)

# Add main application source
target_sources(app PRIVATE src/main.c)

# Include mandatory module source folders
add_subdirectory(src/modules/network)
add_subdirectory(src/modules/cloud)
add_subdirectory(src/modules/location)
add_subdirectory(src/modules/fota)
add_subdirectory(src/modules/button)

# Optional modules
add_subdirectory_ifdef(CONFIG_APP_POWER src/modules/power)
add_subdirectory_ifdef(CONFIG_APP_ENVIRONMENTAL src/modules/environmental)
add_subdirectory_ifdef(CONFIG_APP_LED src/modules/led)
add_subdirectory_ifdef(CONFIG_APP_SHELL src/modules/shell)

if (CONFIG_NRF_CLOUD_COAP_SEC_TAG GREATER_EQUAL 2147483648 AND CONFIG_NRF_CLOUD_COAP_SEC_TAG LESS_EQUAL 2147483667)
	message(WARNING "CONFIG_NRF_CLOUD_COAP_SEC_TAG is set to a developer security tag. "
			"TLS traffic can now be decrypted with Nordic tools. "
			"This should only be used during development and testing.")
endif()
