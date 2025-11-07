# Stop-and-Wait ARQ: Compact Formulas & Key Concepts

## Throughput
$$
R = \frac{L \times 8}{T_{\text{total}}}
$$
- $L$: file size (bytes)
- $T_{\text{total}}$: transfer time (s)

## Measured Efficiency
$$
S_{\text{measured}} = \frac{R}{C}
$$
- $R$: throughput (bps)
- $C$: link rate (bps)

## Bit Error Rate to Frame Error Rate
$$
FER = 1 - (1 - BER)^{N}
$$
- $BER$: bit error rate
- $N$: frame size (bits)

## Stop-and-Wait ARQ Theoretical Efficiency
$$
S_{\text{theoretical}} = \frac{1 - FER}{1 + 2a}
$$
- $FER$: frame error rate
- $a$: delay ratio

## Delay Ratio
$$
a = \frac{T_{\text{prop}}}{T_{\text{frame}}}
$$
- $T_{\text{prop}}$: propagation delay (s)
- $T_{\text{frame}}$: frame transmission time (s)

## Frame Transmission Time
$$
T_{\text{frame}} = \frac{N}{C}
$$
- $N$: frame size (bits)
- $C$: link rate (bps)

## Expectation of Attempts (Geometric)
$$
E[\text{attempts}] = \frac{1}{1 - p_e}
$$
- $p_e$: frame error rate

## Efficiency Concept
$$
S = \frac{\text{Useful Time}}{\text{Total Time}}
$$
- Useful time: time spent transmitting data
- Total time: transmission, propagation, waiting, retransmissions
