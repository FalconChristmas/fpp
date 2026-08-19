#!/bin/bash

# PB2_EEPROM redirects the target for testing; it defaults to the real hardware.
EEPROM=${PB2_EEPROM:-/sys/bus/i2c/devices/0-0050/eeprom}

# create a basic eeprom, this specifies week one of 2025 which no
# real PB2's were made so won't  conflict with  existing

OLDNUM=$(dd if="${EEPROM}" bs=1 skip=52 count=4 2>/dev/null)
OLDNUMHEX=$(dd if="${EEPROM}" bs=1 skip=52 count=4 2>/dev/null | xxd -p)

cat > /tmp/eeprom.txt <<EOF
00000000: aa55 33ee 0137 0010 2e00 504f 434b 4554  .U3..7....POCKET
00000010: 4245 4147 4c32 4131 3047 4131 3030 3030  BEAGL2A10GA10000
00000020: 3047 4130 4131 3030 3031 3031 3235 5042  0GA0A100011625PB
00000030: 3230 3030 3337 3335 1102 00a8 12fe ffff  20003735........
EOF
cat /tmp/eeprom.txt | xxd -r > /tmp/eeprom

if [ "$OLDNUMHEX" != "ffffffff" ] && [ -n "$OLDNUMHEX" ]; then
    NEWID=$OLDNUM
else
    # Now generate a random count for this instance
    NEWID=$(head /dev/urandom | tr -dc 0-9 | head -c 4)
fi

echo $NEWID | dd of=/tmp/eeprom bs=1 seek=52 count=4 conv=notrunc

# Read existing EEPROM content
NEWSIZE=$(wc -c < /tmp/eeprom)
dd if="${EEPROM}" of=/tmp/eeprom_existing bs=1 count=$NEWSIZE 2>/dev/null

# Merge: only overwrite existing EEPROM bytes where the new template byte is not 0xff
python3 -c "
with open('/tmp/eeprom_existing', 'rb') as f:
    existing = bytearray(f.read())
with open('/tmp/eeprom', 'rb') as f:
    new = bytearray(f.read())
result = bytearray(existing)
for i in range(min(len(new), len(existing))):
    if existing[i] == 0xff:
        result[i] = new[i]
with open('/tmp/eeprom_merged', 'wb') as f:
    f.write(result)
"

dd if=/tmp/eeprom_merged of="${EEPROM}"
