# BM2 Battery Monitor BLE Protocol Specification

## Overview

The BM2 battery monitor communicates over Bluetooth Low Energy (BLE)
using encrypted packets. All communication is performed using AES‑128
CBC encryption with a fixed 16‑byte key and a zero‑initialized IV. Both
requests and responses are encrypted in 16‑byte blocks. Messages are
transported through a write characteristic and received through a notify
characteristic.

## BLE Services

### Characteristics

-   **Write characteristic:** UUID
    `0000fff3-0000-1000-8000-00805f9b34fb`
-   **Notify characteristic:** UUID
    `0000fff4-0000-1000-8000-00805f9b34fb`

Devices continuously transmit measurement updates through notifications.
Commands are sent via encrypted writes.

## Encryption Scheme

### AES Mode

-   **Algorithm:** AES‑128 CBC
-   **Key:** `6c 65 61 67 65 6e 64 ff fe 31 38 38 32 34 36 36`
-   **IV:** 16 bytes of zero
-   **Padding:** Zero padding to 16‑byte boundary

### Packet Layout

All outgoing packets consist of:

    [ plaintext command bytes … ]  → AES‑CBC → [ encrypted 16‑byte blocks ]

Incoming notifications contain encrypted blocks which must be decrypted
and zero‑padding removed.

## Command Structure

Command messages use a compact binary format. The first byte identifies
the command group, the second byte the subtype, and additional bytes
provide parameters.

### Command Prefix Bytes

-   `E5 02` (signed: `-27, 2`): Request measurement data\
-   `E7 01` (signed: `-25, 1`): Request charging diagnostic\
-   `E3 xx …` (signed: `-29, …`): Request history data\
-   `E9 02` (signed: `-23, 2`): Set alert threshold\
-   `E8 02` (signed: `-24, 2`): Set mode selection or operating options

Values may vary depending on model revision.

### Threshold Adjustment Command

    E9 02 [mode byte] [voltage high byte] [voltage low byte]

-   Mode byte ranges 0--10 depending on threshold mode.
-   Voltage is provided in centivolts.

### Mode/Option Command

    E8 02 [mode id] [option value]

Used for configuration such as units, behaviour, or test initiation.

## Notification Frames

After AES decryption, data is interpreted as ASCII hex encoded values.
Layouts vary according to message type.

### Voltage Status Frame

Format length ≥ 16 hex characters:

    [00-01]  Header
    [02-04]  Voltage × 100
    [05]     Status flag
    [06-07]  Battery level (0–100)
    [08-11]  Timer B (integer)
    [12-15]  Timer C (integer)
    [… optional extra bytes …]

Voltage is obtained by interpreting the 3‑digit hex field as a decimal
value and dividing by 100.

### Charging Diagnostic Frame

Two distinct patterns exist:

#### Pattern A --- "fefefe" diagnostic

    FE FE FE …

Indicates a predefined diagnostic result.

#### Pattern B --- standard diagnostic

    [02-03]   Status
    [04-06]   Idle voltage × 100
    [07-09]   High‑load voltage × 100

### Cranking Test Frame

Frames containing the marker:

    FF FE FE

hold cranking voltage samples.

Format:

    [04-07]  Base timing seed
    [08-10]  Current voltage × 100
    [11]     Status
    [12-??]  List of 3‑digit sample voltages × 100
    […]     Terminal "FF FE FE" marker

A derived timestamp is computed from the base seed and previous timing
values transmitted in voltage frames.

### History Frames

History frames contain a sequence of 8‑character blocks:

    VVV TTTT

Where: - `VVV` = centivolts\
- `T` = event type\
- Samples are spaced 120 seconds apart, and timestamp reconstruction is
relative to the latest reference time supplied by the device.

## Measurement Types

### Voltage

Real‑time system voltage, centivolt precision.

### Battery Power Estimate

Percentage of estimated remaining capacity.

### Status Codes

-   0: Normal
-   1: Weak
-   2: Very weak
-   Additional values may exist for certain model variants.

### Cranking Status

Interpreted from status byte in cranking frame: - 0: Normal - 1: Slow
crank - 2: Failed crank

## Data Flow Summary

1.  Device sends periodic encrypted notifications.
2.  Notifications are decrypted and interpreted as one of:
    -   Voltage status update\
    -   Charging diagnostic\
    -   Cranking test sample set\
    -   Historical dataset\
3.  Commands may be issued to request a specific dataset or to configure
    thresholds and behaviour.

## Error Handling

Malformed decrypted frames should be ignored. Zero‑padding is expected
at the end of decrypt output. Unknown headers should be logged for
future decoding.

## Timing

-   Cranking samples appear in bursts following a start request.
-   History samples are returned in sequences that must be reassembled
    in reverse order.
-   Regular voltage frames occur continuously, generally once per
    second.
