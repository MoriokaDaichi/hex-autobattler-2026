import bpy, sys, struct

argv = sys.argv[sys.argv.index("--") + 1:]
png_path = argv[0]
dds_path = argv[1]

img = bpy.data.images.load(png_path)
w, h = img.size
print(f"{png_path}: {w}x{h} channels={img.channels}")

pixels = list(img.pixels)  # flat RGBA floats, row 0 = bottom in Blender

def to_u8(f):
    v = int(round(f * 255.0))
    if v < 0:
        v = 0
    if v > 255:
        v = 255
    return v

# Build RGBA8 buffer for DDS.
# NOTE: do NOT flip rows here. tkmExporter_batch.ms writes UV.v straight from
# 3ds Max's gettvert without any V-flip, and 3ds Max's V axis already matches
# Blender's (V=0 at the bottom). Flipping here made row 0 of the PNG (bottom
# in Blender/Max UV space) become row 0 of the DDS (top, per the DirectX/DDS
# convention where V=0 is the top row) -- an extra, undesired flip that made
# every sampled texel land on the wrong UV island. This was the real cause
# behind the "shattered glass" look on Goblin and all other Meshy units;
# the source textures/UVs were never actually corrupted.
row_bytes = w * 4
buf = bytearray(row_bytes * h)
for y in range(h):
    src_y = y
    src_off = src_y * w * 4
    dst_off = y * row_bytes
    for x in range(w):
        so = src_off + x * 4
        do = dst_off + x * 4
        buf[do + 0] = to_u8(pixels[so + 0])  # R
        buf[do + 1] = to_u8(pixels[so + 1])  # G
        buf[do + 2] = to_u8(pixels[so + 2])  # B
        buf[do + 3] = to_u8(pixels[so + 3]) if img.channels == 4 else 255  # A

DDSD_CAPS = 0x1
DDSD_HEIGHT = 0x2
DDSD_WIDTH = 0x4
DDSD_PITCH = 0x8
DDSD_PIXELFORMAT = 0x1000
DDPF_FOURCC = 0x4
DDSCAPS_TEXTURE = 0x1000
DXGI_FORMAT_R8G8B8A8_UNORM = 28
D3D10_RESOURCE_DIMENSION_TEXTURE2D = 3

with open(dds_path, "wb") as f:
    def u32(v):
        return struct.pack("<I", v)

    hdr = b""
    hdr += u32(124)  # dwSize
    hdr += u32(DDSD_CAPS | DDSD_HEIGHT | DDSD_WIDTH | DDSD_PIXELFORMAT | DDSD_PITCH)  # dwFlags
    hdr += u32(h)  # dwHeight
    hdr += u32(w)  # dwWidth
    hdr += u32(row_bytes)  # dwPitchOrLinearSize
    hdr += u32(0)  # dwDepth
    hdr += u32(0)  # dwMipMapCount
    hdr += b"\x00" * (4 * 11)  # dwReserved1[11]
    # DDS_PIXELFORMAT (32 bytes)
    hdr += u32(32)  # dwSize
    hdr += u32(DDPF_FOURCC)  # dwFlags
    hdr += b"DX10"  # dwFourCC
    hdr += u32(0)  # dwRGBBitCount
    hdr += u32(0)  # dwRBitMask
    hdr += u32(0)  # dwGBitMask
    hdr += u32(0)  # dwBBitMask
    hdr += u32(0)  # dwABitMask
    hdr += u32(DDSCAPS_TEXTURE)  # dwCaps
    hdr += u32(0)  # dwCaps2
    hdr += u32(0)  # dwCaps3
    hdr += u32(0)  # dwCaps4
    hdr += u32(0)  # dwReserved2
    assert len(hdr) == 124, len(hdr)

    dx10hdr = b""
    dx10hdr += u32(DXGI_FORMAT_R8G8B8A8_UNORM)  # dxgiFormat
    dx10hdr += u32(D3D10_RESOURCE_DIMENSION_TEXTURE2D)  # resourceDimension
    dx10hdr += u32(0)  # miscFlag
    dx10hdr += u32(1)  # arraySize
    dx10hdr += u32(0)  # miscFlags2
    assert len(dx10hdr) == 20, len(dx10hdr)

    f.write(b"DDS ")
    f.write(hdr)
    f.write(dx10hdr)
    f.write(bytes(buf))

print(f"WROTE {dds_path} ({w}x{h})")
