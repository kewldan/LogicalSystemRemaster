execute_process(
        COMMAND git apply --check "${PATCH_FILE}"
        WORKING_DIRECTORY "${ENGINE_SOURCE_DIR}"
        RESULT_VARIABLE PATCH_CAN_APPLY
        OUTPUT_QUIET
        ERROR_QUIET)

if (PATCH_CAN_APPLY EQUAL 0)
    execute_process(
            COMMAND git apply --whitespace=nowarn "${PATCH_FILE}"
            WORKING_DIRECTORY "${ENGINE_SOURCE_DIR}"
            RESULT_VARIABLE PATCH_RESULT)
    if (NOT PATCH_RESULT EQUAL 0)
        message(FATAL_ERROR "Failed to apply the Engine compatibility patch")
    endif ()
else ()
    # FetchContent may keep a populated source tree between CMake runs. Treat
    # a cleanly reversible patch as already applied instead of failing.
    execute_process(
            COMMAND git apply --reverse --check "${PATCH_FILE}"
            WORKING_DIRECTORY "${ENGINE_SOURCE_DIR}"
            RESULT_VARIABLE PATCH_ALREADY_APPLIED
            OUTPUT_QUIET
            ERROR_QUIET)
    if (NOT PATCH_ALREADY_APPLIED EQUAL 0)
        message(FATAL_ERROR "Engine sources do not match cmake/engine-posix.patch")
    endif ()
endif ()
