#!/usr/bin/env python3
"""
upload_patch.py - Send a .pd patch to ESP32 over serial

Usage:
    python3 upload_patch.py <port> <file.pd> [file2.pd ...]

Examples:
    python3 upload_patch.py /dev/cu.usbmodem* simple-phasor.pd
    python3 upload_patch.py /dev/cu.usbmodem14101 data/*.pd
    python3 upload_patch.py COM3 my-patch.pd          # Windows
"""

import serial
import sys
import os
import time


def upload_patch(ser, filepath):
    filename = os.path.basename(filepath)
    with open(filepath, 'r') as f:
        content = f.read()

    print(f"Uploading '{filename}' ({len(content)} bytes)...")

    # Send header
    ser.write(f"PATCH_BEGIN {filename}\n".encode())
    time.sleep(0.05)

    # Send content line by line (ESP32 Serial buffer is small)
    for line in content.splitlines():
        ser.write((line + "\n").encode())
        time.sleep(0.01)  # small delay to avoid overrunning serial buffer

    # Send footer
    ser.write(b"PATCH_END\n")
    time.sleep(0.1)

    # Read response from ESP32
    while ser.in_waiting:
        print(ser.readline().decode(), end='')

    print(f"Done: '{filename}'")


def main():
    if len(sys.argv) < 3:
        print("Usage: upload_patch.py <port> <file.pd> [file2.pd ...]")
        print("       upload_patch.py /dev/cu.usbmodem* data/*.pd")
        sys.exit(1)

    port = sys.argv[1]
    files = sys.argv[2:]

    ser = serial.Serial(port, 115200, timeout=1)
    time.sleep(0.5)  # wait for ESP32 serial to settle

    # Drain any pending output
    while ser.in_waiting:
        print(ser.readline().decode(), end='')

    for filepath in files:
        if not os.path.isfile(filepath):
            print(f"Skipping '{filepath}' (not found)")
            continue
        upload_patch(ser, filepath)

    ser.close()
    print(f"\nUploaded {len(files)} patch(es). Reboot ESP32 to load.")


if __name__ == '__main__':
    main()
