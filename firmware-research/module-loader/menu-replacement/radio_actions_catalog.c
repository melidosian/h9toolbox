#include "radio_actions.h"

const struct radio_action_info radio_actions[RADIO_ACTION_COUNT] = {
    {"channels", "CHANNELS / VFO", "CHANNEL SETTINGS", "Program memories and VFOs"},
    {"radioInfo", "RADIO INFO", "SETUP", "Model, band and firmware"},
    {"aprsInfo", "APRS INFO", "APRS", "Query modem and protocol"},
    {"manual", "USER MANUAL", "SETUP", "Manuals and video QR codes"},
    {"resetVfo", "RESET VFOS", "SETUP", "Restore VFO defaults"},
    {"resetChannels", "RESET CHANNELS", "SETUP", "Restore factory memories"},
    {"resetAnalog", "RESET ANALOG DATA", "SETUP", "Restore analog parameters"},
    {"resetAll", "RESET ALL SETTINGS", "SETUP", "Radio and Toolbox defaults"},
    {"resetAprs", "RESET APRS SETTINGS", "APRS identity", "Native and Toolbox APRS"},
    {"btInfo", "BLUETOOTH NAME / PIN", "Bluetooth", "Read name and pairing PIN"},
    {"btPairing", "BLUETOOTH PAIRING", "Bluetooth", "Find or reconnect a device"},
    {"btReset", "RESET BLUETOOTH", "Bluetooth", "Clear paired BT devices"},
    {"toneScan", "SEEK CTCSS / DCS", "VFO & scan", "Identify received sub-audio"},
    {"frequencyScan", "SCAN FREQUENCY", "VFO & scan", "Capture a nearby frequency"},
    {"rangeScan", "SCAN SAVED RANGE", "VFO & scan", "Capture within saved limits"},
    {"spectrum", "SPECTRUM", "VFO & scan", "Sweep, listen and use peak"},
};
