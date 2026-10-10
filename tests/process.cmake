# SPDX-License-Identifier: BSD-2-Clause OR Apache-2.0
execute_process(COMMAND "${LAUNCHER}" "${PROBE}" "" "two words" "a\"b" "tail\\" "@opaque.rsp" "é" RESULT_VARIABLE status)
if(NOT status EQUAL 37)
  message(FATAL_ERROR "Argument or exit-status forwarding failed: ${status}")
endif()
