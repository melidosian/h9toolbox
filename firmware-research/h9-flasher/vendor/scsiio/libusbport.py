"""macOS SCSI port: bulk-only transport over libusb for the BR23 update device.

Written for this project; not part of jl-uboot-tool. The device path is the
USB VID:PID, for example "4C4A:2342". The transport in usb_msc.py is a copy of
firmware-research/resource-usb/usb_msc.py, which carried INQUIRY,
WRITE_MEMORY, READ_MEMORY, and JUMP_TO_MEMORY to a TD-H9 on 2026-11-08.
Residue is counted from the bytes actually carried, as the Windows port does;
the BR23 ROM's CSW residue field is not meaningful. Run as root on macOS so
libusb can detach the system mass-storage driver.
"""

from scsiio.common import SCSIDevBase, SCSIException
from scsiio import usb_msc


class SCSIDev(SCSIDevBase):
    def open(self, path):
        vid, pid = usb_msc.parse_device(path)
        try:
            self.dev = usb_msc.BulkOnlyDevice(vid, pid)
        except usb_msc.TransportError as error:
            raise SCSIException(str(error)) from error
        self.is_open = True

    def close(self):
        if self.is_open:
            self.dev.close()
            self.is_open = False

    def execute(self, cdb, data_out, data_in, max_sense_len=32, return_sense_buffer=False):
        try:
            return self.dev.execute(cdb, data_out, data_in, max_sense_len, return_sense_buffer)
        except usb_msc.TransportError as error:
            raise SCSIException(str(error)) from error
