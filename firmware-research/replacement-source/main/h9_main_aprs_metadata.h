#ifndef H9_MAIN_APRS_METADATA_H
#define H9_MAIN_APRS_METADATA_H

#include <stddef.h>
#include <stdint.h>

#include "h9_main_aprs_receive.h"

#define H9_MAIN_APRS_VERSION_CAPACITY 16u
#define H9_MAIN_GPS_BUFFER_CAPACITY 1024u

typedef struct {
    char module[H9_MAIN_APRS_VERSION_CAPACITY];
    char protocol[H9_MAIN_APRS_VERSION_CAPACITY];
    uint8_t has_module;
    uint8_t has_protocol;
} H9MainAprsVersions;

typedef struct {
    uint8_t sentence[H9_MAIN_GPS_BUFFER_CAPACITY];
    size_t length;
} H9MainGpsCollector;

typedef void (*H9MainGpsSentence)(void *context, const uint8_t *sentence,
                                  size_t length);

void h9_main_aprs_versions_init(H9MainAprsVersions *versions);
int h9_main_aprs_version_from_frame(const H9MainWireReceiver *receiver,
                                    H9MainAprsVersions *versions);
void h9_main_gps_init(H9MainGpsCollector *collector);
size_t h9_main_gps_feed(H9MainGpsCollector *collector, const uint8_t *data,
                        size_t length, H9MainGpsSentence on_sentence,
                        void *context);
size_t h9_main_aprs_gps_from_frame(H9MainGpsCollector *collector,
                                   const H9MainWireReceiver *receiver,
                                   H9MainGpsSentence on_sentence,
                                   void *context);
size_t h9_main_aprs_gpio_reply_from_frame(const H9MainWireReceiver *receiver,
                                           uint8_t *output, size_t capacity);

#endif
