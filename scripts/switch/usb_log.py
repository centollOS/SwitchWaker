#!/usr/bin/env -S uv run --script
# /// script
# requires-python = ">=3.10"
# dependencies = ["pyusb==1.3.1"]
# ///
"""Print a Switch probe's live log sent over USB (libnx usbComms, 057e:3000).

Run on the host, not in a container (Docker Desktop has no USB access):

    uv run scripts/switch/usb_log.py [--out FILE]

Needs libusb (macOS: brew install libusb). Start it before or after launching
the probe; it waits for the console, prints everything it receives, and waits
again when the probe exits. Ctrl+C stops it.
"""

import argparse
import sys
import time

import usb.core
import usb.util

VENDOR_ID = 0x057E
PRODUCT_ID = 0x3000  # libnx usbComms default; 0x2000 is the console's own mode


def find_in_endpoint(device):
    device.set_configuration()
    interface = device.get_active_configuration()[(0, 0)]
    endpoint = usb.util.find_descriptor(
        interface,
        custom_match=lambda e: usb.util.endpoint_direction(e.bEndpointAddress)
        == usb.util.ENDPOINT_IN,
    )
    if endpoint is None:
        raise RuntimeError("usbComms interface has no IN endpoint")
    return endpoint


def stream(device, out):
    endpoint = find_in_endpoint(device)
    print("[usb_log] connected; streaming", file=sys.stderr, flush=True)
    while True:
        try:
            data = endpoint.read(0x1000, timeout=500)
        except usb.core.USBTimeoutError:
            continue
        text = bytes(data).decode("utf-8", errors="replace")
        sys.stdout.write(text)
        sys.stdout.flush()
        if out is not None:
            out.write(text)
            out.flush()


def main():
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("--out", help="also append the log to this file")
    args = parser.parse_args()
    out = open(args.out, "a", encoding="utf-8") if args.out else None

    print("[usb_log] waiting for a probe on 057e:3000 ...", file=sys.stderr, flush=True)
    try:
        while True:
            device = usb.core.find(idVendor=VENDOR_ID, idProduct=PRODUCT_ID)
            if device is None:
                time.sleep(0.25)
                continue
            try:
                stream(device, out)
            except usb.core.USBError as error:
                # The probe exited or the cable was pulled; wait for the next run.
                print(f"\n[usb_log] disconnected ({error}); waiting ...",
                      file=sys.stderr, flush=True)
                usb.util.dispose_resources(device)
                time.sleep(0.5)
    except KeyboardInterrupt:
        pass
    finally:
        if out is not None:
            out.close()


if __name__ == "__main__":
    main()
