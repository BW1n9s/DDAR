"""Package only the experimental DLL, disabled config, test guide and licenses."""
import argparse
import configparser
import hashlib
import json
import zipfile
from pathlib import Path


def main():
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument('--dev-root', type=Path, default=Path('F:/DDAR-Dev'))
    ap.add_argument('--output', type=Path)
    args = ap.parse_args()
    root = args.dev_root.resolve()
    output = (args.output or root / 'Packages/DDAR-Phase0-0.0.2-test.zip').resolve()
    if not output.is_relative_to(root): ap.error('Output must remain inside the confirmed development root')
    repo = Path(__file__).resolve().parents[1]
    dll = root / 'Workspace/stage/DDARProbe/SKSE/Plugins/DDARProbe.dll'
    ini = repo / 'config/DDARExperiment.ini'
    config = configparser.ConfigParser(interpolation=None)
    config.read(ini, encoding='utf-8')
    for flag in ('EnableExperiment', 'AllowExtraTargetSlot52'):
        if config['Experiment'][flag] != '0': raise ValueError('Package defaults must remain disabled and strict')
    files = [(dll, 'SKSE/Plugins/DDARProbe.dll'), (ini, 'SKSE/Plugins/DDARExperiment.ini'),
             (repo / 'docs/phase0-experiment.md', 'README-TEST.zh-CN.md'),
             (root / 'Dependencies/CommonLibSSE-NG/LICENSE', 'licenses/CommonLibSSE-NG.txt')]
    for name in ('minhook', 'fmt', 'spdlog', 'rapidcsv'):
        files.append((root / f'Dependencies/installed/x64-windows-static-md/share/{name}/copyright', f'licenses/{name}.txt'))
    for path, _ in files:
        if not path.is_file(): raise FileNotFoundError(path)
    manifest = {'version': '0.0.2-phase0-experiment', 'defaultEnabled': False,
                'strictSlotPolicy': True, 'inGameTested': False,
                'dllSHA256': hashlib.sha256(dll.read_bytes()).hexdigest(),
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
