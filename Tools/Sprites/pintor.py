# Pintor de sprites por formas: elipses, rectángulos con bisel y polígonos con sombreado por una
# luz fija (arriba a la izquierda), más un contorno oscuro por pieza. Cada capa es un lienzo RGBA.

import math

LUZ = (-0.55, -0.65, 0.52)
_l = math.sqrt(sum(c * c for c in LUZ))
LUZ = tuple(c / _l for c in LUZ)


def hexa(h, a=255):
    h = h.lstrip("#")
    return (int(h[0:2], 16), int(h[2:4], 16), int(h[4:6], 16), a)


class Lienzo:
    def __init__(self, ancho, alto):
        self.ancho = ancho
        self.alto = alto
        self.px = [[None] * ancho for _ in range(alto)]

    def poner(self, x, y, c):
        if 0 <= x < self.ancho and 0 <= y < self.alto and c is not None:
            self.px[y][x] = c

    def tono(self, rampa, intensidad):
        """rampa: colores de claro a oscuro; intensidad 0..1 (1 = muy iluminado)."""
        i = int((1.0 - max(0.0, min(1.0, intensidad))) * len(rampa))
        return rampa[min(len(rampa) - 1, i)]

    def elipse(self, cx, cy, rx, ry, rampa, plana=False):
        for y in range(int(cy - ry - 1), int(cy + ry + 2)):
            for x in range(int(cx - rx - 1), int(cx + rx + 2)):
                nx = (x + 0.5 - cx) / rx
                ny = (y + 0.5 - cy) / ry
                d = nx * nx + ny * ny
                if d > 1.0:
                    continue
                if plana:
                    self.poner(x, y, rampa[0])
                    continue
                nz = math.sqrt(max(0.0, 1.0 - d))
                luz = nx * LUZ[0] + ny * LUZ[1] + nz * LUZ[2]
                self.poner(x, y, self.tono(rampa, 0.05 + 0.8 * luz))  # pocas zonas muy claras: más volumen

    def rect(self, x0, y0, x1, y1, rampa, bisel=1):
        """Bloque con borde claro arriba-izquierda y oscuro abajo-derecha."""
        for y in range(y0, y1 + 1):
            for x in range(x0, x1 + 1):
                c = rampa[1] if len(rampa) > 2 else rampa[0]
                if y - y0 < bisel or x - x0 < bisel:
                    c = rampa[0]
                if y1 - y < bisel or x1 - x < bisel:
                    c = rampa[-1]
                self.poner(x, y, c)

    def poligono(self, puntos, rampa, sombra_x=True):
        xs = [p[0] for p in puntos]
        ys = [p[1] for p in puntos]
        for y in range(int(min(ys)), int(max(ys)) + 1):
            for x in range(int(min(xs)), int(max(xs)) + 1):
                if _dentro(x + 0.5, y + 0.5, puntos):
                    t = (x - min(xs)) / max(1.0, max(xs) - min(xs)) if sombra_x else (y - min(ys)) / max(1.0, max(ys) - min(ys))
                    self.poner(x, y, self.tono(rampa, 1.0 - t))

    def linea(self, x0, y0, x1, y1, c):
        n = max(abs(x1 - x0), abs(y1 - y0)) + 1
        for i in range(n):
            t = i / max(1, n - 1)
            self.poner(round(x0 + (x1 - x0) * t), round(y0 + (y1 - y0) * t), c)

    def contorno(self, c):
        """Contorno exterior de lo pintado en esta capa."""
        nuevos = []
        for y in range(self.alto):
            for x in range(self.ancho):
                if self.px[y][x] is not None:
                    continue
                for dx, dy in ((1, 0), (-1, 0), (0, 1), (0, -1)):
                    xx, yy = x + dx, y + dy
                    if 0 <= xx < self.ancho and 0 <= yy < self.alto and self.px[yy][xx] is not None and self.px[yy][xx][3] > 200:
                        nuevos.append((x, y))
                        break
        for x, y in nuevos:
            self.px[y][x] = c
        return self

    # ---------- Estilo plano (como los sprites originales): base + sombra y contorno gordo ----------

    def elipse_plana(self, cx, cy, rx, ry, colores, umbral=0.15):
        """colores = (base, sombra): la sombra cae abajo a la derecha (umbral bajo = más sombra)."""
        base, sombra = colores
        for y in range(int(cy - ry - 1), int(cy + ry + 2)):
            for x in range(int(cx - rx - 1), int(cx + rx + 2)):
                nx = (x + 0.5 - cx) / rx
                ny = (y + 0.5 - cy) / ry
                if nx * nx + ny * ny > 1.0:
                    continue
                self.poner(x, y, sombra if nx * 0.92 + ny * 0.38 > umbral else base)

    def caja_plana(self, x0, y0, x1, y1, colores, sombra_ancho=None):
        """Bloque con sombra a la derecha y abajo (por defecto, casi la mitad derecha)."""
        base, sombra = colores
        if sombra_ancho is None:
            sombra_ancho = max(1, (x1 - x0 + 1) * 2 // 5)
        for y in range(y0, y1 + 1):
            for x in range(x0, x1 + 1):
                en_sombra = x > x1 - sombra_ancho or y > y1 - sombra_ancho + 1
                self.poner(x, y, sombra if en_sombra else base)

    def poligono_plano(self, puntos, colores):
        base, sombra = colores
        xs = [p[0] for p in puntos]
        corte = min(xs) + (max(xs) - min(xs)) * 0.55
        ys = [p[1] for p in puntos]
        for y in range(int(min(ys)), int(max(ys)) + 1):
            for x in range(int(min(xs)), int(max(xs)) + 1):
                if _dentro(x + 0.5, y + 0.5, puntos):
                    self.poner(x, y, sombra if x + 0.5 > corte else base)

    def borrar(self, x0, y0, x1, y1):
        for y in range(y0, y1 + 1):
            for x in range(x0, x1 + 1):
                if 0 <= x < self.ancho and 0 <= y < self.alto:
                    self.px[y][x] = None
        return self

    def contorno_gordo(self, c, grosor=2):
        """Contorno de 'grosor' píxeles alrededor de lo pintado (2 a 32x32 = 1 a 16x16)."""
        for _ in range(grosor):
            self.contorno(c)
        return self

    def encima(self, otra):
        """Copia encima lo pintado en otra capa."""
        for y in range(self.alto):
            for x in range(self.ancho):
                if otra.px[y][x] is not None:
                    self.px[y][x] = otra.px[y][x]
        return self

    def mover(self, dx, dy):
        nuevo = [[None] * self.ancho for _ in range(self.alto)]
        for y in range(self.alto):
            for x in range(self.ancho):
                xx, yy = x + dx, y + dy
                if 0 <= xx < self.ancho and 0 <= yy < self.alto:
                    nuevo[yy][xx] = self.px[y][x]
        self.px = nuevo
        return self


def _dentro(x, y, pts):
    dentro = False
    j = len(pts) - 1
    for i in range(len(pts)):
        xi, yi = pts[i]
        xj, yj = pts[j]
        if (yi > y) != (yj > y) and x < (xj - xi) * (y - yi) / (yj - yi + 1e-9) + xi:
            dentro = not dentro
        j = i
    return dentro
