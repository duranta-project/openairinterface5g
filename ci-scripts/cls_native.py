# SPDX-License-Identifier: LicenseRef-CSSL-1.0

import logging
import re

import cls_cmd
from cls_ci_helper import archiveArtifact

DPDK_PATH = '/opt/dpdk-t2-22.11.0'

class Native():

	def Build(ctx, node, HTML, directory, options):
		logging.debug(f'Building on server: {node}')

		with cls_cmd.getConnection(node) as ssh:
			base = f"{directory}/cmake_targets"
			ret = ssh.run(f"C_INCLUDE_PATH={DPDK_PATH}/include/ PKG_CONFIG_PATH={DPDK_PATH}/lib64/pkgconfig/ {base}/build_oai {options} > {base}/build_oai.log", timeout=900)
			success = ret.returncode == 0
			logs = ssh.run(f"cat {base}/build_oai.log", silent=True)
			logging.debug(f"build finished with code {ret.returncode}")

			archiveArtifact(ssh, ctx, f'{base}/build_oai.log')

			# check if build artifacts are there
			# NOTE: build_oai should fail with exit code if it could not build, but it does not
			build_dir = f"{base}/ran_build/build"
			build_gnb = re.search('--gNB', options) is not None
			if build_gnb:
				success = success and ssh.run(f"ls {build_dir}/nr-softmodem").returncode == 0
			build_enb = re.search('--eNB', options) is not None
			if build_enb:
				success = success and ssh.run(f"ls {build_dir}/lte-softmodem").returncode == 0

		if success:
			logging.info('\u001B[1m Building OAI Pass\u001B[0m')
			HTML.CreateHtmlTestRowQueue(options, 'OK', [])
		else:
			logging.error('\u001B[1m Building OAI Failed\u001B[0m')
			HTML.CreateHtmlTestRowQueue(options, 'KO', [])
		return success
