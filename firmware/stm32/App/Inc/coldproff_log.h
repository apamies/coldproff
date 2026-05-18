/* coldproff_log.h — lightweight logging macros for STM32 build
 *
 * Define DEBUG before including to enable printf output (via SWO or UART).
 * In production builds, all log calls compile to nothing (zero overhead).
 */
#ifndef COLDPROFF_LOG_H
#define COLDPROFF_LOG_H

#ifdef DEBUG
  #include <stdio.h>
  #define LOG_I(tag, fmt, ...) printf("[" tag "] "     fmt "\r\n", ##__VA_ARGS__)
  #define LOG_E(tag, fmt, ...) printf("[ERR][" tag "] " fmt "\r\n", ##__VA_ARGS__)
  #define LOG_D(tag, fmt, ...) printf("[DBG][" tag "] " fmt "\r\n", ##__VA_ARGS__)
#else
  #define LOG_I(tag, fmt, ...) do {} while(0)
  #define LOG_E(tag, fmt, ...) do {} while(0)
  #define LOG_D(tag, fmt, ...) do {} while(0)
#endif

#endif /* COLDPROFF_LOG_H */
