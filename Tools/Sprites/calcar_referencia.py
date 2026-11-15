# Calca cada objeto del arte de referencia del usuario (Docs/Referencias/objetos_broma.png) a 32x32:
# separa el fondo de la casilla (relleno por inundación desde el borde), recorta el dibujo, lo reduce
# por bloques tomando el color más repetido de cada bloque y quita los píxeles sueltos.
#   python Tools/Sprites/calcar_referencia.py   -> Assets/Iconos/Objetos/<id>.png
import sys, os
from collections import Counter, deque
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import png

RAIZ = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", ".."))
W, H, ref = png.leer(os.path.join(RAIZ, "Docs", "Referencias", "objetos_broma.png"))
SALIDA = os.path.join(RAIZ, "Assets", "Iconos", "Objetos")

# Zonas de dibujo de cada casilla (sin el texto de debajo)
COLS = [(22, 252), (278, 508), (530, 762), (780, 1012)]
FILAS = [(22, 282), (398, 636)]
IDS = ["chancleta_inquisicion", "escudo_carton", "pocion_malir_sal", "espada_multiusos",
       "casco_cubo_basura", "boton_panico", "mascara_no_estoy", "gallina_lanza_huevos"]


def dif(a, b):
    return abs(a[0] - b[0]) + abs(a[1] - b[1]) + abs(a[2] - b[2])


def calcar(x0, y0, x1, y1, n=32, umbral=42):
    w, h = x1 - x0, y1 - y0
    px = lambda x, y: ref[y0 + y][x0 + x]
    borde = [px(x, 0) for x in range(w)] + [px(x, h - 1) for x in range(w)]
    fondo = Counter((c[0] // 8, c[1] // 8, c[2] // 8) for c in borde).most_common(1)[0][0]
    fondo = (fondo[0] * 8 + 4, fondo[1] * 8 + 4, fondo[2] * 8 + 4)
    es_fondo = [[False] * w for _ in range(h)]
    cola = deque([(x, y) for x in range(w) for y in (0, h - 1)] + [(x, y) for y in range(h) for x in (0, w - 1)])
    while cola:
        x, y = cola.popleft()
        if not (0 <= x < w and 0 <= y < h) or es_fondo[y][x]:
            continue
        if dif(px(x, y), fondo) > umbral:
            continue
        es_fondo[y][x] = True
        cola.extend(((x + 1, y), (x - 1, y), (x, y + 1), (x, y - 1)))
    xs = [x for y in range(h) for x in range(w) if not es_fondo[y][x]]
    ys = [y for y in range(h) for x in range(w) if not es_fondo[y][x]]
    bx0, bx1, by0, by1 = min(xs), max(xs), min(ys), max(ys)
    lado = max(bx1 - bx0 + 1, by1 - by0 + 1) / (n - 1)   # deja 1 px de margen
    ox = bx0 - ((n - 1) * lado - (bx1 - bx0 + 1)) / 2
    oy = by0 - ((n - 1) * lado - (by1 - by0 + 1)) / 2
    out = []
    for ty in range(n):
        fila = []
        for tx in range(n):
            ca, total = Counter(), 0
            for sy in range(int(oy + ty * lado), int(oy + (ty + 1) * lado)):
                for sx in range(int(ox + tx * lado), int(ox + (tx + 1) * lado)):
                    total += 1
                    if 0 <= sx < w and 0 <= sy < h and not es_fondo[sy][sx]:
                        c = px(sx, sy)
                        ca[(c[0] // 6 * 6, c[1] // 6 * 6, c[2] // 6 * 6)] += 1
            if total and sum(ca.values()) / total >= 0.45:
                c = ca.most_common(1)[0][0]
                fila.append((c[0], c[1], c[2], 255))
            else:
                fila.append((0, 0, 0, 0))
        out.append(fila)
    return out, lado


def limpiar(img):
    """Quita píxeles sueltos (con 0 o 1 vecinos opacos)."""
    n = len(img)
    out = [f[:] for f in img]
    for y in range(n):
        for x in range(n):
            if img[y][x][3] == 0:
                continue
            v = sum(1 for dx in (-1, 0, 1) for dy in (-1, 0, 1) if (dx or dy) and 0 <= x + dx < n and 0 <= y + dy < n and img[y + dy][x + dx][3])
            if v <= 1:
                out[y][x] = (0, 0, 0, 0)
    return out


resultados = {}
i = 0
for (fy0, fy1) in FILAS:
    for (cx0, cx1) in COLS:
        img, lado = calcar(cx0, fy0, cx1, fy1)
        img = limpiar(img)
        resultados[IDS[i]] = img
        png.escribir(os.path.join(SALIDA, IDS[i] + ".png"), img)
        print(IDS[i], "bloque de %.1f px" % lado)
        i += 1
