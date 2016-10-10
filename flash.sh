#!/bin/bash -e

rm *.bin || true
particle compile core
particle flash --usb `ls | grep -i -E '\.bin$' | head -1`
