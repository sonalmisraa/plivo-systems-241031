# Reliable UDP Frame Streaming with XOR FEC

This submission implements a low-latency UDP sender and receiver for a stream of 160-byte frames generated every 20 ms. It is designed to tolerate packet loss, delay, duplication, and reordering while remaining below the assignment's 2.00x bandwidth-overhead limit.

## Build

```bash
make clean
make
```

This produces:

```text
./sender
./receiver
```

## Run

Recommended grading configuration:

```bash
python3 run.py --profile profiles/B.json --delay_ms 120 --duration 30
```

A verified Profile A run is:

```bash
python3 run.py --profile profiles/A.json --delay_ms 60 --duration 30
```

## Protocol

The custom sender-to-receiver packet contains:

- 32-bit frame sequence number
- 16-bit FEC group number
- 8-bit packet type (`DATA` or `PARITY`)
- 160-byte payload

The sender groups two consecutive data frames and computes one parity payload:

```text
parity = payload[0] XOR payload[1]
```

If one data packet is missing, the receiver reconstructs it using the received data packet and parity packet.

## Receiver behavior

The receiver:

1. Validates packet length, group range, sequence range, and group consistency.
2. Accepts out-of-order DATA and PARITY packets.
3. Stores the first copy of each packet needed for reconstruction.
4. Recovers one missing data packet per FEC group when possible.
5. Immediately forwards the first valid original or reconstructed frame to port 47020.
6. Uses an atomic flag to prevent duplicate delivery.

## Verified results

| Profile | Delay | Misses | Miss rate | Overhead | Result |
|---|---:|---:|---:|---:|---|
| A | 40 ms | 195–203 | 13.00–13.53% | 1.57x | Invalid |
| A | 60 ms | 11/1500 | 0.73% | 1.57x | Valid |
| B | 120 ms | 12/1500 | 0.80% | 1.57x | Valid |

See `RUNLOG.md` for experiment details and `NOTES.md` for the final design summary.
