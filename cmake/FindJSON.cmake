find_path(JSON_INCLUDE_DIR nlohmann/json.hpp
    HINTS
    /usr/include
    /usr/local/include
    ENV CMAKE_PREFIX_PATH
)

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(JSON DEFAULT_MSG JSON_INCLUDE_DIR)
