# Diseño base (solo el fotograma 0, sin animación) del ladrón, el mago y el tanque, sobre la base
# común de plantilla.py. Cada uno en su .ase de un fotograma con capas, para retocarlo en LibreSprite,
# y una vista previa con los sprites antiguos arriba y los nuevos abajo.
#   python Tools/Sprites/heroes_diseno.py <carpeta_salida> <vista.png> [--animar]
# Con --animar escribe cada .ase con los 4 fotogramas de reposo y su hoja <heroe>_hoja.png.

import os, sys
sys.path.insert(0, os.path.dirname(__file__))
from pintor import hexa
from ase import Sprite
import plantilla as P
import png

SALIDA = sys.argv[1] if len(sys.argv) > 1 and __name__ == "__main__" else "."
VISTA = sys.argv[2] if len(sys.argv) > 2 else os.path.join(SALIDA, "diseno.png")
RAIZ = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", ".."))

PERSONA = {
    "k": hexa("c49a7c"), "K": hexa("8e6656"), "h": hexa("3e2e2a"), "H": hexa("2e2220"),
    "c": hexa("3f3d4c"), "C": hexa("2c2a38"), "b": hexa("8a5236"), "B": hexa("553024"),
}
ACERO = {"1": hexa("3e3f55"), "2": hexa("63677f"), "3": hexa("9096aa"), "4": hexa("c2cad6"), "5": hexa("eef0e4"),
         "g": hexa("c9f0ff")}
CUERO = {"m": hexa("c07a48"), "l": hexa("8a5236"), "n": hexa("553024")}
SOMBRA = {"s": (20, 10, 20, 80)}
V = "................"


def capa(filas):
    filas = filas + [V] * (16 - len(filas))
    for f in filas:
        assert len(f) == 16, (f, len(f))
    return "\n".join(filas)


# ---------------- LADRÓN: capucha (1 px más grande que la cabeza) con la cara en sombra, pañuelo,
# chaqueta corta de cuero con correa cruzada y guantes ----------------
LADRON_COL = dict(PERSONA, **CUERO,
    y=hexa("5e6b5c"), u=hexa("414c45"), U=hexa("2f3833"), v=hexa("232a27"),     # capucha: brillo/base/sombra/fondo
    w=hexa("6e2b30"), W=hexa("4a1c24"),                                          # pañuelo
    z=hexa("7a5440"), a=hexa("5a3a2c"), A=hexa("3e2820"))                        # chaqueta de cuero
LADRON_CAPUCHA = capa([
    "....########....",
    "...#yyuuuuUU#...",
    "..#yuuuuuuUUU#..",
    "..#yuuuuKKKKU#..",
    "..#uuuuu....U#..",
    "..#uuuuv....U#..",
    "..#uuuvwwwwW#...",
    "...#uuvwwwwW#...",
    "....#uvvvwW#....",
])
LADRON_CHAQUETA = capa([V] * 9 + [
    "...#zanaaaAAA#..",
    "..#zaaanaaAAA#..",
    "..#za#mlnlA###..",
    ".....aaaaA#.....",
])
LADRON_GUANTES = dict(LADRON_COL, k=hexa("5a3a2c"), K=hexa("3e2820"))

# ---------------- MAGO: sombrero gigante con la punta caída hacia atrás y el ala a la altura de la
# nariz (le tapa los ojos), sonrisa, túnica larga
# violeta oscura hasta los pies, colgante de cobre, piel pálida ----------------
MAGO_COL = dict(PERSONA, **CUERO,
    k=hexa("b8a89a"), K=hexa("7e6e6a"),                                          # piel pálida
    y=hexa("5c4a6e"), w=hexa("3e3250"), W=hexa("2c2440"), O=hexa("8a5236"),      # sombrero (y su cinta)
    z=hexa("66548a"), p=hexa("4a3a5e"), P=hexa("342a46"),                        # túnica
    o=hexa("c07a48"), f=hexa("e8dcc8"))                                          # colgante de cobre, dientes
MAGO_SOMBRERO = capa([
    "...######.......",
    ".##ywwwwwW#.....",
    "#y#wwwwwwWW#....",
    "#w#ooooooOOO#...",
    "#ywwwwwwwwWWWWW#",
    ".##############.",
])
# Sonrisa con dientes (f): lo único que se le ve de la cara bajo el ala; tapa la boca recta de la base
MAGO_CARA = capa([V] * 6 + [
    "........#ff#....",
    "........####....",
])
MAGO_TUNICA = capa([V] * 9 + [
    "...#zpppppPPP#..",
    "..#zppppoPPPP#..",
    "..#zp#ppppP###..",
    "....#pppppP#....",
    "...#zpppppPP#...",
    "..#zppppPPPPP#..",
])

# ---------------- TANQUE: el yelmo cerrado que llevaba el caballero, hecho igual que el del sprite viejo
# (rejilla de barrotes, sus mismos grises), y armadura pesada con ribetes dorados ----------------
TANQUE_COL = dict(PERSONA, **CUERO,
    a=hexa("adc1cf"), b=hexa("90919e"),                                          # grises del yelmo antiguo
    **{"4": hexa("adc1cf"), "3": hexa("90919e"), "2": hexa("6a6d7a")},            # acero de la armadura
    G=hexa("e8c060"), o=hexa("a87a2a"))                                          # ribetes dorados
TANQUE_YELMO = capa([
    V,
    ".....######.....",
    "....#abbbbb#....",
    "...#ab#######...",
    "...#bb#a#a#a#...",
    "...#ba#a#a#a#...",
    "...#baaaaaaa#...",
    "...##bbb##b#....",
])
TANQUE_ARMADURA = capa([V] * 8 + [
    "....###323##....",
    "..#G4#43332#G2#.",
    ".#G43#43322#G2#.",
    ".##32#GooG####..",
    ".....#323#......",
    ".....#2G2#......",
])
TANQUE_GUANTES = dict(TANQUE_COL, k=hexa("adc1cf"), K=hexa("6a6d7a"))


def heroe(nombre, col, ropa_torso, ropa_cabeza, manos_col, cara=None):
    capas = [("Sombra", P.SOMBRA, SOMBRA), ("Piernas", P.PIERNAS, col), ("Cuerpo", P.CUERPO, col)]
    if cara:
        capas.append(("Cara", cara, col))
    capas.append(("Ropa_torso", ropa_torso, col))
    if ropa_cabeza:
        capas.append(("Ropa_cabeza", ropa_cabeza, col))
    capas.append(("Manos", P.MANOS, manos_col))
    return nombre, capas


import heroe_caballero as CAB
# Caballero sonriente: comisuras hacia arriba y dientes
CABALLERO_CARA = capa([V] * 6 + [
    "........#ff#....",
    ".........##.....",
])
HEROES = [
    heroe("ladron", LADRON_COL, LADRON_CHAQUETA, LADRON_CAPUCHA, LADRON_GUANTES),
    heroe("mago", MAGO_COL, MAGO_TUNICA, MAGO_SOMBRERO, MAGO_COL, cara=MAGO_CARA),
    heroe("caballero", dict(CAB.COLORES, f=hexa("e8dcc8")), CAB.ARMADURA, None, CAB.GUANTELETES, cara=CABALLERO_CARA),
    heroe("tanque", TANQUE_COL, TANQUE_ARMADURA, TANQUE_YELMO, TANQUE_GUANTES),
]


def componer(capas, f=0):
    img = [[None] * 16 for _ in range(16)]
    for nombre, texto, col in capas:
        px = P.capa(nombre, texto, col, f)
        for y in range(16):
            for x in range(16):
                if px[y][x] is not None:
                    img[y][x] = px[y][x] if img[y][x] is None else png.mezclar(img[y][x], px[y][x])
    return img


if __name__ == "__main__":
    os.makedirs(SALIDA, exist_ok=True)
    ANIMAR = "--animar" in sys.argv
    compuestos = {}
    for nombre, capas in HEROES:
        ruta = os.path.join(SALIDA, nombre + ".ase")
        if ANIMAR:
            # Reposo de 4 fotogramas (plantilla.py): lo de arriba baja 1 px en el 3 y el 4, parpadeo en el 4
            P.exportar(ruta, capas)
            hoja = [[(0, 0, 0, 0)] * (16 * P.FOTOGRAMAS) for _ in range(16)]
            for f in range(P.FOTOGRAMAS):
                img = componer(capas, f)
                for y in range(16):
                    for x in range(16):
                        if img[y][x] is not None:
                            hoja[y][f * 16 + x] = img[y][x]
            png.escribir(os.path.join(SALIDA, nombre + "_hoja.png"), hoja)
            compuestos[nombre] = [componer(capas, f) for f in range(P.FOTOGRAMAS)]
            continue
        s = Sprite(16, 16)
        for n, _, _ in capas:
            s.capa(n)
        s.fotograma(P.DURACION_MS)
        for i, (_, texto, col) in enumerate(capas):
            s.pintar(i, 0, P.parsear(texto, col, 0))
        s.guardar(ruta)
        print("escrito", ruta)
        compuestos[nombre] = [componer(capas)]

    # Hoja conjunta para el juego (heroes.png): una fila por héroe (ladrón, mago, caballero, tanque) y
    # una columna por fotograma de reposo. El juego la usa con hframes=4, vframes=4.
    if ANIMAR:
        conjunta = []
        for n in ["ladron", "mago", "caballero", "tanque"]:
            for y in range(16):
                fila = []
                for f in range(P.FOTOGRAMAS):
                    fila += [c if c is not None else (0, 0, 0, 0) for c in compuestos[n][f][y]]
                conjunta.append(fila)
        png.escribir(os.path.join(SALIDA, "heroes.png"), conjunta)
        print("hoja conjunta", os.path.join(SALIDA, "heroes.png"))

    # Vista previa: arriba los antiguos (ladrón, mago, caballero, tanque), abajo los nuevos en el mismo orden
    _, _, vieja = png.leer(os.path.join(RAIZ, "Assets", "2D Pixel Dungeon Asset Pack", "character and tileset", "Dungeon_Character_2.png"))
    recorte = lambda hoja, fx: [fila[fx * 16:fx * 16 + 16] for fila in hoja[:16]]
    orden = ["ladron", "mago", "caballero", "tanque"]
    if ANIMAR:
        # Una fila por héroe con sus 4 fotogramas
        filas_vista = [compuestos[n] for n in orden]
    else:
        filas_vista = [[recorte(vieja, 0), recorte(vieja, 2), recorte(vieja, 4), recorte(vieja, 6)],
                       [compuestos[n][0] for n in orden]]
    ESC, HUECO, FONDO = 10, 4, (59, 42, 46, 255)
    W = 4 * 16 + 5 * HUECO
    H = len(filas_vista) * 16 + (len(filas_vista) + 1) * HUECO
    lienzo = [[FONDO] * (W * ESC) for _ in range(H * ESC)]
    for fila_i, fila in enumerate(filas_vista):
        for i, img in enumerate(fila):
            ox, oy = HUECO + i * (16 + HUECO), HUECO + fila_i * (16 + HUECO)
            for y in range(16):
                for x in range(16):
                    c = img[y][x]
                    if c is None or c[3] == 0:
                        continue
                    for yy in range(ESC):
                        linea = lienzo[(oy + y) * ESC + yy]
                        for xx in range(ESC):
                            linea[(ox + x) * ESC + xx] = png.mezclar(linea[(ox + x) * ESC + xx], c)
    png.escribir(VISTA, lienzo)
    print("vista", VISTA)
