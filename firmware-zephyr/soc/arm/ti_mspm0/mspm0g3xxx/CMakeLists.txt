# SPDX-License-Identifier: Apache-2.0

if(CONFIG_SOC_MSPM0G3507)
  zephyr_compile_definitions(-D__MSPM0G3507__)
endif()

zephyr_sources(soc.c)

if(DEFINED CONFIG_MSPM0_IMAGE_OUTPUT_TI_TXT)
  set(MSPM0_TI_TXT_NAME ${CONFIG_KERNEL_BIN_NAME}.txt)
  set_property(
    GLOBAL APPEND
    PROPERTY
      extra_post_build_commands
      COMMAND
      $ENV{CCS_BASE}/ccs/tools/compiler/ti-cgt-armllvm_3.2.0.LTS/bin/tiarmhex
      ${CONFIG_KERNEL_BIN_NAME}.elf
      --ti_txt
      --diag_wrap=off
      -o
      ${MSPM0_TI_TXT_NAME})
endif()
