# Escritor mínimo de archivos .ase (formato de Aseprite, que LibreSprite abre y edita).
# Imagen RGBA, varias capas, varios fotogramas con su duración y etiquetas de animación.
# Referencia del formato: https://github.com/aseprite/aseprite/blob/main/docs/ase-file-specs.md
#
#   from ase import Sprite
#   s = Sprite(32, 32)
#   s.capa("Cuerpo"); s.capa("Espada")          # de abajo arriba
#   s.fotograma(duracion_ms=180)                # devuelve el índice
#   s.pintar(capa=0, fotograma=0, pixeles)      # pixeles: lista de filas de tuplas (r, g, b, a) o None
#   s.etiqueta("reposo", 0, 3)
#   s.guardar("heroe.ase")

import struct, zlib


def _texto(t):
    b = t.encode("utf-8")
    return struct.pack("<H", len(b)) + b


class Sprite:
    def __init__(self, ancho, alto):
        self.ancho = ancho
        self.alto = alto
        self.capas = []        # nombres
        self.duraciones = []   # ms por fotograma
        self.cels = {}         # (capa, fotograma) -> bytes RGBA del lienzo entero
        self.etiquetas = []    # (nombre, desde, hasta)

    def capa(self, nombre):
        self.capas.append(nombre)
        return len(self.capas) - 1

    def fotograma(self, duracion_ms=150):
        self.duraciones.append(duracion_ms)
        return len(self.duraciones) - 1

    def pintar(self, capa, fotograma, pixeles):
        """pixeles: alto filas de ancho tuplas (r, g, b, a); None = transparente."""
        buf = bytearray(self.ancho * self.alto * 4)
        for y, fila in enumerate(pixeles[: self.alto]):
            for x, c in enumerate(fila[: self.ancho]):
                if c is None:
                    continue
                i = (y * self.ancho + x) * 4
                buf[i:i + 4] = bytes(c if len(c) == 4 else (c[0], c[1], c[2], 255))
        self.cels[(capa, fotograma)] = bytes(buf)

    def etiqueta(self, nombre, desde, hasta):
        self.etiquetas.append((nombre, desde, hasta))

    @staticmethod
    def _chunk(tipo, datos):
        return struct.pack("<IH", len(datos) + 6, tipo) + datos

    def _cel(self, capa, datos):
        # Recorta al rectángulo con contenido para no guardar transparencia de más
        a, h = self.ancho, self.alto
        xs, ys = [], []
        for y in range(h):
            for x in range(a):
                if datos[(y * a + x) * 4 + 3]:
                    xs.append(x)
                    ys.append(y)
        if not xs:
            return None
        x0, x1, y0, y1 = min(xs), max(xs), min(ys), max(ys)
        w, hh = x1 - x0 + 1, y1 - y0 + 1
        recorte = bytearray()
        for y in range(y0, y1 + 1):
            recorte += datos[(y * a + x0) * 4:(y * a + x1 + 1) * 4]
        cuerpo = struct.pack("<HhhBHh5x", capa, x0, y0, 255, 2, 0)  # tipo 2 = imagen comprimida
        cuerpo += struct.pack("<HH", w, hh) + zlib.compress(bytes(recorte))
        return self._chunk(0x2005, cuerpo)

    def guardar(self, ruta):
        frames = b""
        for f, dur in enumerate(self.duraciones):
            chunks = []
            if f == 0:
                # Paleta mínima (algunos lectores la esperan) y las capas
                chunks.append(self._chunk(0x2019, struct.pack("<III8x", 1, 0, 0) + struct.pack("<HBBBB", 0, 0, 0, 0, 0)))
                for nombre in self.capas:
                    chunks.append(self._chunk(0x2004, struct.pack("<HHHHHHB3x", 3, 0, 0, 0, 0, 0, 255) + _texto(nombre)))
                if self.etiquetas:
                    datos = struct.pack("<H8x", len(self.etiquetas))
                    for nombre, desde, hasta in self.etiquetas:
                        datos += struct.pack("<HHBH6x3xB", desde, hasta, 0, 0, 0) + _texto(nombre)
                    chunks.append(self._chunk(0x2018, datos))
            for c in range(len(self.capas)):
                datos = self.cels.get((c, f))
                if datos:
                    ch = self._cel(c, datos)
                    if ch:
                        chunks.append(ch)
            cuerpo = b"".join(chunks)
            n = len(chunks)
            cabecera = struct.pack("<IHHH2xI", 16 + len(cuerpo), 0xF1FA, min(n, 0xFFFF), dur, n)
            frames += cabecera + cuerpo
        cab = struct.pack("<IHHHHHIH8xB3xHBBhhHH84x", 128 + len(frames), 0xA5E0, len(self.duraciones),
                          self.ancho, self.alto, 32, 1, 100, 0, 0, 1, 1, 0, 0, 16, 16)
        assert len(cab) == 128, len(cab)
        with open(ruta, "wb") as f:
            f.write(cab + frames)
