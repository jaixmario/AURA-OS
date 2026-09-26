import zlib
import struct
import os

def bmp_to_png(bmp_path, png_path):
    with open(bmp_path, 'rb') as f:
        data = f.read()

    # Read BMP header
    if data[:2] != b'BM':
        raise ValueError("Not a BMP file")

    offset = struct.unpack('<I', data[10:14])[0]
    width = struct.unpack('<i', data[18:22])[0]
    height = struct.unpack('<i', data[22:26])[0]
    bpp = struct.unpack('<H', data[28:30])[0]

    if bpp != 24:
        raise ValueError(f"Only 24 bpp supported, got {bpp}")

    is_top_down = height < 0
    height = abs(height)

    row_bytes = width * 3
    pad_bytes = (4 - (row_bytes % 4)) % 4
    stride = row_bytes + pad_bytes

    # Extract scanlines in top-down RGB order
    raw_rows = []
    for y in range(height):
        if is_top_down:
            row_idx = y
        else:
            row_idx = height - 1 - y

        row_start = offset + row_idx * stride
        row_data = data[row_start:row_start + row_bytes]

        # Convert BGR to RGB and prepend PNG filter byte (0)
        png_row = bytearray(1 + row_bytes)
        png_row[0] = 0 # filter: None
        for x in range(width):
            b = row_data[x * 3 + 0]
            g = row_data[x * 3 + 1]
            r = row_data[x * 3 + 2]
            png_row[1 + x * 3 + 0] = r
            png_row[1 + x * 3 + 1] = g
            png_row[1 + x * 3 + 2] = b
        raw_rows.append(bytes(png_row))

    idat_data = zlib.compress(b''.join(raw_rows), 9)

    def make_chunk(chunk_type, chunk_data):
        crc = zlib.crc32(chunk_type + chunk_data) & 0xffffffff
        return struct.pack('>I', len(chunk_data)) + chunk_type + chunk_data + struct.pack('>I', crc)

    png_bytes = bytearray(b'\x89PNG\r\n\x1a\n')
    # IHDR
    ihdr_data = struct.pack('>IIBBBBB', width, height, 8, 2, 0, 0, 0)
    png_bytes.extend(make_chunk(b'IHDR', ihdr_data))
    # IDAT
    png_bytes.extend(make_chunk(b'IDAT', idat_data))
    # IEND
    png_bytes.extend(make_chunk(b'IEND', b''))

    with open(png_path, 'wb') as f:
        f.write(png_bytes)

    print(f"[+] Successfully converted to PNG: {png_path} ({os.path.getsize(png_path)} bytes)")

if __name__ == '__main__':
    project_root = os.path.dirname(os.path.abspath(__file__))
    bmp = os.path.join(project_root, "screenshot.bmp")
    png = os.path.join(project_root, "screenshot.png")
    bmp_to_png(bmp, png)
