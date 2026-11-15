# Lector y escritor mínimos de PNG (RGBA de 8 bits, sin entrelazado), para hacer vistas previas
# sin depender de Pillow ni de Godot.

import struct, zlib


def leer(ruta):
    """Devuelve (ancho, alto, filas) con filas de tuplas (r, g, b, a)."""
    datos = open(ruta, "rb").read()
    assert datos[:8] == b"\x89PNG\r\n\x1a\n", ruta
    i, idat, paleta, trns = 8, b"", None, None
    while i < len(datos):
        n, tipo = struct.unpack(">I4s", datos[i:i + 8])
        cuerpo = datos[i + 8:i + 8 + n]
        if tipo == b"IHDR":
            ancho, alto, prof, color, _, _, entre = struct.unpack(">IIBBBBB", cuerpo)
            assert prof == 8 and entre == 0, (ruta, prof, entre)
        elif tipo == b"PLTE":
            paleta = [tuple(cuerpo[j:j + 3]) for j in range(0, len(cuerpo), 3)]
        elif tipo == b"tRNS":
            trns = cuerpo
        elif tipo == b"IDAT":
            idat += cuerpo
        i += 12 + n
    canales = {6: 4, 2: 3, 3: 1, 0: 1, 4: 2}[color]
    crudo = zlib.decompress(idat)
    paso = ancho * canales
    filas, previa, p = [], bytearray(paso), 0
    for _ in range(alto):
        filtro = crudo[p]; linea = bytearray(crudo[p + 1:p + 1 + paso]); p += 1 + paso
        for x in range(paso):
            a = linea[x - canales] if x >= canales else 0
            b = previa[x]
            c = previa[x - canales] if x >= canales else 0
            if filtro == 1: linea[x] = (linea[x] + a) & 255
            elif filtro == 2: linea[x] = (linea[x] + b) & 255
            elif filtro == 3: linea[x] = (linea[x] + (a + b) // 2) & 255
            elif filtro == 4:
                pp = a + b - c; pa, pb, pc = abs(pp - a), abs(pp - b), abs(pp - c)
                linea[x] = (linea[x] + (a if pa <= pb and pa <= pc else b if pb <= pc else c)) & 255
        fila = []
        for x in range(ancho):
            px = linea[x * canales:(x + 1) * canales]
            if color == 6: fila.append(tuple(px))
            elif color == 2: fila.append((px[0], px[1], px[2], 255))
            elif color == 3:
                r, g, b_ = paleta[px[0]]
                fila.append((r, g, b_, trns[px[0]] if trns and px[0] < len(trns) else 255))
            elif color == 0: fila.append((px[0], px[0], px[0], 255))
            else: fila.append((px[0], px[0], px[0], px[1]))
        filas.append(fila)
        previa = linea
    return ancho, alto, filas


def escribir(ruta, filas):
    alto, ancho = len(filas), len(filas[0])
    crudo = bytearray()
    for fila in filas:
        crudo.append(0)
        for c in fila:
            crudo += bytes(c if len(c) == 4 else (c[0], c[1], c[2], 255))
    def chunk(t, d):
        return struct.pack(">I", len(d)) + t + d + struct.pack(">I", zlib.crc32(t + d) & 0xFFFFFFFF)
    png = b"\x89PNG\r\n\x1a\n" + chunk(b"IHDR", struct.pack(">IIBBBBB", ancho, alto, 8, 6, 0, 0, 0))
    png += chunk(b"IDAT", zlib.compress(bytes(crudo), 9)) + chunk(b"IEND", b"")
    open(ruta, "wb").write(png)


def mezclar(fondo, c):
    """c encima de fondo (ambos RGBA)."""
    a = c[3] / 255.0
    return tuple(int(fondo[i] * (1 - a) + c[i] * a) for i in range(3)) + (255,)
