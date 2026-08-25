#!/bin/bash

# PB2_EEPROM redirects the target for testing; it defaults to the real hardware.
EEPROM=${PB2_EEPROM:-/sys/bus/i2c/devices/0-0050/eeprom}

# Create a basic eeprom for the industrial board. Offset 44..49 is the
# manufacturing date as MMDDYY; this template carries 010426 (Jan 4 2026),
# which -- unlike the base board's template -- was NOT changed to an
# impossible date and may well be a real manufacturing date.
#
# That is safe because of the unit-number marker below: a unit number we
# assign can never be one the factory issued, whatever date it sits behind.
#
# The template was taken from a dump of a real board, so the bytes outside the
# date and unit number are a genuine board's.

# The unit number is the six ASCII digits at offset 50..55; offset 44..49 is the
# manufacturing date (MMDDYY) and the board ID.  Boards are manufactured in
# batches of under 10000 per day, so a factory unit number never exceeds 9999
# and the leading digit at offset 50 is always "0".
#
# That leaves 2-9 free as a marker for a unit number we assigned ourselves. Using
# it means a repaired board can never collide with a real board -- not merely
# that a collision is unlikely -- whatever date the template carries, and it
# makes a repaired board identifiable from its serial alone.  The remaining five
# digits give 8 x 10^5 distinct values.
#
# A fully programmed unit number is left exactly as it is: this must never
# overwrite a factory serial.
OLDUNIT=$(dd if="${EEPROM}" bs=1 skip=50 count=6 2>/dev/null)

# Five hex reads' worth of entropy, retried because tr can filter out everything
rand_digits() {
    local n="$1" out=""
    while [ "${#out}" -lt "$n" ]; do
        out="${out}$(head -c 512 /dev/urandom | tr -dc 0-9)"
    done
    printf '%s' "${out:0:$n}"
}

if [[ "$OLDUNIT" =~ ^[0-9]{6}$ ]]; then
    # Factory (or previously assigned) unit number, fully programmed. Keep it.
    NEWUNIT="$OLDUNIT"
    ASSIGNED=n
else
    LEADPOOL="23456789"
    # 10# so a leading zero is not read as octal
    LEAD="${LEADPOOL:$((10#$(rand_digits 3) % 8)):1}"
    NEWUNIT="${LEAD}$(rand_digits 5)"
    ASSIGNED=y
fi

cat > /tmp/eeprom.txt <<EOF
00000000: aa55 33ee 0137 0010 2e00 504f 434b 4554  .U3..7....POCKET
00000010: 4245 4147 4c32 4131 4930 4131 3030 3030  BEAGL2A1I0A10000
00000020: 4930 4130 4131 3030 3031 3034 3236 5042  I0A0A100010426PB
00000030: 3249 3030 3536 3932 1102 00a8 12fe ffff  2I005692........
EOF
cat /tmp/eeprom.txt | xxd -r > /tmp/eeprom

printf '%s' "$NEWUNIT" | dd of=/tmp/eeprom bs=1 seek=50 count=6 conv=notrunc 2>/dev/null

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

# Apply the unit number outside the merge.
#
# The merge only replaces bytes that are still 0xFF, which is right for every
# other field but wrong here: a board whose unit number is partly programmed
# (offset 50..51 written, 52..55 still blank) would keep its "00" lead and take
# only the low digits from us, landing back inside the factory space unmarked.
# The unit number has to be decided as one field, so write all six bytes.
if [ "$ASSIGNED" = "y" ]; then
    printf '%s' "$NEWUNIT" | dd of=/tmp/eeprom_merged bs=1 seek=50 count=6 conv=notrunc 2>/dev/null
fi

dd if=/tmp/eeprom_merged of="${EEPROM}"
