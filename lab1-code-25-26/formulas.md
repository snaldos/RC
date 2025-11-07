

# Stop-and-Wait ARQ: Compact Formulas & Concepts

## Core Relationships

- **Frame Transmission Time:** $T_f = \frac{N}{C}$
- **Propagation Delay:** $T_{prop}$
- **Delay Ratio:** $a = \frac{T_{prop}}{T_f}$
- **Frame Error Rate (FER):** $FER = 1 - (1 - err_{byte})^{N}$
- **Frame Error Probability (from BER):** $P_{err} = 1 - (1-p)^n$
- **Expected Attempts:** $E[k] = \frac{1}{1 - FER}$

## Efficiency & Throughput

- **Stop-and-Wait Efficiency:**
	$$S = \frac{\text{Useful Time}}{\text{Total Time}}$$
	$$S = \frac{T_f}{T_{prop} + T_f + T_{prop} + T_{ACK}}$$

	$$S = \frac{1}{E[k](1 + 2a)}$$
  $$S = \frac{1 - FER}{1 + 2a}$$

- **Throughput:**
	$$R = \frac{L \times 8}{T_{\text{total}}}$$
- **Measured Efficiency:**
	$$S_{\text{measured}} = \frac{R}{C}$$

## Notes on Approximations

Note: In Stop-and-Wait ARQ, $T_{ACK}$ (ACK transmission time) is typically omitted from the total time formula, as it is much smaller than $T_f$ and $T_{prop}$ and does not significantly affect efficiency in most practical cases.

