# Equipo de los héroes en dos versiones, sobre la plantilla común (plantilla.py):
#   - PUESTO: la pieza a 16x16 en su sitio del cuerpo, con los 4 fotogramas del reposo
#     (Assets/Sprites/Puesto/<id>.png, 64x16). Se dibuja directamente ENCIMA de la persona.
#   - MOCHILA: la pieza suelta y centrada, como se ve en el saco (Assets/Iconos/Objetos/<id>.png, 16x16).
# También las personas sin manos y sus manos (personas.png / manos.png, una fila por héroe), porque
# las manos van encima de lo que empuñan. Cada objeto queda en Assets/Sprites/Objetos/<id>.ase para
# retocarlo (con la persona del caballero de referencia, medio transparente).
#   python Tools/Sprites/objetos.py

import os, sys
sys.path.insert(0, os.path.dirname(__file__))
from pintor import hexa
from ase import Sprite
import plantilla as P
import png
import personas as PER
import heroes_diseno as D
import heroe_caballero as CAB

RAIZ = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", ".."))
V = "................"
capa = D.capa
SOMBRA_CARA = (20, 10, 20, 110)   # sombra translúcida (vale para cualquier piel)

ACERO = {"1": hexa("3e3f55"), "2": hexa("63677f"), "3": hexa("9096aa"), "4": hexa("c2cad6"), "5": hexa("eef0e4")}
MADERA = {"l": hexa("8a5236"), "n": hexa("553024"), "m": hexa("c07a48")}
ORO = {"G": hexa("e8c060"), "o": hexa("a87a2a")}

# ---------- Armas (en la mano de delante, 14,10; la mano se dibuja encima del mango) ----------
ESPADA = CAB.ESPADA
DAGA = capa([V] * 5 + [
    "..............#.",
    ".............#5#",
    ".............#3#",
    "............#mln",
    ".............#n#",
    ".............#n#",
    "..............#.",
])
MAZA = capa([V,
    "............####",
    "............#43#",
    "............#32#",
    "............####",
    ".............#l#",
    ".............#l#",
    ".............#l#",
    ".............#l#",
    ".............#n#",
    ".............#n#",
    "..............#.",
])
ARCO = capa([V, V,
    ".............##.",
    "............#l#.",
    "...........f#l#.",
    "...........f.#l#",
    "...........f.#l#",
    "...........f.#l#",
    "...........f.#l#",
    "...........f.#n#",
    "...........f.#n#",
    "...........f#l#.",
    "............#l#.",
    ".............##.",
])
BASTON = capa([
    ".............##.",
    "............#5v#",
    "............#vV#",
    ".............#l#",
    ".............#l#",
    ".............#n#",
    ".............#l#",
    ".............#l#",
    ".............#n#",
    ".............#l#",
    ".............#l#",
    ".............#n#",
    ".............#l#",
    ".............#n#",
    ".............#n#",
])
# ---------- Mano de detrás (4,12) ----------
ESCUDO = capa([V] * 8 + [
    ".######.........",
    ".#4rrq#.........",
    ".#rGGq#.........",
    ".#rGGq#.........",
    ".#rrqq#.........",
    "..#rq#..........",
    "...##...........",
])
# ---------- Espalda (detrás de la persona) ----------
ANTORCHA = capa([V,
    "..#.............",
    ".#y#............",
    "#yo#............",
    "#oR#............",
    ".#l#............",
    ".#l#............",
    "..#l#...........",
    "..#n#...........",
])
MOCHILA_PEQ = capa([V] * 8 + [
    "#####...........",
    "#mll#...........",
    "#lln#...........",
    "#lnn#...........",
    ".###............",
])
MOCHILA_GRA = capa([V] * 5 + [
    ".####...........",
    "#rrtq#..........",
    "######..........",
    "#mll#...........",
    "#mll#...........",
    "#lln#...........",
    "#lnn#...........",
    "#lnn#...........",
    ".###............",
])
# ---------- Accesorio (encima de las manos): un brillo de oro en el dedo ----------
ANILLO = capa([V] * 10 + ["..............G."])

# Icono del anillo (suelto se ve mejor dibujado aparte)
ANILLO_ICONO = capa([V] * 3 + [
    ".......##.......",
    "......#vV#......",
    ".....##GG##.....",
    "....#G####o#....",
    "....#G#..#o#....",
    "....#G#..#o#....",
    "....#oG##oo#....",
    ".....#oooo#.....",
    "......####......",
])

GEMA = {"v": hexa("6ab0e8"), "V": hexa("3a6aa8")}
TELA = {"r": hexa("a3404a"), "t": hexa("762a38"), "q": hexa("4a1a2c")}
LLAMA = {"y": hexa("f8d860"), "o": hexa("e88a30"), "R": hexa("b84a28")}
CUERDA = {"f": hexa("e8dcc8")}
CAPUCHA_COL = dict(D.LADRON_COL, K=SOMBRA_CARA)

# id -> (ranura de dibujo, texto, colores, icono propio o None)
#   ranura de dibujo: "espalda" (detrás de la persona), "cuerpo", "cabeza", "mano_secundaria",
#   "mano_principal" (antes de las manos) o "accesorio" (después de las manos)
OBJETOS = {
    "espada_corta": ("mano_principal", ESPADA, dict(ACERO, **MADERA), None),
    "daga": ("mano_principal", DAGA, dict(ACERO, **MADERA), None),
    "maza": ("mano_principal", MAZA, dict(ACERO, **MADERA), None),
    "arco_corto": ("mano_principal", ARCO, dict(MADERA, **CUERDA), None),
    "baston_arcano": ("mano_principal", BASTON, dict(ACERO, **MADERA, **GEMA), None),
    "escudo_heraldico": ("mano_secundaria", ESCUDO, dict(ACERO, **TELA, **ORO), None),
    "antorcha": ("espalda", ANTORCHA, dict(MADERA, **LLAMA), None),
    "mochila_pequena": ("espalda", MOCHILA_PEQ, MADERA, None),
    "mochila_grande": ("espalda", MOCHILA_GRA, dict(MADERA, **TELA), None),
    "anillo_suerte": ("accesorio", ANILLO, ORO, (ANILLO_ICONO, dict(ORO, **GEMA))),
    "cota_malla": ("cuerpo", CAB.ARMADURA, CAB.COLORES, None),
    "armadura_placas": ("cuerpo", D.TANQUE_ARMADURA, D.TANQUE_COL, None),
    "tunica": ("cuerpo", D.MAGO_TUNICA, D.MAGO_COL, None),
    "chaqueta_cuero": ("cuerpo", D.LADRON_CHAQUETA, D.LADRON_COL, None),
    "capucha": ("cabeza", D.LADRON_CAPUCHA, CAPUCHA_COL, None),
    "sombrero_mago": ("cabeza", D.MAGO_SOMBRERO, D.MAGO_COL, None),
    "yelmo_rejilla": ("cabeza", D.TANQUE_YELMO, D.TANQUE_COL, None),
}
# Orden de dibujo en el juego (de atrás adelante); "persona" y "manos" son el héroe
ORDEN = ["mochila", "luz", "persona", "cuerpo", "cabeza", "mano_secundaria", "mano_principal", "manos", "accesorio_1", "accesorio_2"]

HEROES = ["ladron", "mago", "caballero", "tanque"]


def fotogramas(texto, col):
    """Las 4 imágenes 16x16 del reposo (el equipo sube y baja con el torso)."""
    return [P.capa("Equipo", texto, col, f) for f in range(P.FOTOGRAMAS)]


def hoja(imgs):
    filas = []
    for y in range(16):
        fila = []
        for img in imgs:
            fila += [c if c is not None else (0, 0, 0, 0) for c in img[y]]
        filas.append(fila)
    return filas


def centrado(img):
    """La pieza recortada y centrada en 16x16 (versión de la mochila)."""
    pts = [(x, y) for y in range(16) for x in range(16) if img[y][x] is not None and img[y][x][3] > 0]
    x0, x1 = min(p[0] for p in pts), max(p[0] for p in pts)
    y0, y1 = min(p[1] for p in pts), max(p[1] for p in pts)
    dx = (16 - (x1 - x0 + 1)) // 2 - x0
    dy = (16 - (y1 - y0 + 1)) // 2 - y0
    return P.mover(img, dx, dy)


def guardar_ase(ruta, imgs, referencia):
    s = Sprite(16, 16)
    s.capa("Referencia")
    s.capa("Puesto")
    for _ in imgs:
        s.fotograma(P.DURACION_MS)
    for f, img in enumerate(imgs):
        s.pintar(0, f, [[(c[0], c[1], c[2], 70) if c else None for c in fila] for fila in referencia[f]])
        s.pintar(1, f, img)
    s.etiqueta("reposo", 0, len(imgs) - 1)
    s.guardar(ruta)


if __name__ == "__main__":
    dir_puesto = os.path.join(RAIZ, "Assets", "Sprites", "Puesto")
    dir_icono = os.path.join(RAIZ, "Assets", "Iconos", "Objetos")
    dir_ase = os.path.join(RAIZ, "Assets", "Sprites", "Objetos")
    dir_heroes = os.path.join(RAIZ, "Assets", "Sprites", "Heroes32")
    for d in (dir_puesto, dir_icono, dir_ase):
        os.makedirs(d, exist_ok=True)

    # Personas sin manos y manos, una fila por héroe
    filas_personas, filas_manos = [], []
    referencia = None
    for n in HEROES:
        capas = PER.capas_persona(n)
        sin_manos = [c for c in capas if c[0] != "Manos"]
        manos = [c for c in capas if c[0] == "Manos"]
        imgs = [PER.componer(sin_manos, f) for f in range(P.FOTOGRAMAS)]
        filas_personas += hoja(imgs)
        filas_manos += hoja([PER.componer(manos, f) for f in range(P.FOTOGRAMAS)])
        if n == "caballero":
            referencia = [PER.componer(capas, f) for f in range(P.FOTOGRAMAS)]
    png.escribir(os.path.join(dir_heroes, "personas.png"), filas_personas)
    png.escribir(os.path.join(dir_heroes, "manos.png"), filas_manos)

    for oid, (ranura, texto, col, icono) in OBJETOS.items():
        imgs = fotogramas(texto, col)
        png.escribir(os.path.join(dir_puesto, oid + ".png"), hoja(imgs))
        if icono:
            ico = P.parsear(icono[0], icono[1], 0)
        else:
            ico = centrado(P.parsear(texto, col, 0))
        png.escribir(os.path.join(dir_icono, oid + ".png"), hoja([ico]))
        guardar_ase(os.path.join(dir_ase, oid + ".ase"), imgs, referencia)
        print(oid, ranura)
