/* Generated from stock-menu-v1.0.38.json; do not edit by hand. */
#ifndef STOCK_MENU_CATALOG_H
#define STOCK_MENU_CATALOG_H

struct radio_category { const char *name; const char *const *items; unsigned count; };
enum { RADIO_CATEGORY_COUNT = 10, RADIO_ITEM_COUNT = 110 };

static const char *const radio_items_0[] = {
    "Squelch Lv",
    "Step Freq",
    "Vox Level",
    "Vox Delay",
    "TOT",
    "Roger Beep",
    "Keypad Beep",
    "Power Save",
    "Keypad Lock",
    "Dual Watch",
    "Voice",
    "Backlight",
    "Brightness",
    "Pwr On Disp",
    "Display",
    "ANI-Edit",
    "Disp Type-A",
    "Disp Type-B",
    "DTMF SIDE",
    "PTT-Delay",
    "Language",
    "Alarm Mode",
    "STE",
    "Tone Burst",
    "Talk Around",
    "FM Interrupt",
    "PF1 S Press",
    "PF1 L Press",
    "PF2 S Press",
    "PF2 L Press",
    "Top S Press",
    "Top L Press",
    "Factory R",
    "Breath LED",
    "Mic Gain",
    "DTMF Speed",
    "DCD",
    "D-Hold",
    "D-Rsp",
    "200TX",
    "350TX",
    "500TX",
    "Menu Color",
};
static const char *const radio_items_1[] = {
    "RX Sub-Audio",
    "TX Sub-Audio",
    "Bandwidth",
    "TX Power",
    "Busy Lock",
    "Call Code",
    "PTT-ID",
    "Scan Add",
    "Scramble",
    "Offset",
    "Set-D",
    "Memory CHL",
    "Delete CHL",
    "Hopping",
    "CH-Name",
    "Modulation",
};
static const char *const radio_items_2[] = {
    "Freq Cal",
    "AM Vol Level",
};
static const char *const radio_items_3[] = {
    "Radio Info",
    "APRS Info",
    "User Manual",
};
static const char *const radio_items_4[] = {
    "Scan Mode",
    "Seek DCS",
    "Hang Time",
    "Scan Freq",
    "Freq Ranger",
    "Spectrum",
};
static const char *const radio_items_5[] = {
    "BT On/Off",
    "BT Mode",
    "BT Name",
    "BT Pairing",
    "OD PTT",
    "OD Mode",
    "BT Int Mic",
    "BT Int Spk",
    "BT Mic Gain",
    "BT Spk Gain",
    "BT PIN Code",
    "BT Reset",
};
static const char *const radio_items_6[] = {
    "SMS On/Off",
    "Send SMS",
    "Inbox",
    "Sent",
    "Draft",
    "SMS Mode",
    "RX Filter",
    "Statistics",
    "Clear",
};
static const char *const radio_items_7[] = {
    "GPS On/Off",
    "GPS Info",
    "Position",
    "Time Zone",
    "Speed Unit",
    "Dist Unit",
    "Alt Unit",
    "Fix Lon",
    "Fix Lat",
    "Fix Alt",
};
static const char *const radio_items_8[] = {
    "Fix Coord",
    "GPS Coord",
};
static const char *const radio_items_9[] = {
    "APRS Switch",
    "Beacon Set",
    "Beacon Type",
    "Relay Set",
    "Advanced Set",
    "Beacon List",
    "APRS Reset",
};

static const struct radio_category radio_categories[RADIO_CATEGORY_COUNT] = {
    { "Radio Setting", radio_items_0, sizeof radio_items_0 / sizeof *radio_items_0 },
    { "Program Chann", radio_items_1, sizeof radio_items_1 / sizeof *radio_items_1 },
    { "Analog Set", radio_items_2, sizeof radio_items_2 / sizeof *radio_items_2 },
    { "Radio Info", radio_items_3, sizeof radio_items_3 / sizeof *radio_items_3 },
    { "Scan", radio_items_4, sizeof radio_items_4 / sizeof *radio_items_4 },
    { "Bluetooth", radio_items_5, sizeof radio_items_5 / sizeof *radio_items_5 },
    { "SMS", radio_items_6, sizeof radio_items_6 / sizeof *radio_items_6 },
    { "GNSS", radio_items_7, sizeof radio_items_7 / sizeof *radio_items_7 },
    { "Site Type", radio_items_8, sizeof radio_items_8 / sizeof *radio_items_8 },
    { "APRS", radio_items_9, sizeof radio_items_9 / sizeof *radio_items_9 },
};

#endif
