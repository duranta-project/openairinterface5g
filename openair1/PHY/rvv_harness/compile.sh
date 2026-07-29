#!/usr/bin/env bash
# Cross-compile the RVV bring-up harness for the RISC-V target, then scp the
# resulting binary to the board and run it there (no qemu in this environment).
#
#   ./build.sh                 # build with RVV (default march, matches OAI)
#   ./build.sh norvv           # build scalar-only (no 'v') as a compile smoke test
#   MARCH=rv64gcv_zvl256b ./build.sh   # override march (e.g. pin min VLEN)
#
# Env (same discovery as cmake_targets/cross-riscv.cmake):
#   K3_SYSROOT / RISCV_SYSROOT   target sysroot   (default: $HOME/sysroots/k3)
#   CC                           cross gcc        (default: spacemit riscv gcc)
set -euo pipefail
cd "$(dirname "$0")"

SYSROOT="${K3_SYSROOT:-${RISCV_SYSROOT:-$HOME/sysroots/k3}}"
CC="${CC:-$HOME/opt/toolchains/spacemit-riscv/bin/riscv64-unknown-linux-gnu-gcc}"
MULTIARCH="riscv64-linux-gnu"

if [ "${1:-}" = "norvv" ]; then
  MARCH="${MARCH:-rv64gc_zba_zbb_zbs_zicond}"
else
  MARCH="${MARCH:-rv64gcv_zba_zbb_zbc_zbs_zicond}"
fi

[ -x "$CC" ] || { echo "cross gcc not found: $CC (set CC=...)"; exit 1; }
[ -d "$SYSROOT" ] || { echo "sysroot not found: $SYSROOT (set K3_SYSROOT=...)"; exit 1; }

# The K3 Debian/Bianbu sysroot keeps glibc headers under the multiarch dir; the
# compiler does not search it from --sysroot alone (see cross-riscv.cmake).
INCFLAGS=""
[ -d "$SYSROOT/usr/include/$MULTIARCH" ] && INCFLAGS="-isystem$SYSROOT/usr/include/$MULTIARCH"
LIBFLAGS=""
[ -d "$SYSROOT/usr/lib/$MULTIARCH" ] && LIBFLAGS="-B$SYSROOT/usr/lib/$MULTIARCH -L$SYSROOT/usr/lib/$MULTIARCH"

for t in rvv_cpx_mult_test rvv_chcomp_test rvv_llr_test rvv_c16mult_test rvv_multadd_test rvv_rotate_test rvv_precoder_test crc_clmul_test wto_test interleave_test dft_bfly_test dft_transpose_test dft_leaf_test dft_fft_test vsse8_test dft_4step_test rvv_unscramble_test rvv_multcplx_test rvv_chlevel_test rvv_ldpc_interleave_test rvv_sigenergy_test rvv_csi_test rvv_qpsk2layer_test rvv_qam16_2layer_test rvv_qam64_2layer_test rvv_qam64_lbest_test; do
  set -x
  "$CC" --sysroot="$SYSROOT" -march="$MARCH" -mabi=lp64d -O2 -Wall -Wextra \
    $INCFLAGS $LIBFLAGS \
    "$t.c" -o "$t" -lm
  set +x
done
echo "built in $(pwd)   (march=$MARCH):  rvv_cpx_mult_test  rvv_chcomp_test"
echo "run on target:  scp rvv_* <board>: && ssh <board> './rvv_chcomp_test 8'"
