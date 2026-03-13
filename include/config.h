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

#endif /* CONFIG_H */
