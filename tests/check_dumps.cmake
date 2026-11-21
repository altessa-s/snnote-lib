# Copyright 2026 ALTESSA SOLUTIONS INC.
# Use of this source code is governed by an MIT-style license that can be
# found in the LICENSE file.

file(STRINGS "${SOURCE}/tests/expected/dumps.sha256" lines)
foreach(line IN LISTS lines)
    string(SUBSTRING "${line}" 0 64 expected)
    string(SUBSTRING "${line}" 66 -1 fixture)
    string(REGEX REPLACE "\r$" "" fixture "${fixture}")
    execute_process(COMMAND "${TOOL}" "${SOURCE}/tests/fixtures/${fixture}"
                    OUTPUT_FILE "${BINARY}/checked.dump" RESULT_VARIABLE result)
    if(NOT result EQUAL 0)
        message(FATAL_ERROR "dump failed: ${fixture}")
    endif()
    file(SHA256 "${BINARY}/checked.dump" actual)
    if(NOT actual STREQUAL expected)
        message(FATAL_ERROR "dump mismatch: ${fixture}")
    endif()
endforeach()
message(STATUS "All 32 canonical dumps match")
