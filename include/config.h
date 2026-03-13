/**
 * Hardware and build configuration (non-secret).
 * Secrets (WiFi, MQTT, OTA password) belong in private.h (copy from Example_Private.h).
 * Meter ID and frequency are also in private.h (GDO0, METER_YEAR, METER_SERIAL, FREQUENCY).
 */

#ifndef CONFIG_H
#define CONFIG_H

/* SPI pins for CC1101 - adjust for your board if different */
#ifdef ESP8266
#define SPI_CSK  PIN_SPI_SCK
#define SPI_MISO PIN_SPI_MISO
#define SPI_MOSI PIN_SPI_MOSI
#define SPI_SS   PIN_SPI_SS
#endif

#ifdef ESP32
#define SPI_CSK  SCK
#define SPI_MISO MISO
#define SPI_MOSI MOSI
#define SPI_SS   SS
#endif

/**
 * GDO0 sense: CC1101 drives GDO0 HIGH when sync word is detected (datasheet).
 * If the device worked before the FALSE fix and stopped after, try uncommenting
 * INVERT_GDO0_SENSE to use the previous (inverted) polarity for receive.
 */
#define INVERT_GDO0_SENSE 1
#ifndef INVERT_GDO0_SENSE
#define GDO0_SIGNAL_LEVEL  HIGH
#else
#define GDO0_SIGNAL_LEVEL  LOW
#endif

#endif /* CONFIG_H */
