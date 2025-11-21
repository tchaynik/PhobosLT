#pragma once

#define SERIAL_BAUD 460800

// Only define DEBUG_OUT if not already defined via build flags
#ifndef DEBUG_OUT
#define DEBUG_OUT Serial
#endif

#ifdef DEBUG_OUT
#define DEBUG_INIT DEBUG_OUT.begin(SERIAL_BAUD); \
                   DEBUG_OUT.setTimeout(0);
#define DEBUG(...) do { \
    if (DEBUG_OUT && DEBUG_OUT.availableForWrite() > 64) { \
        DEBUG_OUT.printf(__VA_ARGS__); \
    } \
} while(0)
#define DEBUG_PRINT(str) do { \
    if (DEBUG_OUT && DEBUG_OUT.availableForWrite() > strlen(str)) { \
        DEBUG_OUT.print(str); \
    } \
} while(0)
#define DEBUG_PRINTLN(str) do { \
    if (DEBUG_OUT && DEBUG_OUT.availableForWrite() > strlen(str) + 2) { \
        DEBUG_OUT.println(str); \
    } \
} while(0)
#else
#define DEBUG_INIT
#define DEBUG(...)
#define DEBUG_PRINT(str)
#define DEBUG_PRINTLN(str)
#endif
