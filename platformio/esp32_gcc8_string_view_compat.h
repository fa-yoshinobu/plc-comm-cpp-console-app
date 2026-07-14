#pragma once

// The ESP32 Arduino 2.0.x GCC 8 standard library provides std::string_view but
// advertises the early 201603 feature-test value. mcprotocol-serial-cpp uses a
// 201606 cutoff for its fallback, so normalize the value after loading the
// actual standard header. This app-local shim avoids modifying either library.
#if __cplusplus >= 201703L
#include <string_view>
#if defined(__cpp_lib_string_view) && (__cpp_lib_string_view == 201603L)
#undef __cpp_lib_string_view
#define __cpp_lib_string_view 201606L
#endif
#endif
