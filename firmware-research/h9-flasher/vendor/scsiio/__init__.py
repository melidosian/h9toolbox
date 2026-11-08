import sys

if sys.platform == 'win32':
    from scsiio.win32port import SCSIDev

elif sys.platform == 'linux':
    from scsiio.linuxport import SCSIDev

elif sys.platform == 'darwin':
    # Added by this project: libusb bulk-only transport (see libusbport.py).
    from scsiio.libusbport import SCSIDev

else:
    raise ImportError("Platform '%s' is not supported (yet)" % sys.platform)
