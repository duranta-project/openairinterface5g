# SPDX-License-Identifier: LicenseRef-CSSL-1.0
"""Build an RF-simulator image from native Linux binaries and their libraries."""
import argparse
import hashlib
import json
from pathlib import Path
import re
import shutil
import subprocess
import tempfile

p = argparse.ArgumentParser(description=__doc__)
p.add_argument('--build', type=Path, required=True)
p.add_argument('--output', type=Path, required=True)
p.add_argument('--tag', default='sib9-basic:local')
a = p.parse_args()
build = a.build.resolve()
a.output.mkdir(parents=True, exist_ok=True)
records = {}
with tempfile.TemporaryDirectory(prefix="sib9-runtime-", dir=a.output) as temporary:
    context = Path(temporary)
    root = context / 'rootfs'
    (root / 'tmp').mkdir(parents=True)
    inputs = [build / name for name in ('nr-softmodem', 'nr-uesoftmodem',
              'librfsimulator.so', 'libparams_libconfig.so', 'libdfts.so', 'libldpc.so')]
    copied = set()
    for path in inputs:
        target = root / 'oai' / path.name
        target.parent.mkdir(parents=True, exist_ok=True)
        shutil.copyfile(path, target)
        records[str(path)] = hashlib.sha256(path.read_bytes()).hexdigest()
        result = subprocess.run(['ldd', str(path)], text=True, capture_output=True, check=True)
        if 'not found' in result.stdout:
            raise SystemExit(result.stdout)
        for library in re.findall(r'(?:=>\s+)?(/[^\s]+)\s+\(', result.stdout):
            if library in copied:
                continue
            copied.add(library)
            dest = root / library.lstrip('/')
            dest.parent.mkdir(parents=True, exist_ok=True)
            shutil.copyfile(library, dest)
            records[library] = hashlib.sha256(Path(library).read_bytes()).hexdigest()
    (context / 'Dockerfile').write_text('FROM scratch\nCOPY --chmod=755 rootfs/ /\nWORKDIR /oai\n'
        'ENV LD_LIBRARY_PATH=/oai\nCMD ["/oai/nr-softmodem"]\n')
    subprocess.run(['docker', 'build', '--network=none', '-t', a.tag, str(context)], check=True)
    image = json.loads(subprocess.check_output(['docker', 'image', 'inspect', a.tag]))[0]
    record = {'image': a.tag, 'id': image['Id'], 'build': str(build), 'files': records,
              'limits': 'Custom native-runtime image for RF simulation, not the upstream production Dockerfile'}
    (a.output / 'runtime-image.json').write_text(json.dumps(record, indent=2) + '\n')
print(image['Id'])
