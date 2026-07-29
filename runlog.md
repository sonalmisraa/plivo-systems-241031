# RUNLOG

All experiments used a 30-second stream containing 1500 frames. A run is valid when the deadline-miss rate is at most 1.00% and total bandwidth overhead is at most 2.00x.

## Experiment 1 — Initial 4+1 FEC implementation

- **Profile:** A
- **Playout delay:** 60 ms
- **Frames:** 1500
- **Deadline misses:** 1335 (89.00%)
- **Bandwidth overhead:** 1.30x
- **Result:** INVALID

### Change tested

Implemented XOR-based forward error correction using four data packets and one parity packet. The receiver buffered frames and released them close to their configured playout deadlines.

### Observation

The design failed because parity for the earliest frame in a four-frame group could not be generated until the remaining three frames had been produced. At 20 ms per frame, this added up to 60 ms before network delay was even considered. Waiting until the deadline to forward frames also introduced unnecessary scheduler risk.

---

## Experiment 2 — Final 2+1 FEC at low delay

- **Profile:** A
- **Playout delay:** 40 ms
- **Seed 1:** 203 misses (13.53%), INVALID
- **Seed 2:** 195 misses (13.00%), INVALID
- **Seed 3:** 202 misses (13.47%), INVALID
- **Bandwidth overhead:** 1.57x in each run

### Change tested

Reduced the FEC group size to two data packets plus one XOR parity packet and changed the receiver to forward original or reconstructed frames immediately.

### Observation

The smaller FEC group makes parity available after one additional 20 ms frame, but a 40 ms deadline is still too aggressive once relay delay and packet loss are included. The consistently high miss rate across the tested seeds shows that 40 ms is not a viable operating point for Profile A.

---

## Experiment 3 — Final design on Profile A

- **Profile:** A
- **Playout delay:** 60 ms
- **Frames:** 1500
- **Deadline misses:** 11 (0.73%)
- **Bandwidth overhead:** 1.57x
- **Relay packets:** 2250 upstream packets
- **Relay drops:** 52
- **Relay duplicates:** 14
- **Result:** VALID

### Change tested

Used two data packets plus one XOR parity packet, duplicate suppression at the receiver, immediate forwarding, and reconstruction when exactly one data packet in a group was missing.

### Observation

The implementation met both grading constraints. Immediate forwarding removed avoidable playout scheduling delay, while the 2+1 FEC structure recovered many isolated losses without exceeding the bandwidth limit.

---

## Experiment 4 — Final design on Profile B

- **Profile:** B
- **Playout delay:** 120 ms
- **Frames:** 1500
- **Deadline misses:** 12 (0.80%)
- **Bandwidth overhead:** 1.57x
- **Relay packets:** 2250 upstream packets
- **Relay drops:** 123
- **Relay duplicates:** 25
- **Result:** VALID

### Change tested

Tested the final design under the more demanding practice profile, which has higher loss and a larger delay range than Profile A.

### Observation

The run remained below the 1.00% deadline-miss cap and the 2.00x overhead cap. This is the strongest verified operating point and is therefore the recommended grading configuration.

---

## Final configuration

- **FEC:** Two DATA packets plus one XOR PARITY packet
- **Receiver behavior:** Immediately forwards the first valid copy of each original or reconstructed frame
- **Duplicate handling:** Atomic per-frame flag prevents repeated delivery
- **Feedback traffic:** None
- **Measured overhead:** 1.57x
- **Recommended grading delay:** **120 ms**

The 120 ms recommendation is conservative because it is the verified valid result on the harsher supplied profile. A lower delay may work on some profiles or seeds, but it was not selected without sufficient validation.