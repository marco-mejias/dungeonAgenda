# Iconos de mochila (32x32) de los 8 objetos de broma, calcados del arte de referencia del usuario.
# Se pintan sobre una rejilla de caracteres (un carácter = un píxel) con pinceles de píxel sueltos y
# líneas en escalera limpia (2:1 o 45°), y al final llevan 1 px de contorno exterior.

import os, sys
sys.path.insert(0, os.path.dirname(__file__))
from pintor import hexa

N = 32
CONTORNO = hexa("25131a")


class Rejilla:
    def __init__(self):
        self.g = [["."] * N for _ in range(N)]

    def p(self, x, y, ch):
        if 0 <= x < N and 0 <= y < N:
            self.g[y][x] = ch

    def fila(self, y, x0, x1, ch):
        for x in range(x0, x1 + 1):
            self.p(x, y, ch)

    def texto(self, x0, y0, filas):
        """Calca un bloque de texto en (x0, y0); '.' no pinta."""
        for dy, f in enumerate(filas):
            for dx, ch in enumerate(f):
                if ch != ".":
                    self.p(x0 + dx, y0 + dy, ch)

    def imagen(self, colores, contorno_extra=None):
        col = dict(colores)
        col["#"] = CONTORNO
        img = [[None if ch == "." else col[ch] for ch in fila] for fila in self.g]
        img = contornear(img, CONTORNO)
        if contorno_extra:
            img = contornear(img, contorno_extra)
        return img


def contornear(img, color):
    nuevo = [fila[:] for fila in img]
    for y in range(N):
        for x in range(N):
            if img[y][x] is not None:
                continue
            for dx, dy in ((1, 0), (-1, 0), (0, 1), (0, -1)):
                xx, yy = x + dx, y + dy
                if 0 <= xx < N and 0 <= yy < N and img[yy][xx] is not None and img[yy][xx][3] > 200:
                    nuevo[y][x] = color
                    break
    return nuevo


ACERO = {"1": hexa("3e3f55"), "2": hexa("63677f"), "3": hexa("9096aa"), "4": hexa("c2cad6"), "5": hexa("eef0e4")}


# 1. Chancleta de la Inquisición: de perfil y en diagonal (la puntera abajo a la izquierda), con tiras
#    de cuero tachonadas y un aura dorada que la rodea con rayos.
def chancleta():
    r = Rejilla()
    # Suela en escalera 2:1: de la puntera (x4, y25) al talón (x27, y14); 4 filas de grueso
    for k in range(12):
        for x in (4 + 2 * k, 5 + 2 * k):
            y = 25 - k
            for i, ch in enumerate("kmld"):
                if (x == 4 and i == 0) or (x == 27 and i == 3):
                    continue  # puntera y talón redondeados
                r.p(x, y + i, ch)
    # Tira del empeine: un arco de 2 px que sale de la suela, sube y vuelve a bajar (con tachuelas)
    arco = [(8, 22), (8, 21), (8, 20), (8, 19), (9, 18), (9, 17), (10, 16), (10, 15), (11, 14), (12, 13),
            (13, 12), (14, 12), (15, 11), (16, 11), (17, 11), (18, 12), (19, 12), (20, 13), (20, 14), (21, 15),
            (21, 16), (21, 17)]
    for x, y in arco:
        r.p(x, y, "M")
    for x, y in arco:                       # grosor: la cara de dentro, más oscura
        if r.g[y + 1][x + 1] == ".":
            r.p(x + 1, y + 1, "m")
    for x, y in [(9, 18), (12, 13), (16, 11), (20, 13)]:
        r.p(x, y, "s")
    # Tira del talón: lazo sobre el talón
    talon = [(23, 15), (23, 14), (23, 13), (23, 12), (24, 11), (25, 10), (26, 10), (27, 11), (27, 12), (27, 13)]
    for x, y in talon:
        r.p(x, y, "M")
    for x, y in talon:
        if r.g[y + 1][x - 1] == "." and x > 24:
            r.p(x - 1, y + 1, "m")
    r.p(25, 10, "s")
    col = {"k": hexa("e0b080"), "m": hexa("a8643c"), "l": hexa("7a4630"), "d": hexa("4a2a1e"),
           "M": hexa("d08a50"), "s": hexa("c2cad6")}
    img = r.imagen(col, contorno_extra=hexa("f0c040"))   # aura: un anillo dorado
    # Rayos del aura: trazos sueltos sin contorno, apuntando hacia fuera
    Y = hexa("f8e070")
    for x0, y0, dx, dy in [(14, 6, 0, -1), (5, 10, -1, -1), (24, 6, 1, -1), (2, 19, -1, 0), (29, 17, 1, 0),
                           (8, 30, -1, 1), (18, 30, 0, 1), (28, 25, 1, 1)]:
        for s in range(3):
            x, y = x0 + dx * s, y0 + dy * s
            if 0 <= x < N and 0 <= y < N and img[y][x] is None:
                img[y][x] = Y
    return img


# 2. Escudo de cartón mojado: caja triste, empapada por abajo, con gotas y charco
def escudo_carton():
    r = Rejilla()
    r.texto(6, 3, [
        "kkkkkkkkkkkkkkkkkkC.",
        "kkkkkkkkkkkkkkkkkkCC",
        "kccccccccccCcccccbCC",
        "kccccccccccCccccbbCC",
        "kccccccccccCcccccbCC",
        "kcc##cccccccc##ccccC",
        "kcccc#ccccccc#cccccC",
        "kcc###ccccccc###cccC",
        "kcc###ccccccc###cccC",
        "kccccccccccccccccbcC",
        "kcccccc######ccccbcC",
        "kccccc#cccccc#cccccC",
        "kcccc#cccccccc#ccccC",
        "kcccccccccccccccccbC",
        "kwcccccccccccccccwbC",
        "kwwccccccccccccwwwwC",
        "kwwwwccccccccwwwwwwC",
        "CwwwwwwwwwwwwwwwwwwC",
    ])
    r.texto(1, 21, [
        "...bbbBBBBBBBBBBBBBBBBBBbbbb...",
        ".bbBBBBBBBBBBBBBBBBBBBBBBBBBBbb",
        "...bbbbbbbbbbbbbbbbbbbbbbbbb...",
    ])
    for x, y in [(3, 9), (3, 10), (28, 12), (28, 13), (28, 17)]:
        r.p(x, y, "b")
    col = {"k": hexa("c8a670"), "c": hexa("9a7a4e"), "C": hexa("6e5434"), "w": hexa("5e4a36"),
           "b": hexa("6ab0e8"), "B": hexa("3a6a98")}
    return r.imagen(col)


# 3. Poción: matraz redondo con corcho, remolino arcoíris y dos cables (rojo con clavija azul,
#    verde con punta roja)
def pocion():
    r = Rejilla()
    r.texto(13, 2, ["nnnn", "nMmn", "nMmn"])                   # corcho
    r.texto(13, 5, ["gGGg", "gGGg"])                            # cuello
    r.texto(7, 7, [
        "....gggggggg....",
        "..ggGhhRRROOGgg.",
        ".gGhhRRRrOOOyyGg",
        "gGhRRrrOOOyyyvvGg",
        "gGRRrrOOOyyyvvvGg",
        "gGRrrOOyyyvvvVVGg",
        "gGrrOOyyyvvvVVVGg",
        "gGrOOyyyvvvVVVRGg",
        "gGOOyyyvvvVVVRRGg",
        ".gGyyyvvvVVVRRGg.",
        ".gGGyvvvVVRRrGGg.",
        "..ggGGGGGGGGGgg..",
        "....ggggggggg....",
    ])
    # cable rojo por la izquierda, acaba en una clavija azul
    for x, y in [(12, 4), (11, 4), (10, 3), (9, 3), (8, 4), (7, 5), (6, 6), (5, 7), (5, 8), (4, 9), (4, 10),
                 (4, 11), (5, 12), (5, 13), (4, 14), (3, 15), (3, 16), (3, 17)]:
        r.p(x, y, "R")
    r.texto(2, 18, ["bb", "bb", ".b"])
    # cable verde por la derecha, acaba en una punta roja
    for x, y in [(17, 4), (18, 4), (19, 3), (20, 3), (21, 4), (22, 5), (23, 6), (24, 7), (25, 8), (25, 9),
                 (26, 10), (26, 11), (26, 12), (25, 13), (25, 14), (26, 15), (27, 16), (27, 17)]:
        r.p(x, y, "y")
    r.texto(27, 18, ["RR", "R."])
    col = {"n": hexa("5a3424"), "M": hexa("c08a5a"), "m": hexa("8a5a3a"), "g": hexa("c9e4ee"), "G": hexa("7a98a8"),
           "h": hexa("f4f8f8"), "R": hexa("d84a4a"), "r": hexa("e88a4a"), "O": hexa("f0c040"),
           "y": hexa("6ac050"), "v": hexa("5a90e0"), "V": hexa("8a5ad0"), "b": hexa("5a9ae8")}
    return r.imagen(col)


# 4. Espada multiusos inútil: navaja roja en diagonal (cruz blanca), hoja arriba a la derecha,
#    tenedor arriba a la izquierda, peine y sacacorchos
def espada_multiusos():
    r = Rejilla()
    # cuerpo rojo a 45°, 5 px de grueso: de (21,13) a (5,28)
    for y in range(14, 29):
        x0 = 32 - y
        for i in range(5):
            r.p(x0 + i - 1, y, "h" if i == 0 else ("R" if i == 4 else "r"))
    r.texto(11, 20, [".w.", "www", ".w."])                       # cruz blanca
    for y in range(11, 15):                                       # virola dorada
        r.fila(y, 32 - y - 1, 32 - y + 2, "G")
    # hoja de acero a 45° hasta la punta (29,3)
    for y in range(3, 12):
        x0 = 33 - y
        r.p(x0 - 1, y, "5")
        r.p(x0, y, "4")
        r.p(x0 + 1, y, "2")
    # tenedor: sale de la virola hacia arriba a la izquierda y acaba en tres púas
    for y in range(6, 13):
        r.p(y + 6, y, "3")
        r.p(y + 7, y, "2")
    r.texto(8, 1, ["4.4.4", "4.4.4", "4.4.4", "34443", ".3333"])
    # peine: sale de la virola hacia la derecha, púas hacia abajo
    r.fila(13, 22, 29, "4")
    r.fila(14, 22, 29, "2")
    for x in range(23, 30, 2):
        r.p(x, 15, "3")
        r.p(x, 16, "3")
    # sacacorchos: espiral corta pegada al cuerpo
    r.texto(21, 17, ["3...", ".3..", "3...", ".3.."])
    col = dict(ACERO, r=hexa("c84040"), R=hexa("8a2a2a"), h=hexa("e86a5a"), w=hexa("f0e8e0"),
               G=hexa("e8c060"))
    return r.imagen(col)


# 5. Casco de cubo de basura: cubo estrecho con estrías, tapa con asa, dos agujeros por ojos,
#    un pez colgando a la izquierda y una cáscara de plátano abajo a la derecha
def casco_basura():
    r = Rejilla()
    r.texto(13, 1, [".44.", "4..3", "4..3"])                    # asa
    r.texto(8, 4, [
        ".5444443333332..",
        "5444444333333322",
        ".22222222222221.",
    ])                                                            # tapa
    for y in range(7, 27):                                        # cuerpo estriado
        estrecho = 1 if y >= 24 else 0
        r.texto(10 + estrecho, y, ["4343333232321"[estrecho:len("4343333232321") - estrecho]])
    r.fila(27, 11, 21, "1")
    r.texto(12, 12, ["##...##", "##...##", "##...##"])          # los ojos
    # pez colgando (cabeza abajo)
    r.texto(2, 13, [
        ".F.F.",
        "..FF.",
        ".ffF.",
        "fffff",
        "ffeff",
        "fffff",
        ".fff.",
        "..f..",
    ])
    # cáscara de plátano
    r.texto(20, 24, [
        ".....YY..",
        "....YyY..",
        "...YyyYY.",
        "YYYyyyyYY",
        "yy.....yY",
    ])
    col = dict(ACERO, f=hexa("6a9ad0"), F=hexa("3a6a9a"), e=hexa("1a1420"), Y=hexa("f0d040"), y=hexa("b8981c"))
    return r.imagen(col)


# 6. El botón de "¡PÁNICO!": seta roja sobre un muelle recto, caja de madera con "!" y dos
#    exclamaciones rojas a los lados
def boton_panico():
    r = Rejilla()
    for x0 in (2, 28):                                           # exclamaciones: barra y punto
        for y in range(3, 9):
            r.fila(y, x0, x0 + 1, "X")
        r.fila(10, x0, x0 + 1, "X")
        r.fila(11, x0, x0 + 1, "X")
    r.texto(9, 2, [
        "...hhrrrrrr...",
        ".rhhrrrrrrrrR.",
        "rhhrrrrrrrrrRR",
        "rrrrrrrrrrrRRR",
        "RrrrrrrrrrRRRR",
        ".RRRRRRRRRRRR.",
    ])
    r.fila(8, 12, 19, "2")                                       # base de la seta
    for y in (9, 11, 13):                                         # muelle recto
        r.fila(y, 12, 19, "4")
    for y in (10, 12):
        r.p(11, y, "2"); r.p(20, y, "2")
        r.fila(y, 12, 19, "1")
    r.texto(6, 14, [
        "MMMMMMMMMMMMMMMMMMMn",
        "MmmmmmmmmmmmmmmmmmnL",
        "MllllllllllllllllnLL",
        "Mm#mmmmmmXXmmmmm#nLL",
        "MmmmmmmmmXXmmmmmmnLL",
        "MmmmmmmmmXXmmmmmmnLL",
        "MllllllllXXllllllnLL",
        "MmmmmmmmmXXmmmmmmnLL",
        "MmmmmmmmmmmmmmmmmnLL",
        "MmmmmmmmmXXmmmmmmnLL",
        "MllllllllXXllllllnLL",
        "Mm#mmmmmmmmmmmmm#nLL",
        "MmmmmmmmmmmmmmmmmnLL",
        "nnnnnnnnnnnnnnnnnnLL",
        ".LLLLLLLLLLLLLLLLLL.",
    ])
    col = dict(ACERO, r=hexa("e04848"), R=hexa("9a2a2a"), h=hexa("f8b0a0"), X=hexa("d83a3a"),
               M=hexa("d08a50"), m=hexa("a8643c"), l=hexa("7a4630"), n=hexa("5a3424"), L=hexa("3a2018"))
    return r.imagen(col)


# 7. Máscara de "No estoy aquí": cartón con hojas pegadas, cara enfadada y cordel a los lados
def mascara():
    r = Rejilla()
    r.texto(7, 3, [
        "kkkkkkkkkkkkkkkkkC",
        "kvgvgGgvgGgvgGgvcC",
        "kgGvggGvgGvgGgGgcC",
        "kvg##GgvgGgv##vgcC",
        "kgGv##ggvgg##GgvcC",
        "kvgGg#v#gG#v#GgvcC",
        "kgGvgG##vg##gvGgcC",
        "kvgGgvgGgvgGgvgGcC",
        "kgGvgGgvgGvgGgvgcC",
        "kvgGgvg#GgGv#gGvcC",
        "kgGvgG#gvGgvg#vgcC",
        "kvgGgv########vGcC",
        "kgGvgGgvgGgvgGgvcC",
        "kvgGvgGgvgGvgGgvcC",
        "kccccccccccccccccC",
        "CCCCCCCCCCCCCCCCCC",
    ])
    for x, y in [(6, 9), (5, 9), (4, 10), (4, 11), (5, 12), (25, 9), (26, 9), (27, 10), (27, 11), (26, 12)]:
        r.p(x, y, "s")
    col = {"k": hexa("c8a670"), "c": hexa("9a7a4e"), "C": hexa("6e5434"), "g": hexa("6a8a3a"),
           "G": hexa("4a6428"), "v": hexa("8aaa50"), "s": hexa("b89a6a")}
    return r.imagen(col)


# 8. Gallina lanza-huevos: gallina marrón de perfil (mira a la izquierda), casco militar verde,
#    prismáticos en los ojos, pico amarillo, barbilla roja y patas naranjas
def gallina():
    r = Rejilla()
    r.texto(0, 1, [
        "........jjhhhhhh................",
        "......jjhhhhhhhhhh..............",
        ".....jhhhhhhhhhhhhH.............",
        "....HHHHHHHHHHHHHHHH............",
        ".........qqpppppP...............",
        ".AAAAAAA.qqppppppP..............",
        "baaaaaaaAqpppppppPP.............",
        "baaaaaaaApppppppppP.........P...",
        ".AAAAAAAApppppppppPP.......PpP..",
        ".....yYqqppppppppPPP......PppP..",
        "....yyYqpppppppppPPP.....PpppP..",
        "......rqppppppppppPPPP..PppppP..",
        "......rrqpppppppppppppPPpppppP..",
        ".......rqqpppppppppppppppppppP..",
        "........qqqpppppPPPPpppppppppP..",
        "........qqqqppppPqqqPPpppppPPP..",
        "........qqqqqpppPqqqqqPPppPPP...",
        ".........qqqqqppPPqqqqqqPPPPP...",
        ".........qqqqqqpppPPPqqqqPPPP...",
        "..........qqqqqqppppPPPPPPPP....",
        "...........qqqqqqpppppppPPP.....",
        "............qqqqqqqpppPPP.......",
        "..............PPPPPPPPPP........",
        "................o....o..........",
        "................o....o..........",
        "...............oo...oo..........",
        "..............o.o..o.o..........",
    ])
    col = {"p": hexa("b87a46"), "P": hexa("7e4e2a"), "q": hexa("dca064"), "h": hexa("5e7a3a"),
           "H": hexa("3e5426"), "j": hexa("86a254"), "a": hexa("5a5a66"), "A": hexa("3a3a44"),
           "b": hexa("6ab0e8"), "y": hexa("f0c040"), "Y": hexa("c89020"), "r": hexa("d83a3a"), "o": hexa("e88a30")}
    return r.imagen(col)


ICONOS = {
    "chancleta_inquisicion": chancleta,
    "escudo_carton": escudo_carton,
    "pocion_malir_sal": pocion,
    "espada_multiusos": espada_multiusos,
    "casco_cubo_basura": casco_basura,
    "boton_panico": boton_panico,
    "mascara_no_estoy": mascara,
    "gallina_lanza_huevos": gallina,
}

# La gallina en el mapa (16x16, mira a la derecha como los héroes; el contorno va dibujado)
GALLINA_MAPA = """
................
................
......#####.....
.....#Hhhhj#....
....#HHHHHHHH#..
.....#Pppq#AAAA#
...#..#PPpqAaab#
..#P#.#PPppqqY..
..#pP#PPpppqqYy#
..#ppPPpppppqr#.
..#PpppPqqqpqr#.
...#PPPqqqqqq#..
....#PPPqqqq#...
.....##o##o#....
......#o.#o#....
................
"""
GALLINA_MAPA_COL = {"p": hexa("b87a46"), "P": hexa("7e4e2a"), "q": hexa("dca064"), "h": hexa("5e7a3a"),
                    "H": hexa("3e5426"), "j": hexa("86a254"), "a": hexa("5a5a66"), "A": hexa("3a3a44"),
                    "b": hexa("6ab0e8"), "y": hexa("f0c040"), "Y": hexa("c89020"), "r": hexa("d83a3a"),
                    "o": hexa("e88a30")}
