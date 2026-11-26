"""Audit every active factory menu row against compiled replacement endpoints."""
from pathlib import Path
import json
import re

ROOT = Path(__file__).resolve().parent
def settings(*ids): return {"settings": list(ids)}
def action(name): return {"actions": [name]}
def channel(field): return {"actions": ["channels"], "channel_fields": [field]}
def module(name): return {"modules": [name]}
def message(route, page):
    return {"modules": ["APRS", page, ".APRS STORE"], "message_route": route}

RETIRED_RF_SMS = {"retired": "rf_sms"}

# Same order as the pinned stock inventory, not the replacement's row order.
BINDINGS = {
    0: [settings("squelch"),settings("aStep","bStep"),settings("voxGain"),settings("voxDelay"),
        settings("timeout"),settings("roger"),settings("beep"),settings("powerSave"),settings("keyLock"),
        settings("dualWatchEnabled"),settings("voicePrompt"),settings("backlight"),settings("brightness"),
        settings("powerOnType","powerLine1","powerLine2","powerLine3"),settings("display"),settings("ani"),
        settings("aLabel"),settings("bLabel"),settings("sidetone"),settings("pttDelay"),settings("language"),
        settings("alarm"),settings("tailClean"),settings("repeaterTone"),settings("talkAround"),settings("fmInterrupt"),
        settings("pf1Short"),settings("pf1Long"),settings("pf2Short"),settings("pf2Long"),settings("topShort"),settings("topLong"),
        {"actions":["resetVfo","resetChannels","resetAnalog","resetAll"]},settings("breathLed"),settings("micGain"),
        settings("dtmfSpeed"),settings("decode"),settings("autoReset"),settings("decodeResponse"),settings("tx200"),
        settings("tx350"),settings("tx500"),settings("color")],
    1: [channel(2),channel(3),channel(4),channel(5),channel(6),settings("callCode"),channel(7),channel(13),channel(8),
        settings("aOffset","bOffset"),channel(11),
        {"actions":["channels"],"functions":["radio_channel_copy"]},
        {"actions":["channels"],"functions":["radio_channel_delete"]},channel(9),channel(12),channel(10)],
    2: [settings("calUhf","calVhf"),settings("amVolume")],
    3: [action("radioInfo"),action("aprsInfo"),action("manual")],
    4: [settings("scanMode"),action("toneScan"),settings("scanHang"),action("frequencyScan"),
        settings("rangerLow","rangerHigh"),action("spectrum")],
    5: [settings("btOn"),settings("btMode"),action("btInfo"),action("btPairing"),settings("odPtt"),settings("odMode"),
        settings("btInternalMic"),settings("btInternalSpeaker"),settings("btMic"),settings("btSpeaker"),action("btInfo"),action("btReset")],
    6: [RETIRED_RF_SMS, message("new", "APRS SEND"), message("inbox", "APRS MSG"),
        message("sent", "APRS MSG"), message("draft", "APRS SEND"),
        RETIRED_RF_SMS, RETIRED_RF_SMS, RETIRED_RF_SMS, RETIRED_RF_SMS],
    8: [settings("gnssOn"),module("GPS"),settings("positionFormat"),settings("timeZone"),settings("speedUnit"),
        settings("distanceUnit"),settings("altitudeUnit"),settings("fixedLongitude","longitudeSide"),
        settings("fixedLatitude","latitudeSide"),settings("fixedAltitude")],
    9: [settings("siteType"),settings("siteType")],
    10: [settings("aprsOn"),settings("call","ssid","symbolTable","symbol","customInfo","route1","route1Count","route2","route2Count"),
         settings("micE","micType"),settings("forwardChannel","digi1On","digi1Name","digi2On","digi2Name","forwardWait","remotePassword"),
         {"settings":["reportVoltage","reportSats","reportMileage","beaconInterval","rxChannel","txChannel","pttPriority","rxPopup"],
          "controls":["beaconType"]},
         module("APRS"),action("resetAprs")],
}

def validate(setting_ids=None, action_ids=None, module_ids=None):
    inventory = json.loads((ROOT / "stock-menu-v1.0.38.json").read_text())
    if setting_ids is None:
        setting_ids = set(re.findall(r'^\s*\{ "([^"]+)", "[^"]+", \d+,', (ROOT / "radio_settings_catalog.h").read_text(), re.M))
    if action_ids is None:
        action_ids = set(re.findall(r'^\s*\{"([^"]+)",', (ROOT / "radio_actions_catalog.c").read_text(), re.M))
    if module_ids is None: module_ids = {"GPS", "APRS", ".APRS STORE", "APRS MSG", "APRS SEND"}
    rows, disabled, retired = [], [], []
    for category in inventory["stock_root_categories"]:
        if category["disabled_in_dev"]:
            disabled.extend(category["items"])
            continue
        bindings = BINDINGS.get(category["id"], [])
        if len(bindings) != len(category["items"]): raise ValueError(f"Incomplete category {category['label']}")
        for label, binding in zip(category["items"], bindings):
            if not binding: raise ValueError(f"No replacement for {label}")
            if binding == RETIRED_RF_SMS:
                retired.append(label)
                continue
            for kind, available in (("settings",setting_ids),("actions",action_ids),("modules",module_ids)):
                missing = set(binding.get(kind, [])) - available
                if missing: raise ValueError(f"{category['label']}/{label}: missing {kind}: {sorted(missing)}")
            for control in binding.get("controls", []):
                if control != "beaconType" or "APRS_BEACON_TYPE" not in (ROOT.parent / "modules/menu/toolbox_menu.c").read_text():
                    raise ValueError(f"Missing control for {label}")
            for field in binding.get("channel_fields", []):
                if not 0 <= field < 15: raise ValueError(f"Invalid channel field for {label}")
            for function in binding.get("functions", []):
                if not re.search(r'\bint\s+'+re.escape(function)+r'\s*\(', (ROOT/"radio_channels.c").read_text()):
                    raise ValueError(f"Missing channel function {function}")
            rows.append({"category":category["label"],"native_row":label,**binding})
    active = len(rows) + len(retired)
    if active != 110 or len(retired) != 5 or len(disabled) != 9:
        raise ValueError("Pinned active menu inventory changed")
    return {"stock_sha256":inventory["stock_sha256"], "active_rows":active, "implemented_rows":len(rows),
            "disabled_morse_rows":disabled, "retired_rf_sms_rows":retired,
            "hardware_tested":False, "rows":rows}

if __name__ == '__main__': print(json.dumps(validate(), indent=2))
