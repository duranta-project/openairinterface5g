#!/bin/bash
# 256QAM 2-layer reduced-L sweep (analysis; companion to nr_mimo_lbest_detector.md sec 6a).
#
# Question: with the hot -2 ML log2_maxh, does the L-best REDUCED search track full-ML (L=256)
# on TDL-A?  If L256=64/32/16 tracks L256=256 in BLER, that is the 256QAM complexity win
# (256QAM ML is float/analysis-only today; full search is O(256) per target per RE, no SIMD kernel).
#
# Path gate (UE DL 2-layer 256QAM ML) needs BOTH:
#   -E            -> ue->do_ml (master ML switch)
#   OAI_LBEST=1   -> ml256 gate routing Qm=8 to nr_qam256_llr_2layer_lbest (float)
#   OAI_LBEST_L256=<L>  -> candidate count (256 = full ML; smaller = reduced search)
# 256QAM multi-layer needs >=4 RX to converge -> 4T4R (-y4 -z4).  -q1 = 256QAM MCS table (MCS 20-27).
# ML log2_maxh offset defaults to -2 (hot); override via OAI_ML_MAXH_OFF.
#
# Override from the env, e.g.:  NR_DLSIM=/path/to/nr_dlsim MCS=20 SNR=22 N=1000 CH=C ./q256_L_sweep.sh
B=${NR_DLSIM:-/home/jovyan/openairinterface5g_x86_A100/build/nr_dlsim}
OUT=${OUT:-/tmp/q256_sweep}          # logs go here (keeps the source tree clean); override with OUT=
MCS=${MCS:-22}                       # -q1 256QAM table
SNR=${SNR:-22}                       # sweep start (256QAM 2-layer 4T4R TDL-A crossing ~ mid/high 20s dB)
N=${N:-50}                           # trials/SNR (bump to 1000 for citable numbers)
CH=${CH:-A}                          # TDL model (A/C)
mkdir -p "$OUT"
COMMON="-x2 -y4 -z4 -q1 -e${MCS} -Q32 -g${CH} -n${N} -s${SNR}"
echo "binary: $B"
echo "COMMON: $COMMON   (ML log2_maxh off = ${OAI_ML_MAXH_OFF:-default -2})   logs -> $OUT"

# full-ML reference (L=256) then reduced L
for L in 256 64 32 16; do
  OAI_LBEST=1 OAI_LBEST_L256=$L $B $COMMON -E > "$OUT/q256_L${L}.log" 2>&1
  echo "L256=$L done"
done
# MMSE baseline for context (no -E, no OAI_LBEST)
$B $COMMON > "$OUT/q256_mmse.log" 2>&1
echo "MMSE done"

echo "===================== SUMMARY (round-1 BLER / Eff Throughput vs SNR) ====================="
for tag in 256 64 32 16 mmse; do
  f="$OUT/q256_L${tag}.log"; [ "$tag" = mmse ] && f="$OUT/q256_mmse.log"
  label=$([ "$tag" = mmse ] && echo "MMSE" || echo "L256=$tag")
  echo "----- ${label} -----"
  grep "Channel BLER" "$f" 2>/dev/null \
    | sed -E 's/SNR ([0-9.]+).*BLER \(([0-9.e+-]+),.*Eff Throughput ([0-9.]+).*/  SNR \1  BLER1=\2  Thr=\3/'
done
echo "ALL DONE"
