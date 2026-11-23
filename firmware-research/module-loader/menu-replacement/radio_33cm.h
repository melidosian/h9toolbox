#ifndef RADIO_33CM_H
#define RADIO_33CM_H

/* Frequencies are in 10 Hz units. The main loop restores the VM setting once. */
#define RADIO_33CM_LOW 90200000
#define RADIO_33CM_HIGH 92800000
#define RADIO_33CM_STATE 0x299f0
#define RADIO_33CM_MAGIC 0x33335458
#define RADIO_33CM_OFF 0x33330000
#define RADIO_33CM_REGION 0x12b9c
#define RADIO_33CM_SETTING 0x6002
#define RADIO_33CM_VM_KEY 250

static inline int radio_33cm_contains(unsigned frequency)
{
    return frequency >= RADIO_33CM_LOW && frequency <= RADIO_33CM_HIGH;
}

#endif
