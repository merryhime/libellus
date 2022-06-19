find_program(XXD_COMMAND NAMES xxd)

# Usage:
#  add_resources(
#    name                       # Name of created target
#    NAMESPACE <namespace>      # Namespace of created symbols (optional)
#    HEADER_PATH <dir>          # Include path of generated headers (optional)
#    FILES <files [...]>        # Files to include
#  )
function(add_resources NAME)

    set(OPTIONS "")
    set(ONE_VALUE_ARGS NAMESPACE HEADER_PATH)
    set(MULTI_VALUE_ARGS FILES)
    cmake_parse_arguments(ARGS "${OPTIONS}" "${ONE_VALUE_ARGS}" "${MULTI_VALUE_ARGS}" ${ARGN})

    if(NOT XXD_COMMAND)
        message(FATAL_ERROR "xxd not found")
    endif()

    if(NOT DEFINED ARGS_HEADER_PATH)
        set(ARGS_HEADER_PATH ".")
    endif()

    set(OUT_FILE_PATH "${CMAKE_CURRENT_BINARY_DIR}/resouces_output_dir/${ARGS_HEADER_PATH}/")

    add_library("${NAME}")
    target_include_directories("${NAME}" PUBLIC "${CMAKE_CURRENT_BINARY_DIR}/resouces_output_dir")
    target_compile_features("${NAME}" PUBLIC cxx_std_11)

    foreach(IN_FILE IN LISTS ARGS_FILES)
        get_filename_component(IN_FILE_BASE_NAME ${IN_FILE} NAME)
        string(REGEX REPLACE "\\." "_" SYMBOL_NAME "${IN_FILE_BASE_NAME}")

        set(FULL_IN_FILE_PATH "${CMAKE_CURRENT_LIST_DIR}/${IN_FILE}")
        file(SIZE "${FULL_IN_FILE_PATH}" IN_FILE_SIZE)

        set(OUT_HEADER_FILE "${OUT_FILE_PATH}/${SYMBOL_NAME}.hpp")
        set(OUT_SOURCE_FILE "${OUT_FILE_PATH}/${SYMBOL_NAME}.cpp")
        set(OUT_HEXINC_FILE "${OUT_FILE_PATH}/${SYMBOL_NAME}.inc")

        # Write header

        file(
            WRITE "${OUT_HEADER_FILE}"

            "#pragma once\n"
            "\n"
            "#include <array>\n"
            "\n"
        )

        if(DEFINED ARGS_NAMESPACE)
            file(
                APPEND "${OUT_HEADER_FILE}"

                "namespace ${ARGS_NAMESPACE} {\n"
                "\n"
            )
        endif()

        file(
            APPEND "${OUT_HEADER_FILE}"

            "extern const std::array<char, ${IN_FILE_SIZE}> ${SYMBOL_NAME};\n"
        )

        if(DEFINED ARGS_NAMESPACE)
            file(
                APPEND "${OUT_HEADER_FILE}"

                "\n"
                "}  // namespace ${ARGS_NAMESPACE}\n"
            )
        endif()

        list(APPEND OUT_HEADER_FILE_LIST "${OUT_HEADER_FILE}")

        # Write source

        file(
            WRITE "${OUT_SOURCE_FILE}"

            "#include \"${ARGS_HEADER_PATH}/${SYMBOL_NAME}.hpp\"\n"
            "\n"
            "#include <array>\n"
            "\n"
        )

        if(DEFINED ARGS_NAMESPACE)
            file(
                APPEND "${OUT_SOURCE_FILE}"

                "namespace ${ARGS_NAMESPACE} {\n"
                "\n"
            )
        endif()

        file(
            APPEND "${OUT_SOURCE_FILE}"

            "const std::array<char, ${IN_FILE_SIZE}> ${SYMBOL_NAME} {\n"
            "#include \"${ARGS_HEADER_PATH}/${SYMBOL_NAME}.inc\"\n"
            "};\n"
        )

        if(DEFINED ARGS_NAMESPACE)
            file(
                APPEND "${OUT_SOURCE_FILE}"

                "\n"
                "}  // namespace ${ARGS_NAMESPACE}\n"
            )
        endif()

        list(APPEND OUT_SOURCE_FILE_LIST "${OUT_SOURCE_FILE}")

        # Write hex dump

        add_custom_command(
            OUTPUT "${OUT_HEXINC_FILE}"
            COMMAND "${XXD_COMMAND}" -i < "${FULL_IN_FILE_PATH}" > "${OUT_HEXINC_FILE}"
            DEPENDS "${FULL_IN_FILE_PATH}"
        )

        list(APPEND OUT_HEXINC_FILE_LIST "${OUT_HEXINC_FILE}")

    endforeach()

    target_sources("${NAME}" PUBLIC ${OUT_HEADER_FILE_LIST} ${OUT_SOURCE_FILE_LIST})

    add_custom_target("${NAME}_xxd" DEPENDS "${OUT_HEXINC_FILE}")
    add_dependencies("${NAME}" "${NAME}_xxd")

endfunction()
