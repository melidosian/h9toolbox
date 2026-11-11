"""Stand-ins for the CHIRP API, so the TD-H9 module can be imported and its upload run against simulated radios."""
import importlib.util, sys, types, os


class Anything:
    """Absorbs every use of the CHIRP API the module makes while it is being imported."""
    def __init__(self, *a, **k): pass
    def __call__(self, *a, **k): return Anything()
    def __getattr__(self, n): return Anything()
    def __add__(self, o): return Anything()
    __radd__ = __add__
    def __mod__(self, o): return Anything()
    def __iter__(self): return iter(())
    def __getitem__(self, i): return Anything()

class Mod(types.ModuleType):
    def __getattr__(self, n):
        if n.startswith("__"): raise AttributeError(n)
        return Anything()

class RadioError(Exception): pass
class RadioNoResponse(RadioError): pass

def load(path):
    chirp = Mod("chirp"); chirp.__path__ = []
    sub = {}
    for name in ("bitwise", "bandplan_na", "checksum", "chirp_common", "directory", "errors", "memmap", "platform", "util", "settings"):
        sub[name] = Mod("chirp." + name); setattr(chirp, name, sub[name]); sys.modules["chirp." + name] = sub[name]
    sys.modules["chirp"] = chirp
    sub["errors"].RadioError = RadioError; sub["errors"].RadioNoResponse = RadioNoResponse
    sub["chirp_common"].CloneModeRadio = type("CloneModeRadio", (), {})
    sub["chirp_common"].CHARSET_ALPHANUMERIC = "abc"
    sub["directory"].register = lambda c: c
    sub["directory"].detected_by = lambda c: (lambda d: d)
    sub["directory"].register_format = lambda *a, **k: "fmt"
    sub["checksum"].checksum_8bit = lambda data: sum(data) & 255
    sub["platform"].get_platform = lambda: types.SimpleNamespace(is_ble_serial=lambda pipe: False)
    sub["bandplan_na"].ALL_GMRS_FREQS = []
    sub["util"].hexprint = lambda b: bytes(b).hex(" ")
    sub["settings"].__dict__.update({n: Anything for n in ("RadioSetting", "RadioSettings", "RadioSettingValueFloat", "RadioSettingGroup", "RadioSettingSubGroup", "RadioSettingValueBoolean", "RadioSettingValueInvertedBoolean", "RadioSettingValueInteger", "RadioSettingValueList", "RadioSettingValueMap", "RadioSettingValueString", "MemSetting")})
    sys.modules["chirp.settings"] = sub["settings"]
    spec = importlib.util.spec_from_file_location("tdh8_under_test", path)
    mod = importlib.util.module_from_spec(spec); spec.loader.exec_module(mod)
    return mod
