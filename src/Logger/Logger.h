#ifndef SRC_LOGGER_H_
#define SRC_LOGGER_H_

// #define DBG_OUTPUT_PORT_SERIAL
#define DBG_OUTPUT_PORT_WEB
// #define DBG_OUTPUT_PORT_BOTH

#ifdef DBG_OUTPUT_PORT_SERIAL
#define DGB_STREAM Serial
#else
#include "BufferedLogger.h"
#define DGB_STREAM BufferedLogger::instance()
#endif

#ifndef DBG_OUTPUT_PORT_BOTH
#define DEBUG_PRINT(msg)       \
    {                          \
        DGB_STREAM.print(msg); \
    }
#define DEBUG_PRINTLN(msg)       \
    {                            \
        DGB_STREAM.println(msg); \
    }
#define DEBUG_PRINTF(...)                 \
    {                                     \
        DGB_STREAM.printf_P(__VA_ARGS__); \
    }
#else
#define DEBUG_PRINT(msg)                       \
    {                                          \
        Serial.print(msg);                     \
        BufferedLogger::instance().print(msg); \
    }
#define DEBUG_PRINTLN(msg)                       \
    {                                            \
        Serial.println(msg);                     \
        BufferedLogger::instance().println(msg); \
    }
#define DEBUG_PRINTF(...)                                 \
    {                                                     \
        Serial.printf_P(__VA_ARGS__);                     \
        BufferedLogger::instance().printf_P(__VA_ARGS__); \
    }
#endif

#endif  // SRC_LOGGER_H_