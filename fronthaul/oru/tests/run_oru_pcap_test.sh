#!/bin/bash
# SPDX-License-Identifier: MIT
set -e

# This script downloads a PCAP file, unarchives it, and then runs a test executable.
#
# Arguments:
# $1: PCAP_DOWNLOAD_URL        - The URL to download the .xz archive from.
# $2: TEST_ORU_PCAP_EXECUTABLE - The full path to the test executable (e.g., test_oru_pcap).
# $3+: Extra arguments to the executable

DOWNLOAD_URL="$1"
shift
TEST_EXECUTABLE="$1"
shift
# Collect extra arguments
EXTRA_ARGS=()
while [[ $# -gt 0 ]]; do
	EXTRA_ARGS+=("$1")
	shift
done

ARCHIVE_NAME="compressed_pcap.xz"
EXTRACTED_FILENAME="uncompressed.pcap"
curl -L $DOWNLOAD_URL --output $ARCHIVE_NAME
unxz -c $ARCHIVE_NAME > $EXTRACTED_FILENAME



echo "Running test ${TEST_EXECUTABLE} with PCAP ${DOWNLOAD_URL} and arguments: ${EXTRA_ARGS[*]}"
exec "${TEST_EXECUTABLE}" "${EXTRACTED_FILENAME}" "${EXTRA_ARGS[@]}"
