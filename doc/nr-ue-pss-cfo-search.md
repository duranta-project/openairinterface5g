# NR UE blind CFO-PSS search

The NR UE can optionally perform a joint carrier-frequency-offset (CFO), PSS
sequence and timing search during initial synchronization. The feature is
intended for blind acquisition when the initial CFO can exceed the capture
range of the original PSS detector.

The search is disabled by default. When disabled, the existing initial
synchronization path is unchanged.

## Algorithm

The search has two stages:

1. A coarse grid evaluates all three `NID2` sequences and selects CFO, PSS and
   timing jointly.
2. A fine grid keeps the winning `NID2` and refines CFO and timing around the
   coarse result.

The default coarse grid covers `-60` to `+60 kHz` in `5 kHz` steps. The
default fine grid covers `-6` to `+6 kHz` around the coarse winner in `400 Hz`
steps. This evaluates 75 coarse hypotheses and 31 fine hypotheses. The final
CFO is applied once before the existing SSS and PBCH processing. SSS and the
PBCH CRC remain responsible for validating cell acquisition.

The implementation processes the native fixed-point UE receive buffers and
uses the existing OAI PSS references. It does not require the true channel CFO
as an input.

## UE command-line options

| Option | Default | Description |
|---|---:|---|
| `--ue-pss-cfo-search` | disabled | Enable the joint CFO-PSS search. |
| `--ue-pss-cfo-coarse-span` | `60000` | Coarse-grid half-span in Hz. |
| `--ue-pss-cfo-coarse-step` | `5000` | Coarse-grid step in Hz. |
| `--ue-pss-cfo-fine-span` | `6000` | Fine-grid half-span in Hz. |
| `--ue-pss-cfo-fine-step` | `400` | Fine-grid step in Hz. |

`--ue-pss-cfo-search` requires `--ue-fo-compensation`. For example:

```bash
sudo ./nr-uesoftmodem <existing options> \
  --ue-fo-compensation \
  --ue-pss-cfo-search
```

Both grids must contain zero, their steps must be positive, and neither grid
may exceed 257 bins. Invalid configurations stop at UE initialization with a
descriptive error.

## Testing

Five unit-test targets cover configuration validation, a single CFO
hypothesis, coarse search, fine search and the complete Stage 1 pipeline:

```text
test_nr_pss_cfo_search_config
test_nr_pss_cfo_hypothesis
test_nr_pss_cfo_coarse_search
test_nr_pss_cfo_fine_search
test_nr_pss_cfo_stage1
```

`nr_pbchsim -Q` enables the same Stage 1 path in the PHY simulator. Functional
validation should check the estimated CFO and `NID2`, and must continue
through SSS and successful PBCH decoding. A PSS candidate alone is not a cell
acquisition result.
