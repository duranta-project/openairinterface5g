#!/bin/bash
# Does the startup calibration pick the width that is actually fastest under
# sustained load?
#
#   ./check_width_choice.sh [path-to-openairinterface5g]
#
# The calibration in ldpc_encode_parity_check.c measures for ~0.6 ms at module
# load. On parts where AVX-512 frequency licensing matters, that may be too short
# to reach the steady state a real workload runs in -- so it could choose 512-bit
# where 256-bit is better sustained. This forces each width in turn and compares.
#
# Optional environment:
#   BUILD=<dir> build directory, if not $OAI/cmake_targets/ran_build/build
#   CORE=<n>    pin ldpctest to a CPU. Strongly recommended.
#   REPS=<n>    repetitions per width (default 3)
#   TRIALS=<n>  ldpctest -n (default 300)

set -u
OAI=${1:-$HOME/openairinterface5g}
BUILD=${BUILD:-$OAI/cmake_targets/ran_build/build}
REPS=${REPS:-3}
TRIALS=${TRIALS:-300}
PIN=""
[ -n "${CORE:-}" ] && command -v taskset >/dev/null && PIN="taskset -c $CORE"

[ -x "$BUILD/ldpctest" ] || { echo "no ldpctest at $BUILD/ldpctest"; echo "build it: (cd $BUILD && make -j\$(nproc) ldpc ldpctest)"; exit 1; }

echo "host     : $(hostname)"
echo "cpu      : $(lscpu 2>/dev/null | grep -m1 'Model name' | cut -d: -f2- | sed 's/^ *//')"
echo "pinning  : ${PIN:-NONE -- set CORE=<n>}"
echo "reps     : $REPS x $TRIALS trials"
echo

echo "== what the startup calibration measures and chooses =="
CAL=$(OAI_LDPC_SIMD_VERBOSE=1 $PIN "$BUILD/ldpctest" -l 8448 -r 1 -d 3 -n 1 -s 10 2>&1 |
      grep -m1 'ldpc: calibrated')
if [ -n "$CAL" ]; then
  echo "  $CAL"
else
  echo "  no calibration line -- this build has only the 128-bit encoders"
  echo "  (not x86 with AVX2+, or NO_FACTORED). The sweep below is then VACUOUS:"
  echo "  all three forced widths run the same code, so any 'winner' is noise."
fi
echo

# mean parity time over REPS runs at a forced width
meas() { # $1=width $2=K' $3=num $4=den
  local acc=0 n=0 v
  for _ in $(seq "$REPS"); do
    v=$(OAI_LDPC_SIMD_WIDTH=$1 $PIN "$BUILD/ldpctest" -l "$2" -r "$3" -d "$4" -n "$TRIALS" -s 4 2>&1 |
        awk '/ldpc_encoder_optim\(parity\)/{gsub(/ us;/,"");s+=$2;c++} END{if(c)printf "%.4f",s/c}')
    [ -n "$v" ] && { acc=$(awk -v a="$acc" -v b="$v" 'BEGIN{print a+b}'); n=$((n+1)); }
  done
  [ "$n" -gt 0 ] && awk -v s="$acc" -v n="$n" 'BEGIN{printf "%.3f",s/n}' || echo "n/a"
}

for cfg in "BG1 K'=8448 r1/3:8448:1:3" "BG2 K'=3840 r1/5:3840:1:5"; do
  IFS=: read -r tag K NUM DEN <<<"$cfg"
  echo "== $tag, parity generation (us) =="
  best=""; bestw=""
  for w in 128 256 512; do
    t=$(meas "$w" "$K" "$NUM" "$DEN")
    printf "  forced %3s-bit : %s\n" "$w" "$t"
    if [ "$t" != "n/a" ]; then
      if [ -z "$best" ] || awk -v a="$t" -v b="$best" 'BEGIN{exit !(a<b)}'; then best=$t; bestw=$w; fi
    fi
  done
  a=$(meas 0 "$K" "$NUM" "$DEN")   # 0 is invalid, so the calibrated choice is used
  printf "  calibrated     : %s\n" "$a"
  echo "  fastest forced : ${bestw}-bit at $best"
  if [ "$a" != "n/a" ] && [ "$best" != "n/a" ]; then
    awk -v c="$a" -v b="$best" 'BEGIN{
      if (c > b*1.03) printf "  => calibration is %.1f%% off the best width\n", 100*(c/b-1);
      else printf "  => calibration matches the best width\n"}'
  fi
  echo
done
