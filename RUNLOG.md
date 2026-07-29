# Experiment Run Log

## Objective

The objective was to maintain a deadline-miss rate of at most 1%, keep total relay bandwidth below 2.0× the raw stream, and then minimize playout delay.

The implementation uses systematic 2+1 XOR forward-error correction. Every source frame is transmitted immediately as a data packet. For every pair of data frames, the sender also transmits one parity packet containing the byte-wise XOR of the pair.

The receiver forwards original frames immediately and reconstructs one missing frame when the other frame and parity packet are available.

---

## Baseline Observation

The naive implementation transmitted each frame only once. Any packet dropped by the relay therefore caused an unrecoverable deadline miss.

This showed that a loss-recovery mechanism was required before attempting to reduce playout delay.

---

## Experiment 1 — Larger FEC Groups

### Change

Initially, a larger FEC group was considered, where one parity packet protected several consecutive data frames.

### Observation

Although this reduced bandwidth overhead, parity for the earliest frame could not be generated until all later frames in the group had arrived from the source.

Since the source produces one frame every 20 ms, larger groups introduced significant recovery latency. A lost early frame could therefore miss its deadline even when it was mathematically recoverable.

### Conclusion

Recovery latency, rather than only redundancy overhead, is critical in a real-time stream. The FEC group size was reduced to two data frames.

---

## Experiment 2 — 2+1 XOR FEC

### Change

The sender was changed to:

1. Forward every data frame immediately.
2. Group consecutive frames in pairs.
3. Transmit one XOR parity packet after the second frame of each pair becomes available.

For frames `2k` and `2k+1`, parity is:

```text
parity = frame[2k] XOR frame[2k+1]
```

If exactly one frame in the pair is lost, the receiver reconstructs it by XORing the received frame with the parity packet.

### Bandwidth

Each sender-to-receiver packet is 167 bytes:

* 4-byte sequence number
* 2-byte group number
* 1-byte packet type
* 160-byte payload

For every two source frames, the sender transmits:

* Two data packets
* One parity packet

The measured bandwidth overhead was consistently **1.57×**, below the required 2.0× cap.

---

# Profile A Experiments

## Profile A — 60 ms

Command:

```bash
python3 run.py --profile profiles/A.json --delay_ms 60
```

Results:

| Metric             |  Value |
| ------------------ | -----: |
| Frames             |   1500 |
| Deadline misses    |     10 |
| Miss rate          |  0.67% |
| Playout delay      |  60 ms |
| Upstream packets   |   2250 |
| Upstream bytes     | 375750 |
| Relay drops        |     52 |
| Relay duplicates   |     14 |
| Bandwidth overhead |  1.57× |
| Result             |  VALID |

### Interpretation

The 2+1 FEC scheme successfully recovered enough isolated packet losses to remain below the 1% miss-rate cap.

Immediate forwarding prevented unnecessary receiver-side buffering. Original frames were delivered as soon as they arrived, while reconstructed frames were delivered immediately after the required parity and companion data packet became available.

A 60 ms playout delay is therefore a valid operating point for Profile A.

---

# Profile B Delay Sweep

Profile B has a higher loss rate and more severe delay behaviour than Profile A. The delay was gradually reduced to identify the validity boundary.

## Profile B — 120 ms

Command:

```bash
python3 run.py --profile profiles/B.json --delay_ms 120
```

| Metric             |  Value |
| ------------------ | -----: |
| Frames             |   1500 |
| Deadline misses    |     12 |
| Miss rate          |  0.80% |
| Playout delay      | 120 ms |
| Upstream packets   |   2250 |
| Upstream bytes     | 375750 |
| Relay drops        |    123 |
| Relay duplicates   |     25 |
| Bandwidth overhead |  1.57× |
| Result             |  VALID |

The design had a 0.20 percentage-point margin below the miss-rate cap.

---

## Profile B — 110 ms

Command:

```bash
python3 run.py --profile profiles/B.json --delay_ms 110
```

| Metric             |  Value |
| ------------------ | -----: |
| Frames             |   1500 |
| Deadline misses    |     12 |
| Miss rate          |  0.80% |
| Playout delay      | 110 ms |
| Bandwidth overhead |  1.57× |
| Result             |  VALID |

Reducing the delay from 120 ms to 110 ms did not increase the number of deadline misses. This indicates that the 12 missed frames were primarily unrecoverable losses rather than frames arriving between the 110 ms and 120 ms deadlines.

---

## Profile B — 100 ms

Command:

```bash
python3 run.py --profile profiles/B.json --delay_ms 100
```

| Metric             |  Value |
| ------------------ | -----: |
| Frames             |   1500 |
| Deadline misses    |     15 |
| Miss rate          |  1.00% |
| Playout delay      | 100 ms |
| Bandwidth overhead |  1.57× |
| Result             |  VALID |

This was the lowest observed valid delay.

The run is exactly at the permitted 1.00% miss-rate limit. It demonstrates that the mechanism can operate at 100 ms on the supplied Profile B, but it provides no safety margin against different random loss patterns or unseen profiles.

---

## Profile B — 90 ms

Command:

```bash
python3 run.py --profile profiles/B.json --delay_ms 90
```

| Metric             |   Value |
| ------------------ | ------: |
| Frames             |    1500 |
| Deadline misses    |      24 |
| Miss rate          |   1.60% |
| Playout delay      |   90 ms |
| Bandwidth overhead |   1.57× |
| Result             | INVALID |

Reducing the delay by another 10 ms caused nine additional frames to miss their deadlines.

This shows that several recoverable or delayed frames arrive in the interval between 90 ms and 100 ms. Therefore, 90 ms is below the practical jitter-buffer requirement for this design on Profile B.

---

# Profile B Summary

|  Delay | Misses | Miss rate | Overhead | Result  |
| -----: | -----: | --------: | -------: | ------- |
| 120 ms |     12 |     0.80% |    1.57× | VALID   |
| 110 ms |     12 |     0.80% |    1.57× | VALID   |
| 100 ms |     15 |     1.00% |    1.57× | VALID   |
|  90 ms |     24 |     1.60% |    1.57× | INVALID |

The observed validity boundary lies between 90 ms and 100 ms.

---

# Final Decision

The lowest demonstrated valid delay is:

```text
Profile A: 60 ms
Profile B: 100 ms
```

For grading on unknown profiles, the recommended conservative setting is:

```text
DELAY_MS = 110
```

The 100 ms run is valid on the supplied Profile B but lies exactly at the 1% miss-rate cap. A 110 ms delay retains the same 0.80% miss rate as 120 ms while reducing latency by 10 ms and preserving a small safety margin.

---

# Limitations

The 2+1 XOR scheme can recover exactly one missing data packet per pair. It cannot recover a pair when:

1. Both data packets are lost.
2. One data packet and its parity packet are both lost.
3. A required packet arrives after the playout deadline.

The scheme is therefore effective for isolated loss but remains vulnerable to burst losses affecting multiple packets in the same FEC group.

The receiver also uses fixed consecutive pairs, so temporally adjacent losses are more likely to affect the same recovery group. Interleaving frames across FEC groups or adding a second independent parity relation could improve burst-loss resistance, but would require additional bandwidth or recovery delay.

---

# Conclusion

The final 2+1 XOR FEC design achieved:

* A valid 60 ms playout delay on Profile A.
* A lowest observed valid delay of 100 ms on Profile B.
* A safer Profile B operating point of 110 ms.
* Constant measured bandwidth overhead of 1.57×.
* Correct handling of isolated loss, packet reordering, and duplicate delivery.

The experiments demonstrate the central trade-off of the system: reducing playout delay improves responsiveness but leaves less time for delayed parity and data packets to arrive before their deadlines.
