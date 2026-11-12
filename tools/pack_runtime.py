import argparse
import hashlib
import json
import os
import re
import struct
import tempfile
from pathlib import Path

MAGIC = b'SAIPAK1\0'
FORMAT_VERSION = 1
RUNTIME_VERSION = '153.7'
FLAG_NATIVE_LIBRARY = 1
HEADER = struct.Struct('<8sIIHH32s')
ENTRY = struct.Struct('<HHQ32s')
CHUNK_SIZE = 1024 * 1024
VERSION_PATTERN = re.compile(r'[A-Za-z0-9._-]+\Z')


def read_source_manifest(source_directory):
    manifest_path = source_directory / 'manifest.json'
    if not manifest_path.is_file():
        raise ValueError(f'Missing runtime manifest: {manifest_path}')

    manifest = json.loads(manifest_path.read_text(encoding='utf-8'))
    version = manifest.get('version')
    architectures = {value.lower() for value in manifest.get('accept_arch', [])}

    if not isinstance(version, str) or not VERSION_PATTERN.fullmatch(version):
        raise ValueError('The runtime manifest has an invalid version.')
    if not {'x64', 'x86_64', 'x86_64h'} & architectures:
        raise ValueError('The runtime manifest does not list an x64 architecture.')
    if not (source_directory / 'chrome_screen_ai.dll').is_file():
        raise ValueError('The runtime directory has no chrome_screen_ai.dll.')

    return version


def hash_file(path):
    digest = hashlib.sha256()
    size = 0

    with path.open('rb') as source:
        while chunk := source.read(CHUNK_SIZE):
            digest.update(chunk)
            size += len(chunk)

    return size, digest.digest()


def collect_files(source_directory):
    files = []
    path_keys = set()
    dll_names = set()

    for path in sorted(source_directory.rglob('*')):
        if path.is_symlink():
            raise ValueError(f'Symbolic links are not supported: {path}')
        if not path.is_file():
            continue

        relative_path = path.relative_to(source_directory).as_posix()
        relative_bytes = relative_path.encode('utf-8')
        path_key = relative_path.casefold()

        if not relative_path or relative_path.startswith('/') or '..' in relative_path.split('/'):
            raise ValueError(f'Unsafe runtime path: {relative_path}')
        if len(relative_bytes) > 65535:
            raise ValueError(f'Runtime path is too long: {relative_path}')
        if path_key in path_keys:
            raise ValueError(f'Case-insensitive path collision: {relative_path}')
        path_keys.add(path_key)

        is_native_library = path.suffix.lower() == '.dll'
        if is_native_library:
            dll_name = path.name.casefold()
            if dll_name in dll_names:
                raise ValueError(f'Duplicate native DLL filename: {path.name}')
            dll_names.add(dll_name)

        size, digest = hash_file(path)
        files.append({
            'path': path,
            'relative_path': relative_path,
            'relative_bytes': relative_bytes,
            'size': size,
            'digest': digest,
            'flags': FLAG_NATIVE_LIBRARY if is_native_library else 0,
        })

    if not files:
        raise ValueError('The runtime directory contains no files.')

    return files


def calculate_package_id(version, files):
    digest = hashlib.sha256()
    digest.update(version.encode('utf-8'))
    digest.update(b'\0')

    for entry in files:
        digest.update(entry['relative_bytes'])
        digest.update(b'\0')
        digest.update(struct.pack('<Q', entry['size']))
        digest.update(entry['digest'])

    return digest.digest()


def write_package(source_directory):
    source_directory = source_directory.resolve(strict=True)
    manifest_version = read_source_manifest(source_directory)

    if manifest_version != RUNTIME_VERSION:
        raise ValueError(
            f'This SDK is locked to ScreenAI {RUNTIME_VERSION}, not {manifest_version}.'
        )

    version = RUNTIME_VERSION
    output_path = (
        Path(__file__).resolve().parents[1] /
        'runtime' /
        f'screenai-runtime-{RUNTIME_VERSION}.sai'
    ).resolve()

    try:
        output_path.relative_to(source_directory)
    except ValueError:
        pass
    else:
        raise ValueError('The package output must be outside the runtime source directory.')

    files = collect_files(source_directory)
    version_bytes = version.encode('utf-8')
    if len(version_bytes) > 65535:
        raise ValueError('The runtime version string is too long.')

    package_id = calculate_package_id(version, files)
    output_path.parent.mkdir(parents=True, exist_ok=True)
    file_descriptor, temporary_name = tempfile.mkstemp(
        prefix=f'.{output_path.name}.',
        suffix='.tmp',
        dir=output_path.parent,
    )
    temporary_path = Path(temporary_name)

    try:
        with os.fdopen(file_descriptor, 'wb') as output:
            output.write(HEADER.pack(
                MAGIC,
                FORMAT_VERSION,
                len(files),
                len(version_bytes),
                0,
                package_id,
            ))
            output.write(version_bytes)

            for entry in files:
                output.write(ENTRY.pack(
                    len(entry['relative_bytes']),
                    entry['flags'],
                    entry['size'],
                    entry['digest'],
                ))
                output.write(entry['relative_bytes'])

                current_size, current_digest = hash_file(entry['path'])
                if current_size != entry['size'] or current_digest != entry['digest']:
                    raise RuntimeError(f'Runtime file changed while packing: {entry["path"]}')

                with entry['path'].open('rb') as source:
                    while chunk := source.read(CHUNK_SIZE):
                        output.write(chunk)

            output.flush()
            os.fsync(output.fileno())

        os.replace(temporary_path, output_path)
    finally:
        if temporary_path.exists():
            temporary_path.unlink()

    total_bytes = sum(entry['size'] for entry in files)
    print(f'Packed {len(files)} files ({total_bytes:,} bytes) from {source_directory}')
    print(f'Runtime version: {version}')
    print(f'Package id: {package_id.hex()}')
    print(f'Output: {output_path}')


def main():
    parser = argparse.ArgumentParser(
        description=f'Pack the fixed ScreenAI {RUNTIME_VERSION} runtime dependency.'
    )
    parser.add_argument('source_directory', type=Path)
    arguments = parser.parse_args()

    write_package(arguments.source_directory)


if __name__ == '__main__':
    main()
