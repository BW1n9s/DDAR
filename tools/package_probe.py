"""Package the working MCM test interface and its experimental rendering backend."""
import argparse
import hashlib
import json
import zipfile
from pathlib import Path
from verify_mcm import verify


def main():
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument('--dev-root', type=Path, default=Path('F:/DDAR-Dev'))
    ap.add_argument('--output', type=Path)
    args = ap.parse_args()
    root = args.dev_root.resolve()
    output = (args.output or root / 'Packages/DDAR-MCM-0.0.3-test.zip').resolve()
    if not output.is_relative_to(root): ap.error('Output must remain inside the confirmed development root')
    repo = Path(__file__).resolve().parents[1]
    stage = root / 'Workspace/stage/DDARProbe'
    verify(stage, repo)
    dll = stage / 'SKSE/Plugins/DDARProbe.dll'
    files = [(stage / name, name) for name in ('SKSE/Plugins/DDARProbe.dll', 'DDAR.esp', 'SEQ/DDAR.seq',
             'Scripts/DDAR_Native.pex', 'Scripts/DDAR_MCM.pex', 'Scripts/DDAR_PlayerAlias.pex')]
    files += [(repo / 'docs/mcm-test.md', 'README-TEST.zh-CN.md'),
             (repo / 'third_party/DDNG/LICENSE', 'licenses/DDNG-API.txt'),
             (root / 'Dependencies/CommonLibSSE-NG/LICENSE', 'licenses/CommonLibSSE-NG.txt')]
    for name in ('minhook', 'fmt', 'spdlog', 'rapidcsv'):
        files.append((root / f'Dependencies/installed/x64-windows-static-md/share/{name}/copyright', f'licenses/{name}.txt'))
    for path, _ in files:
        if not path.is_file(): raise FileNotFoundError(path)
    manifest = {'version': '0.0.3-mcm-test', 'defaultEnabled': False, 'hasMCM': True,
                'strictSlotPolicy': True, 'inGameTested': False,
                'dllSHA256': hashlib.sha256(dll.read_bytes()).hexdigest(),
                'sha256': {name: hashlib.sha256(path.read_bytes()).hexdigest() for path, name in files},
                'files': [name for _, name in files]}
    output.parent.mkdir(parents=True, exist_ok=True)
    with zipfile.ZipFile(output, 'w', compression=zipfile.ZIP_DEFLATED) as archive:
        for path, name in files: archive.write(path, name)
        archive.writestr('manifest.json', json.dumps(manifest, indent=2))
    with zipfile.ZipFile(output) as archive:
        if archive.testzip(): raise ValueError('Archive CRC verification failed')
        if sorted(archive.namelist()) != sorted(manifest['files'] + ['manifest.json']):
            raise ValueError('Unexpected package entries')
    print(json.dumps({'package': str(output), 'size': output.stat().st_size, **manifest}, indent=2))


if __name__ == '__main__': main()
