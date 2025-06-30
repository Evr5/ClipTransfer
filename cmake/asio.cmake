include(FetchContent)

FetchContent_Declare(
  asio
  GIT_REPOSITORY https://github.com/chriskohlhoff/asio.git
  GIT_TAG asio-1-28-0
)

FetchContent_MakeAvailable(asio)

if (NOT TARGET asio)
    add_library(asio INTERFACE)
    target_include_directories(asio INTERFACE ${asio_SOURCE_DIR}/asio/include)
endif() 

if (TARGET asio)
    target_include_directories(asio SYSTEM INTERFACE ${asio_SOURCE_DIR}/asio/include)

    if (CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
        target_compile_options(asio INTERFACE -Wno-deprecated-literal-operator)
    endif()

    set_target_properties(asio PROPERTIES COMPILE_WARNING_AS_ERROR OFF)
    target_compile_options(asio INTERFACE -w)
endif()