#ifndef RADIO_33CM_H
#define RADIO_33CM_H

/* 10 Hz units. The state word is cleared by ml_aprs_boot. */
#define RADIO_33CM_LOW 90200000
#define RADIO_33CM_HIGH 92800000
#define RADIO_33CM_STATE 0x299f0
#define RADIO_33CM_MAGIC 0x33335458
#define RADIO_33CM_REGION 0x12b9c
#define RADIO_33CM_SETTING 0x6002

static inline int radio_33cm_contains(unsigned frequency)
{
    return frequency >= RADIO_33CM_LOW && frequency <= RADIO_33CM_HIGH;
}

#endif
