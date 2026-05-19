from math import cos, exp, pi, sin
from pathlib import Path


W = 1024
H = 512
OUT = Path(__file__).resolve().parents[1] / "assets" / "envmaps" / "typewriter_studio_soft.hdr"


def to_rgbe(r: float, g: float, b: float) -> bytes:
    v = max(r, g, b)
    if v < 1e-32:
        return bytes((0, 0, 0, 0))

    exponent = 0
    mantissa = v
    while mantissa > 1.0:
        mantissa *= 0.5
        exponent += 1
    while mantissa <= 0.5:
        mantissa *= 2.0
        exponent -= 1

    scale = mantissa * 256.0 / v
    return bytes((
        max(0, min(255, int(r * scale))),
        max(0, min(255, int(g * scale))),
        max(0, min(255, int(b * scale))),
        exponent + 128,
    ))


def normalize(v):
    length = max((v[0] * v[0] + v[1] * v[1] + v[2] * v[2]) ** 0.5, 1e-8)
    return (v[0] / length, v[1] / length, v[2] / length)


def sph_gauss(direction, center, sharpness: float) -> float:
    dot = max(-1.0, min(1.0, direction[0] * center[0] + direction[1] * center[1] + direction[2] * center[2]))
    return exp(sharpness * (dot - 1.0))


LIGHTS = [
    (normalize((0.0, 0.78, -0.62)), (5.8, 5.6, 5.3), 28.0),
    (normalize((-0.92, 0.18, -0.34)), (2.6, 2.55, 2.45), 60.0),
    (normalize((0.92, 0.18, -0.34)), (2.6, 2.55, 2.45), 60.0),
    (normalize((0.0, 0.25, 0.97)), (1.1, 1.1, 1.15), 42.0),
]


def main():
    OUT.parent.mkdir(parents=True, exist_ok=True)

    with OUT.open("wb") as handle:
        handle.write(b"#?RADIANCE\nFORMAT=32-bit_rle_rgbe\n\n")
        handle.write(f"-Y {H} +X {W}\n".encode("ascii"))

        for y in range(H):
            v = (y + 0.5) / H
            theta = v * pi
            dir_y = cos(theta)
            radius = sin(theta)
            scanline = bytearray()

            for x in range(W):
                u = (x + 0.5) / W
                phi = u * 2.0 * pi
                direction = (radius * cos(phi), dir_y, radius * sin(phi))

                sky = 0.035 + 0.11 * max(direction[1], 0.0) ** 1.6
                floor = 0.010 + 0.020 * max(-direction[1], 0.0) ** 1.4
                horizon = 0.018 * exp(-((abs(direction[1]) / 0.22) ** 2.0))
                r = sky + floor + horizon
                g = sky + floor + horizon
                b = sky * 1.03 + floor + horizon * 1.05

                for center, color, sharpness in LIGHTS:
                    weight = sph_gauss(direction, center, sharpness)
                    r += color[0] * weight
                    g += color[1] * weight
                    b += color[2] * weight

                vignette = 0.96 - 0.10 * (abs(direction[0]) ** 1.8 + abs(direction[2]) ** 1.8)
                vignette = max(0.82, vignette)
                scanline.extend(to_rgbe(r * vignette, g * vignette, b * vignette))

            handle.write(scanline)

    print(OUT)


if __name__ == "__main__":
    main()
