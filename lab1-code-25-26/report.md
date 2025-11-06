Based on the provided theoretical slides ("03-data-link-layer.pdf"), the efficiency ($S$) of Automatic Repeat ReQuest (ARQ) mechanisms—specifically Stop and Wait and Go Back N—depends heavily on the transmission time, propagation delay, and, for sliding window protocols, the window size ($W$).

### Key Variables Defined

The efficiency formulas rely on the relationship between two main time components, often condensed into the parameter $a$:

1.  **Frame Transmission Time ($T_f$):** The time required to transmit the information frame.
2.  **Propagation Delay ($T_{prop}$ or $\tau$):** The time for the signal to travel across the link.
3.  **Ratio ($a$):** This dimensionless ratio compares the propagation delay to the frame transmission time:
    $$a = \frac{T_{prop}}{T_f}$$

### 1. Stop and Wait ARQ Efficiency

Stop and Wait (S&W) ARQ is characterized by the sender waiting for a positive confirmation (ACK) from the receiver before transmitting the next information frame.

#### Ideal Case (No Errors, $P_e = 0$)

The theoretical efficiency ($S$) for Stop and Wait ARQ, as presented in the efficiency examples, relates $T_f$ and $T_{prop}$:

$$S = \frac{T_f}{T_f + 2T_{prop} + T_S} = \frac{1}{1 + 2a}$$
*Note: $T_S$ is part of the delay in the Round Trip Time calculation.*

If the propagation delay is large compared to the frame transmission time (i.e., **$a > 1$**), Stop and Wait is considered **inefficient** because the sender transmits only one frame per Round-Trip Time (RTT), where RTT $\approx 2 \cdot T_{prop} + T_f$.

#### Case with Errors

When frames can have errors, the Frame Error Ratio (FER) must be factored in. FER is the probability that a frame has errors. Assuming $P_e$ is the probability that a frame is received in error or lost:

*   The expected number of transmissions ($E[k]$) required to successfully deliver one frame is given by: $E[k] = \frac{1}{1 - P_e}$.

This expectation is crucial for calculating overall efficiency when losses occur. The sources indicate that efficiency analysis must consider errors. The FER ($P[\text{frame has errors}]$) is calculated based on the bit error probability ($p$ or BER) and the frame length ($n$):

$$
P[\text{frame has errors}] = 1 - (1-p)^n
$$


$$S = \frac{T_f}{E[k](T_f + 2T_{prop} + T_S)} = \frac{1 - p_e}{1 + 2a}$$

### 2. Go Back N (GBN) ARQ Efficiency

Go Back N is a Sliding Window protocol that allows the transmission of new frames before earlier ones are acknowledged. The sender may transmit up to $W$ frames without receiving a confirmation (RR/ACK).

The theoretical efficiency ($S$) for Go Back N ARQ, assuming an error-free channel, is determined by the relationship between the window size ($W$) and the transmission ratio $1+2a$:

| Condition | Efficiency Formula (S) | Implication |
| :--- | :--- | :--- |
| **If $W < 1 + 2a$** | $S = \frac{W}{1 + 2a}$ | There is an **idle period** at the end of the RTT. |
| **If $W \geq 1 + 2a$** | $S = 1$ | Transmission is **continuous** (100% utilization). |

If GBN encounters an out-of-sequence frame or detects an error, the receiver silently discards the erroneous frame and sends a negative confirmation (REJ), causing the sender to **Go-Back and retransmit** the rejected frame and all subsequent frames.

### 3. Selective Repeat ARQ

Selective Repeat (SR) is another Sliding Window protocol that improves upon GBN by allowing the receiver to **accept and buffer out-of-sequence frames**.

The sender only retransmits frames specifically signaled by a Selective Reject (SREJ) message. This mechanism is particularly **adequate if $W$ or $a$ is very large**.

While the full efficiency calculation for SR under errors is complex and not explicitly detailed in the source snippets, a key theoretical constraint mentioned is the maximum window size:
*   The maximum window size must be **$W \leq 2^k - 1$** to avoid mistaking a retransmission of an old frame for a new frame, where $k$ is the number of bits used for sequence numbering.

---

**Analogy:** You can think of ARQ efficiency like a construction crew filling potholes in a long, distant road.

*   **Stop and Wait:** The crew chief drives to the site (Propagation Delay, $T_{prop}$), places exactly one shovel full of asphalt (Frame Transmission Time, $T_f$), drives all the way back to the depot for confirmation (waiting for ACK/Timeout), and only then drives back out for the next shovel. If the road is very long ($a$ is large), most of their time is spent driving, making efficiency very low.
*   **Go Back N:** The chief loads $W$ shovels of asphalt and sends a truck with $W$ workers. If the truck gets hit by a rock (error) while delivering the 5th shovel, the chief makes the truck discard shovels 5 through $W$ and start over from shovel 5.
*   **Selective Repeat:** The chief loads $W$ shovels, but if the truck loses only shovel 5, the workers are smart enough to hold onto shovels 6, 7, and 8, and the chief only sends out a special trip to deliver shovel 5. This maximizes efficiency by minimizing unnecessary work.
*

