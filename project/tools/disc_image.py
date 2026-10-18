"""Read two required files from a user's XGD2 image; never write to the image."""
import hashlib
from pathlib import Path
import struct

MAGIC = b'MICROSOFT*XBOX*MEDIA'
XEX_SHA256 = '04a98280c688ae4208ae20e2983dbe2aa094b943d935509035efe1931a7679b8'


class DiscImage:
    def __init__(self, path):
        self.path = Path(path)
        self.stream = self.path.open('rb')
        self.size = self.path.stat().st_size
        try:
            for base in (0, 0xFD90000, 0x2080000):
                if base + 32 * 2048 + 2048 > self.size:
                    continue
                descriptor = self.read(base + 32 * 2048, 2048)
                if descriptor[:20] == MAGIC and descriptor[-20:] == MAGIC:
                    self.base = base
                    self.root = struct.unpack_from('<II', descriptor, 20)
                    break
            else:
                raise ValueError('No supported XGD2 filesystem found. Supply your retail disc ISO.')
        except Exception:
            self.close()
            raise

    def close(self):
        self.stream.close()

    def __enter__(self):
        return self

    def __exit__(self, *args):
        self.close()

    def read(self, offset, size):
        if offset < 0 or size < 0 or offset + size > self.size:
            raise ValueError('Disc entry is outside the ISO')
        self.stream.seek(offset)
        data = self.stream.read(size)
        if len(data) != size:
            raise ValueError('Truncated ISO')
        return data

    def find(self, sector, size, name):
        if not 0 < size <= 64 * 1024 * 1024:
            raise ValueError('Invalid disc directory size')
        data = self.read(self.base + sector * 2048, size)
        offset, visited = 0, set()
        while True:
            if offset in visited or offset + 14 > len(data):
                raise ValueError('Invalid or cyclic disc directory')
            visited.add(offset)
            left, right, entry_sector, entry_size, attributes, length = struct.unpack_from('<HHIIBB', data, offset)
            if not length or offset + 14 + length > len(data):
                raise ValueError('Invalid disc filename')
            entry_name = data[offset + 14:offset + 14 + length].decode('ascii').lower()
            if name.lower() == entry_name:
                return entry_sector, entry_size, attributes
            next_offset = left if name.lower() < entry_name else right
            if not next_offset:
                raise FileNotFoundError(f'{name} is missing from the ISO')
            offset = next_offset * 4

    def file(self, path, max_size):
        sector, size = self.root
        parts = path.replace('\\', '/').split('/')
        for index, name in enumerate(parts):
            sector, size, attributes = self.find(sector, size, name)
            if index < len(parts) - 1 and not attributes & 0x10:
                raise ValueError('Expected a disc directory')
        if attributes & 0x10 or size > max_size:
            raise ValueError('Invalid game file type or size')
        return self.read(self.base + sector * 2048, size)


def read_inputs(iso):
    with DiscImage(iso) as disc:
        xex = disc.file('default.xex', 16 * 1024 * 1024)
        if hashlib.sha256(xex).hexdigest() != XEX_SHA256:
            raise ValueError('Unsupported game executable. Expected the retail USA/Europe release (Title ID 545407DF).')
        flash = disc.file('assets/resource/flash/flash.rpf', 32 * 1024 * 1024)
    from patch_menu import ORIGINAL_ARCHIVE
    if hashlib.sha256(flash).hexdigest() != ORIGINAL_ARCHIVE:
        raise ValueError('Unsupported menu archive in the selected ISO')
    return xex, flash


def validate(iso):
    read_inputs(iso)
    print('Supported retail ISO verified. The disc image is unchanged.', flush=True)


def extract(iso, root):
    root = Path(root)
    xex, flash = read_inputs(iso)
    # Validate both inputs before producing any files. The ISO is opened read-only.
    paths = {root / 'private_work/default.xex': xex,
             root / 'dist/assets/resource/flash/flash.rpf': flash}
    for path, data in paths.items():
        path.parent.mkdir(parents=True, exist_ok=True)
        temp = path.with_suffix(path.suffix + '.tmp')
        temp.write_bytes(data)
        temp.replace(path)
    print('Extracted supported executable and menu archive locally; ISO unchanged.', flush=True)
