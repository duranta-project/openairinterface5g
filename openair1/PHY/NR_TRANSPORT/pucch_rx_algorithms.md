# PUCCH Receiver Algorithms — `pucch_rx.c`

This document describes the physical-layer receiver algorithms in
`openair1/PHY/NR_TRANSPORT/pucch_rx.c` for the 5G NR gNB.
The file implements UCI reception for PUCCH Formats 0, 1, 2, and 3
as specified in 3GPP TS 38.211, 38.212, and 38.213.

---

## Table of Contents

1. [Overview](#1-overview)
2. [Format 0 — `nr_decode_pucch0`](#2-pucch-format-0--nr_decode_pucch0)
3. [Format 1 — `nr_decode_pucch1`](#3-pucch-format-1--nr_decode_pucch1)
4. [Formats 2 & 3 — `nr_decode_pucch2_3`](#4-pucch-formats-2--3--nr_decode_pucch2_3)
5. [Helper / Initialisation Functions](#5-helper--initialisation-functions)
6. [Data Types and Key Structures](#6-data-types-and-key-structures)
7. [Standards References](#7-standards-references)

---

## 1. Overview

PUCCH carries HARQ acknowledgements (ACK/NACK), Scheduling Requests (SR), and
Channel State Information (CSI) from UE to gNB.

| Format | Symbols | PRBs | Bits | Encoding | Main use |
|--------|---------|------|------|----------|----------|
| 0 | 1–2 | 1 | 0–2 | Sequence selection | SR, 1–2 HARQ bits |
| 1 | 4–14 | 1 | 1–2 | OCC + sequence | SR, 1–2 HARQ bits |
| 2 | 1–2 | 1–16 | 3–64 | RM / polar + QPSK | HARQ + SR + CSI |
| 3 | 4–14 | 1–16 | 3–64 | RM / polar + QPSK | HARQ + SR + CSI |

All decoders read from `gNB->common_vars.rxdataF` and write decoded UCI into
an `nfapi_nr_uci_pucch_pdu_*` output structure.

### Non-Coherent Block Detection

All four formats use a common detection philosophy: **non-coherent block
detection**.  The fundamental assumption is that the channel is approximately
constant over a *coherence block* — a contiguous set of REs in time and
frequency over which the phase of the channel can be treated as unknown but
fixed.  The receiver therefore integrates signal energy within each block
without relying on phase knowledge, which avoids the need for explicit
per-subcarrier channel estimation on every resource.

For **Formats 0 and 1**, the PUCCH spans only a single PRB (12 subcarriers).
The entire PRB constitutes one coherence block, and detection is a
non-coherent correlation against the set of candidate sequences.  Within a
single PRB, however, the channel *is* assumed coherent across symbols that
share the same PRB, so those symbols are combined **coherently** (phases are
summed before squaring).  When **intra-slot frequency hopping** is used, the
two hops occupy different PRBs and the channels across the two hops cannot be
assumed phase-coherent; the contributions from each hop are therefore combined
**non-coherently** (squared magnitudes are summed).

For **Formats 2 and 3**, the PUCCH spans multiple PRBs and symbols.  The
receiver accumulates the correlation metric over all groups and all symbols
non-coherently, summing the squared magnitudes of each group's contribution,
so no inter-group phase alignment is assumed or required.  The coherence block
size depends on format and payload length:

- **Format 2, short block (3–11 bits):** 2 PRBs per coherence block.
- **Format 2, polar code (12–64 bits):** a sub-PRB group of 4 REs is the
  coherence block, allowing the LLR computation to remain non-coherent at
  fine granularity.
- **Format 3:** a full PRB (12 subcarriers) is the coherence block.

---

## 2. PUCCH Format 0 — `nr_decode_pucch0`

**Lines 129–466** | **1–2 symbols, 1 PRB, 0–2 bits**

Format 0 encodes information in the *cyclic shift index* of a 12-element
low-PAPR sequence. No data symbols are transmitted; the receiver performs
maximum-likelihood sequence detection.

### 2.1 Cyclic-Shift LUT (`get_pucch0_cs_lut_index`, lines 71–100)

Before detection, the cyclic-shift hopping sequence is pre-computed for the
entire frame and cached, keyed by `pucch_GroupHopping` / `hoppingId`.
`nr_cyclic_shift_hopping()` is called once per (slot, symbol) pair and the
result is stored as an integer LUT (divided by $\pi/6$). Subsequent
calls for the same hopping ID return immediately.

### 2.2 Candidate Sequence Generation

For each OFDM symbol, sequences are generated for all cyclic shifts
$m_\text{cs} \in \{0, \ldots, 11\}$:

$$s_{m_\text{cs}}[n] = e^{j\,\alpha(m_\text{cs})\,n} \cdot r_{u,v}[n], \quad n = 0,\ldots,11$$

where $\alpha(m_\text{cs}) = 2\pi(m_\text{cs,initial} + m_\text{cs} + m_\text{cs,hop})/12$
and $r_{u,v}[n]$ is the base sequence from TS 38.211 Table 5.2.2.2-2.

The number of tested hypotheses depends on payload size:

| Payload | Hypotheses |
|---------|-----------|
| SR only (0 HARQ bits) | 1 |
| 1 HARQ bit ± SR | 4 |
| 2 HARQ bits | 8 |

### 2.3 Received Signal Extraction

Twelve subcarriers starting at the configured PRB are extracted from
`rxdataF` per antenna and per PUCCH symbol. Frequency hopping switches the
PRB offset on the second symbol.

### 2.4 Correlation via 12-Point IDFT

The received vector $\mathbf{y}$ is correlated against each candidate sequence using
a 12-point IDFT with pre-computed integer tables (`idft12_re`, `idft12_im`):

$$C_{aa}[m_\text{cs}] = \sum_{n=0}^{11} y_{aa}[n] \cdot s_{m_\text{cs}}^*[n]$$

implemented in fixed-point as:

$$C_{aa}[m_\text{cs}].\mathrm{re} = \sum_n \bigl( y[n].\mathrm{re} \cdot \texttt{idft12\_re}[m_\text{cs}][n] + y[n].\mathrm{im} \cdot \texttt{idft12\_im}[m_\text{cs}][n] \bigr)$$

$$C_{aa}[m_\text{cs}].\mathrm{im} = \sum_n \bigl( y[n].\mathrm{im} \cdot \texttt{idft12\_re}[m_\text{cs}][n] - y[n].\mathrm{re} \cdot \texttt{idft12\_im}[m_\text{cs}][n] \bigr)$$

### 2.5 Multi-Symbol Combining

| Configuration | Combining method |
|---------------|-----------------|
| 1 symbol | Non-coherent: $\Lambda = \sum_{aa} \lvert C_{aa} \rvert^2$ |
| 2 symbols, no hop | Coherent: $\Lambda = \sum_{aa} \lvert C_{aa}^{(0)} + C_{aa}^{(1)} \rvert^2$ |
| 2 symbols, freq hop | Non-coherent: $\Lambda = \sum_{aa} \bigl(\lvert C_{aa}^{(0)} \rvert^2 + \lvert C_{aa}^{(1)} \rvert^2\bigr)$ |

The ML decision is $\hat{m}_\text{cs} = \arg\max_{m_\text{cs}} \Lambda(m_\text{cs})$.

### 2.6 Bit Extraction

| `nb_harq_bits` | SR present | Decision |
|----------------|------------|----------|
| 0 | yes | SR $= 1$ if $\Lambda_\max > \text{threshold}$, else $0$ |
| 1 | no/yes | HARQ $= \hat{m}_\text{cs} \gg 3$; SR $= \hat{m}_\text{cs} \mathbin{\&} 1$ |
| 2 | no/yes | HARQ bits from $(\hat{m}_\text{cs} \gg 2)\mathbin{\&}1$, $(\hat{m}_\text{cs} \gg 3)\mathbin{\&}1$; SR $= \hat{m}_\text{cs} \mathbin{\&} 1$ |

SNR is computed from the peak-to-average metric ratio and mapped to a CQI
value (0–255, covering $-64$ to $+63.5$ dB). A configurable threshold
`gNB->pucch0_thres` gates the SR decision.

---

## 3. PUCCH Format 1 — `nr_decode_pucch1`

**Lines 468–1074** | **4–14 symbols, 1 PRB, 1–2 bits**

Format 1 modulates 1 or 2 HARQ bits onto a low-PAPR sequence and spreads the
result across OFDM symbols with an orthogonal cover code (OCC).  Alternating
symbols carry DM-RS for channel estimation.

### 3.1 Symbol Layout

Even symbols (0, 2, 4, …) within the PUCCH allocation carry DM-RS; odd
symbols carry data.  Both use the same 12-subcarrier PRB.

### 3.2 Sequence Generation

For each symbol, the group hopping indices $u$, $v$ are obtained from
`nr_group_sequence_hopping()`.  The cyclic shift $\alpha$ from
`nr_cyclic_shift_hopping()` is applied to the base sequence:

$$r_{u,v,\alpha}[n] = e^{j\alpha n} \cdot r_{u,v}[n], \quad n = 0,\ldots,11$$

implemented in fixed-point using a pre-computed $(\cos\alpha n,\,\sin\alpha n)$
pair at each subcarrier and `c16mulShift()`.

### 3.3 OCC Spreading / Despreading

The OCC weight for data symbol index $m$ within spreading factor $N_\text{SF}$ is:

$$w_i(m) = e^{j 2\pi i m / N_\text{SF}}$$

Two operating modes:

- **No intra-slot hopping** — single OCC table
  `table_6_3_2_4_1_1_N_SF_mprime_PUCCH_1_noHop` covers all symbols.
- **Intra-slot frequency hopping** — separate tables for $m'=0$
  (first half-slot) and $m'=1$ (second half-slot).

Received symbols are multiplied by $w_i^*(m)$ and the conjugate of the
reference sequence to despread both DM-RS and data.

### 3.4 Channel Estimation

The DM-RS symbols, after despreading, are averaged across all DM-RS symbols
and subcarriers to yield a per-antenna channel estimate:

$$\hat{h}_r = \frac{1}{N_\text{DM-RS} \cdot 12} \sum_{m,n} z_\text{DM-RS}[r][12m+n]$$

For frequency hopping, independent estimates $\hat{h}_r$ and $\hat{h}_r^{(1)}$
are computed for each hop.

### 3.5 ML Detection

The despread data accumulation across all data symbols gives $y_r$.

**1 HARQ bit (BPSK)** — two hypotheses $d \in \{+1, -1\}$:

$$\Lambda_d = \sum_r \left\lvert \hat{h}_r + \frac{d}{\sqrt{2}}\, y_r \right\rvert^2$$

The sign with the larger metric gives the decoded bit.

**2 HARQ bits (QPSK)** — four hypotheses
$d \in \{+1+j,\; +1-j,\; -1+j,\; -1-j\} / \sqrt{2}$:

$$\Lambda_d = \sum_r \left\lvert \hat{h}_r + d\, y_r \right\rvert^2$$

The maximum-metric index maps to the 2-bit Gray-coded HARQ word.

For frequency hopping, metrics from both hops are summed non-coherently.

---

## 4. PUCCH Formats 2 & 3 — `nr_decode_pucch2_3`

**Lines 1138–1999** | **Variable symbols and PRBs, 3–64 bits**

Formats 2 and 3 share a single decoder function.  Both apply QPSK modulation
and either a Reed-Muller small-block code (3–11 bits) or a polar code
(12–64 bits).  The key difference is their time-frequency structure:

| | Format 2 | Format 3 |
|---|---|---|
| Symbols | 1–2 | 4–14 |
| DM-RS density | Every symbol, RE positions 1,4,7,10 per PRB | Sparse: 2 or 4 dedicated DMRS symbols |
| DM-RS generation | Gold sequence (TS 38.211 §6.4.1.3.2) | Low-PAPR sequence with group/cyclic-shift hopping |
| Data transform | None (frequency domain directly) | FFT across groups of 4 time-domain symbols |

### 4.1 Signal Extraction and Scaling

All received REs are extracted per antenna and symbol into
`rp[Prx][nb_symbols][nb_re_pucch]`.  Total signal energy is accumulated:

$$E = \sum_{aa,\,l} \texttt{signal\_energy\_nodc}(\mathbf{r}_{aa,l})$$

A scaling exponent is derived to keep subsequent fixed-point products in range:

$$\texttt{scaling} = \max\!\left(\left\lfloor \tfrac{1}{2}\log_2 E \right\rfloor - 8,\; 0\right)$$

### 4.2 DM-RS Position Selection (Format 3)

DMRS symbol positions within the PUCCH allocation are selected from
standardised tables based on the number of symbols and the
`additional_dmrs` flag:

| `nr_of_symbols` | `additional_dmrs` | DMRS symbols (relative) |
|-----------------|-------------------|-------------------------|
| 4–9 | either | 2 symbols |
| 10–14 | 0 | 2 symbols |
| 10–14 | 1 | 4 symbols |

### 4.3 Data Scrambling

A Gold sequence is initialised with:

$$c_\text{init} = (\text{RNTI} \ll 15) + \text{data\_scrambling\_id}$$

The binary scrambling sequence is packed into SIMD registers and applied to
data REs via `simde_mm_sign_epi16()` (multiply by $\pm 1$ per bit).

### 4.4 DM-RS Processing and Channel Estimation

**Format 2:** The Gold-sequence pilot values are generated on-the-fly. Each
pilot RE is multiplied by the conjugate of the expected pilot, and an integer
delay estimate is computed via `nr_est_delay()` (128-tap correlation). A
pre-computed frequency-domain phase-ramp filter (`delay_table128`) compensates
the detected delay across all data REs.

**Format 3:** The low-PAPR DM-RS sequence is generated per DMRS symbol using
group/cyclic-shift hopping (same as Format 1). The received DMRS REs are
multiplied by the conjugate of the expected sequence and accumulated into
per-group, per-antenna correlation values `corr32[symb][group][aa]`.

### 4.5 Format 3 FFT Processing

For data symbols, Format 3 uses time-domain OCC spreading (analogous to
Format 1). Groups of 4 consecutive data symbols are collected into an IDFT
input buffer per antenna. When the buffer is full (or at end of PUCCH):

1. Conjugates are taken to convert to IDFT input form.
2. A 12-, 24-, or 36-point FFT is applied via `dft()`.
3. The output is transposed using SIMD unpack/shuffle operations to convert
   from time-interleaved to subcarrier-interleaved order.
4. The gold scrambling sequence is removed with `simde_mm_sign_epi16()`.

The result is equivalent to the de-spread, unscrambled data RE grid used by
Format 2.

### 4.6 Decoding — Short Blocks (3–11 bits), Format 2

`init_pucch2_3_luts()` pre-encodes every information word with
`encodeSmallBlock()` (Reed-Muller / simplex code) and stores the BPSK-mapped
symbols $b[k] \in \{1,-1\}$ in `pucch2_3_lut[N-3][cw]`. Note that $b[i]$ in TS 38.211 is a binary ${0,1}$ sequence which is mapped to BPSK here for convenience in the receiver.

#### Signal model and group structure

The non-coherent group size is $N_g = 2$ PRBs, giving
$N_\text{group} = \lfloor P/2 \rfloor$ groups (where $P$ is `prb_size`).
Each group spans 2 consecutive PRBs: $N_p = 8$ DMRS REs and $N_d = 16$ data
REs per OFDM symbol.

The channel is assumed flat within a group.  For group $g$, symbol $l$,
and receive antenna $aa$:

$$r_\text{DM-RS}[aa,l,k] = h_{g,aa} \cdot p[k] + n[k], \quad k \in \mathcal{K}_\text{DM-RS}(g)$$

$$r_\text{data}[aa,l,k] = h_{g,aa} \cdot d[k] + n[k], \quad k \in \mathcal{K}_\text{data}(g)$$

where $p[k] = (1-2c(2k)+j(1-2c(2k+1)))/\sqrt{2}$ is the QPSK pilot value
generated from the Gold sequence $c(i)$ (TS 38.211 §6.4.1.3.2.1), and
$d[k] \in \{(\pm 1 \pm j)/\sqrt{2}\}$ is the QPSK-modulated codeword symbol
corresponding to $d(i)$-sequence in TS 38.211 §6.3.2.5.2.

#### DMRS-assisted coherent reference per group

The DMRS REs within each group are correlated with the conjugate of the known
pilot sequence to form a channel reference:

$$H[g,l,aa] = \sum_{k \in \mathcal{K}_\text{DM-RS}(g)} r_\text{DM-RS}[aa,l,k] \cdot p^*[k] \;\approx\; h_{g,aa} \cdot N_p$$

(stored as `corr32[l][g][aa]` in the code).

This reference is combined across OFDM symbols to initialise the detection
statistic $Z$:

| Configuration | Initialisation of $Z[g,aa]$ |
|---------------|------------------------------|
| 1 symbol | $H[g,0,aa]$ |
| 2 symbols, no freq hop | $H[g,0,aa] + H[g,1,aa]$ (coherent) |
| 2 symbols, freq hop (hop $d$) | $H[g,d,aa]$ (separate per hop) |

#### Codeword correlation

The transmitted QPSK data symbol $d[k]$ encodes two scrambled bits per subcarrier:

$$d[k] = \frac{(1-2\tilde{b}(2k)) + j\,(1-2\tilde{b}(2k+1))}{\sqrt{2}}$$

where $\tilde{b}(i) = (b(i) + c_\text{scr}(i)) \bmod 2$ is the scrambled bit sequence
(TS 38.211 §6.3.2.5.1) and $c_\text{scr}(i)$ is the Gold scrambling sequence
initialised with $c_\text{init}$ from Section 4.3.

To follow the steps of the correlation with unscrambling 
Let $b'(i) = (1-2b(i))$ and $c'(i) = (1-2c(i))$ so that $\tilde{b}'(i)=(1-2b(i))(1-2c(i))$, and the $k^{\text{th}}$ received
dimension is
$$r_\text{data}(k) =(b'(2k)c'(2k)+jb'(2k+1)c'(2k+1))h(k) + z(k)$$
The $k^{\text{th}}$ component of the desired correlation is
$$\begin{align}r_\text{data}(k)(b'(2k)c'(2k) -jb'(2k+1)c'(2k+1)) = & \text{Re}(r_\text{data}(k))c'(2k)b'(2k) + \text{Im}(r_\text{data}(k))c'(2k+1)b'(2k+1) + \\
& j(\text{Im}(r_\text{data}(k))c'(2k)b'(2k) - \text{Re}(r_\text{data}(k))c'(2k+1)b'(2k+1))\end{align}$$
The receiver first applies the Gold sequence (`c_ptr`) to descramble the received
REs, splitting into components which will later allow for separation of the real and imaginary components (`r_ext` and `r_ext2`
in the code). These two components correspond to the real and imaginary parts of the $k^\text{th}$ component of the overall correlation shown above and are

$$r_\text{ext}[aa,l,k] = c'(2k) \cdot\text{Re}(r_\text{data}[aa,l,k]) +j c'(2k+1) \cdot\text{Im}(r_\text{data}[aa,l,k])$$

$$r_\text{ext2}[aa,l,k] = c'(2k) \cdot\text{Im}(r_\text{data}[aa,l,k]) - jc'(2k+1)\text{Re}(r_\text{data}[aa,l,k])$$
and are implemented using the `simde_mm_sign_epi16`, `oai_mm_conj` and `simde_mm_shuffle_epi8` SIMD methods on the received data samples.

For each candidate codeword
$\mathbf{b}$, the descrambled REs are correlated against the LUT and accumulated
into $Z$:

$$\begin{align} Z[g,aa](\mathbf{b}) \mathrel{+}= \sum_{l} \sum_{k \in \mathcal{K}_\text{data}(g)}
\bigl(&\mathrm{Re}(r_\text{ext}[aa,l,k]) \cdot b'[2k] + \mathrm{Im}(r_\text{ext}[aa,l,k]) \cdot b'[2k+1] + \bigr. \\
& j(\mathrm{Re}(r_\text{ext2}[aa,l,k]) \cdot b'[2k] + \mathrm{Im}(r_\text{ext2}[aa,l,k]) \cdot b'[2k+1]) \bigr)
\end{align}$$

where $L$ is the number of data symbols. After both DMRS initialisation and
data accumulation:

$$Z[g,aa](\mathbf{b}) \approx h_{g,aa} \cdot \bigl(L \cdot N_p + L \cdot N_d \cdot \mathbf{1}\{\mathbf{b} = \mathbf{b}_\text{tx}\}\bigr) + \text{noise}$$

#### ML metric and decision

The per-group squared magnitude is summed non-coherently across groups and
antennas:

$$\Lambda(\mathbf{b}) = \sum_{g,\,aa} \left\lvert Z[g,aa](\mathbf{b}) \right\rvert^2 \qquad \text{(no freq hop)}$$

$$\Lambda(\mathbf{b}) = \sum_{g,\,aa,\,\delta} \left\lvert Z[g,\delta,aa](\mathbf{b}) \right\rvert^2 \qquad \text{(freq hop, hop index } \delta\text{)}$$

Every codeword receives a baseline contribution of $|h|^2 (L N_p)^2$, while
the correct codeword gains an additional $|h|^2 L^2 N_d (2 N_p + N_d)$.
The decision is:

$$\hat{\mathbf{b}} = \arg\max_{\mathbf{b}}\; \Lambda(\mathbf{b})$$

### 4.6.1 Decoding — Short Blocks (3–11 bits), Format 3

Format 3 feeds the same LUT-based correlator as Format 2.  The differences are
in the coherence group geometry, the pilot type, and the data pre-processing
that precedes the descrambling step.

#### Group structure

The coherence group is one full PRB (12 subcarriers), so $N_g = 1$ and
$N_\text{group} = P$ (one group per PRB).  Within each group every subcarrier
carries either DM-RS or data depending on the symbol type:
$N_p = 12$ DM-RS REs and $N_d = 12$ data REs per group per symbol.

#### DM-RS pilot and channel reference

The pilot sequence is the low-PAPR sequence $r_{u,v,\alpha}[k]$
(TS 38.211 §6.3.2.6.3 / §6.4.1.3.2.2), generated with the same
group/cyclic-shift hopping as Format 1.  The channel reference per DMRS
symbol $l_\text{DM-RS}$ is:

$$H[g, l_\text{DM-RS}, aa] = \sum_{k=0}^{11} r_\text{DM-RS}[aa, l_\text{DM-RS}, 12g+k]\cdot r_{u,v,\alpha}^*[k]
\;\approx\; h_{g,aa} \cdot N_p$$

accumulated into `corr32[l_dmrs][g][aa]`.  Contributions from the 2 or 4
DMRS symbols (Section 4.2) are combined coherently to initialise $Z[g,aa]$,
exactly as in the Format 2 table of Section 4.6.

#### OCC despreading (Section 4.5)

Before the descrambling step, the received data symbols are OCC-despread via
IDFT.  Each group of (up to) 4 consecutive data symbols is stacked into the
IDFT input buffer with stride 4 (one sample per symbol per subcarrier), the
input is conjugated, and a length-$N_d$ DFT is applied.  The output is
transposed to produce a subcarrier-interleaved layout equivalent to the
single-symbol, frequency-domain data grid of Format 2.

#### Descrambling and codeword correlation

After IDFT despreading the Gold scrambling sequence (`c_ptr`) is applied
identically to Format 2 to produce `r_ext` and `r_ext2`:

$$r_\text{ext}[aa,l,k] = c'(2k)\cdot\text{Re}(r_\text{data}[aa,l,k]) + j\,c'(2k+1)\cdot\text{Im}(r_\text{data}[aa,l,k])$$

$$r_\text{ext2}[aa,l,k] = c'(2k)\cdot\text{Im}(r_\text{data}[aa,l,k]) - j\,c'(2k+1)\cdot\text{Re}(r_\text{data}[aa,l,k])$$

The codeword correlation, ML metric, and decision are then **identical** to
Section 4.6 with the Format 3 group parameters substituted:

| Parameter | Format 2 | Format 3 |
|-----------|----------|----------|
| Coherence group size | 2 PRBs | 1 PRB |
| $N_p$ per group | 8 | 12 |
| $N_d$ per group | 16 | 12 |
| Pilot $p[k]$ | Gold-sequence QPSK: $(1-2c(2k)+j(1-2c(2k+1)))/\sqrt{2}$ | Low-PAPR $r_{u,v,\alpha}[k]$ |
| Data pre-processing | None | IDFT despreading across 4 symbols |

### 4.7 Decoding — Polar Code (12–64 bits)

**LLR computation** (per 4-RE group):

For each group of 4 data REs and all 256 possible 8-bit partial codewords,
a correlation is computed between the received vector and the coded pattern.
LLR numerator and denominator for each bit $b$ are:

$$\lambda_b^+ = \max_{\mathbf{c}:\,c_b=1} \rho(\mathbf{c}), \qquad
  \lambda_b^- = \max_{\mathbf{c}:\,c_b=0} \rho(\mathbf{c})$$

$$\text{LLR}[b] = \lambda_b^+ - \lambda_b^-$$

where $\rho(\mathbf{c})$ is the per-group correlation energy. LUT tables
(`pucch2_3_polar_llr_num_lut`) pre-encode the bit-to-pattern mapping as SIMD
registers, enabling vectorised accumulation.

**Polar decoding:**

```c
polar_decoder_int16(llrs, decodedPayload, NR_POLAR_UCI_PUCCH_MESSAGE_TYPE, ...)
```

The decoded bit vector is bit-reversed to match the TS 38.212 interleaving
convention.

### 4.8 UCI Payload Extraction

The decoded bitstream is partitioned in order:

| Field | Length |
|-------|--------|
| HARQ-ACK | `bit_len_harq` bits |
| SR | 1 bit (if `sr_flag`) |
| CSI Part 1 | `bit_len_csi_part1` bits |
| CSI Part 2 | flagged in output bitmap |

---

## 5. Helper / Initialisation Functions

### `get_pucch0_cs_lut_index()` (lines 71–100)

Maintains a per-hopping-ID cache of cyclic-shift values for the entire frame.
On first call for a given `hoppingId`, iterates over all slots and symbols,
calls `nr_cyclic_shift_hopping()`, converts to an integer index (dividing by
$\pi/6$), and stores in a flat LUT.  Returns the cache slot index for use by
`nr_decode_pucch0`.

### `init_pucch2_3_luts()` (lines 1098–1129)

Called once at gNB startup.  Populates:

- `pucch2_3_lut[N-3][cw]` — QPSK-mapped small-block codewords
  for $N = 3,\ldots,11$ bits (8 to 2048 entries each).
- `pucch2_3_polar_llr_num_lut[256]` — 8-bit partial-codeword patterns packed
  into SIMD registers for use in the polar LLR computation.

### `nr_fill_pucch()` (lines 34–69)

Finds a free slot in `gNB->pucch[]`, copies the incoming
`nfapi_nr_pucch_pdu_t`, and allocates a beam index if beamforming is active.
Aborts with `AssertFatal` if the PUCCH queue is full.

### `nr_dump_uci_stats()` (lines 2001–2064)

Writes per-UE UCI counters (trial counts, DTX events, noise powers, SR
positive rates) to a file or stdout for monitoring.

---

## 6. Data Types and Key Structures

| Type | Description |
|------|-------------|
| `c16_t` | Complex 16-bit integer (`.r`, `.i`) |
| `c32_t` | Complex 32-bit integer |
| `c64_t` | Complex 64-bit integer |
| `cd_t` | Complex double (used in Format 1 equalization) |
| `cw_t` | Struct holding 16 $\times$ `c16_t` — one small-block codeword |
| `nfapi_nr_pucch_pdu_t` | FAPI input: format, PRBs, symbols, RNTI, hopping config, payload lengths |
| `nfapi_nr_uci_pucch_pdu_format_0_1_t` | UCI output for Formats 0 and 1 |
| `nfapi_nr_uci_pucch_pdu_format_2_3_4_t` | UCI output for Formats 2, 3, and 4 |
| `PHY_VARS_gNB` | gNB context; holds `rxdataF`, `pucch[]` queue, thresholds, statistics |

Fixed-point arithmetic is used throughout.  Powers-of-two shifts replace
division.  SIMD acceleration (AVX2/SSE via the `simde` portability layer) is
used in the Format 2/3 scrambling removal, LLR accumulation, and FFT transpose
loops.  Format 1 equalization uses `cd_t` (double) for the final channel
estimate and hypothesis metrics.

---

## 7. Standards References

| Reference | Scope |
|-----------|-------|
| TS 38.211 §5.2.2 | Low-PAPR base sequences |
| TS 38.211 §6.3.2.2 | Group and sequence hopping |
| TS 38.211 §6.3.2.3 | PUCCH Format 0 sequence |
| TS 38.211 §6.3.2.4 | PUCCH Format 1 (OCC, spreading) |
| TS 38.211 §6.3.2.5 | PUCCH Format 2 |
| TS 38.211 §6.3.2.6 | PUCCH Format 3 |
| TS 38.211 §6.4.1.3 | PUCCH DM-RS |
| TS 38.212 §6.3 | UCI channel coding (polar, Reed-Muller) |
| TS 38.213 §9 | PUCCH procedures and resource allocation |

Key tables used in the implementation:

| Symbol | Standard table |
|--------|---------------|
| `table_5_2_2_2_2` | Base sequences (TS 38.211 Table 5.2.2.2-2) |
| `table_6_3_2_4_1_1_N_SF_mprime_PUCCH_1_noHop` | OCC spreading factors, no hop (TS 38.211 Table 6.3.2.4.1-1) |
| `table_6_3_2_4_1_1_N_SF_mprime_PUCCH_1_m0Hop` | OCC spreading factors, hop $m'=0$ |
| `table_6_3_2_4_1_2_Wi` | OCC weights (TS 38.211 Table 6.3.2.4.1-2) |
