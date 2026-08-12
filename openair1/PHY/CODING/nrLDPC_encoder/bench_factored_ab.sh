#!/bin/bash
# A/B the factored LDPC encoder against the stock one, via ldpctest.
#
#   ./bench_factored_ab.sh [path-to-openairinterface5g]
#
# Rebuilds only libldpc.so (a runtime-loaded shared module) between the two
# configurations, so nothing else is touched and no relink is needed. The
# source edit is reverted on exit, including on interrupt.
#
# Optional environment:
#   CORE=<n>    pin ldpctest to a CPU. Strongly recommended: unpinned runs on a
#               many-core machine drift ~10% from scheduler migration alone,
#               which is larger than some of the effects being measured.
#   REPS=<n>    repetitions per configuration (default 3)
#   TRIALS=<n>  ldpctest -n (default 200)

set -u
OAI=${1:-$HOME/openairinterface5g}
BUILD=$OAI/cmake_targets/ran_build/build
EPC=$OAI/openair1/PHY/CODING/nrLDPC_encoder/ldpc_encode_parity_check.c
REPS=${REPS:-3}
TRIALS=${TRIALS:-200}
PIN=""
[ -n "${CORE:-}" ] && command -v taskset >/dev/null && PIN="taskset -c $CORE"

[ -f "$EPC" ]   || { echo "no $EPC -- pass the openairinterface5g path as \$1"; exit 1; }
[ -d "$BUILD" ] || { echo "no build dir $BUILD -- configure and build ldpctest first"; exit 1; }
# pick the generator from the build directory, not from what is on PATH:
# some boards have ninja installed but a Makefile-configured build tree
if [ -f "$BUILD/build.ninja" ]; then GEN="ninja"
elif [ -f "$BUILD/Makefile" ]; then GEN="make -j$(nproc)"
else echo "no build.ninja or Makefile in $BUILD -- configure the build first"; exit 1; fi

MARK='#define NO_FACTORED 1 // bench_factored_ab'
cleanup() { grep -vxF "$MARK" "$EPC" > "$EPC.abtmp" && mv "$EPC.abtmp" "$EPC"; }
trap cleanup EXIT INT TERM
cleanup

echo "host     : $(hostname)"
echo "cpu      : $(lscpu 2>/dev/null | grep -m1 'Model name' | cut -d: -f2- | sed 's/^ *//')"
echo "           $(grep -m1 'model name' /proc/cpuinfo 2>/dev/null | cut -d: -f2- | sed 's/^ *//')"
echo "cores    : $(nproc)   compiler: $(${CC:-cc} --version | head -1)"
echo "pinning  : ${PIN:-NONE -- results will be noisy, set CORE=<n>}"
echo "reps     : $REPS x $TRIALS trials"
echo

CASES=(
  "BG1_K8448_r1/3:8448:1:3"
  "BG1_K8448_r2/3:8448:2:3"
  "BG1_K8448_r22/25:8448:22:25"
  "BG2_K3840_r1/5:3840:1:5"
  "BG2_K3840_r1/3:3840:1:3"
  "BG2_K3840_r2/3:3840:2:3"
)

build() {
  cleanup
  [ "$1" = STOCK ] && { printf '%s\n' "$MARK" | cat - "$EPC" > "$EPC.abtmp" && mv "$EPC.abtmp" "$EPC"; }
  # ldpc_encode_parity_check.c is #included, not compiled on its own. Ninja
  # tracks that dependency; some Makefile-configured trees do not, and silently
  # reuse the previous object -- which makes the A/B compare a build against
  # itself and report 1.00x. Touch the real translation units to force it.
  touch "$OAI"/openair1/PHY/CODING/nrLDPC_encoder/ldpc_encoder_optim8segmulti.c \
        "$OAI"/openair1/PHY/CODING/nrLDPC_encoder/ldpc_encoder.c 2>/dev/null
  ( cd "$BUILD" && $GEN libldpc.so >/dev/null 2>&1 ) ||
  ( cd "$BUILD" && $GEN ldpctest   >/dev/null 2>&1 ) || { echo "BUILD FAILED ($1)"; exit 1; }
}

# one pass over every case; fills PAR[] and TOT[] (mean us over REPS)
declare -A PAR TOT
sweep() {
  local cfg=$1 c tag K NUM DEN out p t ap at n
  for c in "${CASES[@]}"; do
    IFS=: read -r tag K NUM DEN <<<"$c"
    ap=0; at=0; n=0
    for _ in $(seq "$REPS"); do
      out=$($PIN "$BUILD/ldpctest" -l "$K" -r "$NUM" -d "$DEN" -n "$TRIALS" -s 4 2>&1)
      p=$(awk '/ldpc_encoder_optim\(parity\)/{gsub(/ us;/,"");s+=$2;c++} END{if(c)printf "%.4f",s/c}' <<<"$out")
      t=$(awk '/^ *ldpc_encoder_optim:/       {gsub(/ us;/,"");s+=$2;c++} END{if(c)printf "%.4f",s/c}' <<<"$out")
      [ -n "$p" ] && { ap=$(awk -v a="$ap" -v b="$p" 'BEGIN{print a+b}'); n=$((n+1)); }
      [ -n "$t" ] &&   at=$(awk -v a="$at" -v b="$t" 'BEGIN{print a+b}')
    done
    if [ "$n" -gt 0 ]; then
      PAR[$cfg,$tag]=$(awk -v s="$ap" -v n="$n" 'BEGIN{printf "%.3f",s/n}')
      TOT[$cfg,$tag]=$(awk -v s="$at" -v n="$n" 'BEGIN{printf "%.3f",s/n}')
    else
      PAR[$cfg,$tag]=n/a; TOT[$cfg,$tag]=n/a
    fi
  done
}

echo "building stock ..."   ; build STOCK
MD5_STOCK=$(md5sum "$BUILD/libldpc.so" 2>/dev/null | cut -c1-12)
sweep STOCK
echo "building factored ..."; build FACTORED
MD5_FAC=$(md5sum "$BUILD/libldpc.so" 2>/dev/null | cut -c1-12)
if [ -n "$MD5_STOCK" ] && [ "$MD5_STOCK" = "$MD5_FAC" ]; then
  echo
  echo "ERROR: libldpc.so is identical for both configurations ($MD5_STOCK)."
  echo "The toggle did not take effect, so any numbers below would be bogus."
  exit 1
fi
sweep FACTORED
echo

report() { # $1 = PAR|TOT, $2 = label
  local -n M=$1
  printf '%-20s %9s %9s %9s\n' "$2 (us)" stock factored speedup
  local c tag
  for c in "${CASES[@]}"; do
    IFS=: read -r tag _ _ _ <<<"$c"
    printf '%-20s %9s %9s %9s\n' "$tag" "${M[STOCK,$tag]}" "${M[FACTORED,$tag]}" \
      "$(awk -v a="${M[STOCK,$tag]}" -v b="${M[FACTORED,$tag]}" \
         'BEGIN{if(b+0>0) printf "%.2fx", a/b; else print "n/a"}')"
  done
  echo
}
report PAR "parity generation"
report TOT "encoder total"

echo -n "correctness (factored, BLER at 4 dB): "
$PIN "$BUILD/ldpctest" -l 8448 -r 1 -d 3 -n 100 -s 4 2>&1 | grep -m1 BLER || echo "no BLER line"
