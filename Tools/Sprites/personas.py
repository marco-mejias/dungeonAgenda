# Las cuatro personas sin equipo, sobre la base común (plantilla.py): solo cambian el pelo, la piel,
# la cara y la ropa interior. Todo lo demás (armaduras, sombreros, capuchas...) es equipo y se dibuja
# directamente encima.
#   python Tools/Sprites/personas.py <carpeta_salida> <vista.png>

import os, sys
sys.path.insert(0, os.path.dirname(__file__))
from pintor import hexa
import plantilla as P
import png

V = "................"


def capa(filas):
    filas = filas + [V] * (16 - len(filas))
    for f in filas:
        assert len(f) == 16, (f, len(f))
    return "\n".join(filas)


DIENTES = hexa("e8dcc8")
SONRISA = capa([V] * 6 + ["........#ff#....", ".........##....."])
BOTAS = {"b": hexa("8a5236"), "B": hexa("553024")}

# Cada persona: colores (piel k/K, pelo h/H, ropa interior c/C) y, si hace falta, una capa de cara
PERSONAS = {
    "ladron": dict(
        colores=dict(BOTAS, k=hexa("b88a6a"), K=hexa("80584a"), h=hexa("2a2228"), H=hexa("1c161c"),
                     c=hexa("38403a"), C=hexa("272e2a")),
        cara=None),
    "mago": dict(
        colores=dict(BOTAS, k=hexa("b8a89a"), K=hexa("7e6e6a"), h=hexa("cfcac2"), H=hexa("8e8a86"),
                     c=hexa("4a3a5e"), C=hexa("342a46"), f=DIENTES),
        cara=SONRISA),
    "caballero": dict(
        colores=dict(BOTAS, k=hexa("c49a7c"), K=hexa("8e6656"), h=hexa("d8b048"), H=hexa("a07a2c"),
                     c=hexa("4a4058"), C=hexa("332c40"), f=DIENTES),
        cara=SONRISA),
    "tanque": dict(
        # pelirrojo con barba
        colores=dict(BOTAS, k=hexa("c8907a"), K=hexa("8e5e50"), h=hexa("b0582e"), H=hexa("74361e"),
                     c=hexa("5a4636"), C=hexa("3e3026")),
        cara=capa([V] * 6 + ["......hhh##h....", "......hhhhH.....", ".......hHH......"])),
}


def capas_persona(nombre):
    d = PERSONAS[nombre]
    col = d["colores"]
    capas = [("Sombra", P.SOMBRA, {"s": (20, 10, 20, 80)}), ("Piernas", P.PIERNAS, col), ("Cuerpo", P.CUERPO, col)]
    if d["cara"]:
        capas.append(("Cara", d["cara"], col))
    capas.append(("Manos", P.MANOS, col))
    return capas


def componer(capas, f=0):
    img = [[None] * 16 for _ in range(16)]
    for nombre, texto, col in capas:
        px = P.capa(nombre, texto, col, f)
        for y in range(16):
            for x in range(16):
                if px[y][x] is not None:
                    img[y][x] = px[y][x] if img[y][x] is None else png.mezclar(img[y][x], px[y][x])
    return img


def vista(imagenes, ruta, esc=10, hueco=4, fondo=(59, 42, 46, 255)):
    """imagenes: lista de filas, cada una una lista de imágenes 16x16."""
    cols = max(len(f) for f in imagenes)
    W = cols * 16 + (cols + 1) * hueco
    H = len(imagenes) * 16 + (len(imagenes) + 1) * hueco
    lienzo = [[fondo] * (W * esc) for _ in range(H * esc)]
    for fi, fila in enumerate(imagenes):
        for ci, img in enumerate(fila):
            ox, oy = hueco + ci * (16 + hueco), hueco + fi * (16 + hueco)
            for y in range(16):
                for x in range(16):
                    c = img[y][x]
                    if c is None or c[3] == 0:
                        continue
                    for yy in range(esc):
                        linea = lienzo[(oy + y) * esc + yy]
                        for xx in range(esc):
                            linea[(ox + x) * esc + xx] = png.mezclar(linea[(ox + x) * esc + xx], c)
    png.escribir(ruta, lienzo)


if __name__ == "__main__":
    from ase import Sprite
    salida = sys.argv[1] if len(sys.argv) > 1 else "."
    ruta_vista = sys.argv[2] if len(sys.argv) > 2 else os.path.join(salida, "personas.png")
    os.makedirs(salida, exist_ok=True)
    orden = ["ladron", "mago", "caballero", "tanque"]
    for n in orden:
        capas = capas_persona(n)
        s = Sprite(16, 16)
        for nombre, _, _ in capas:
            s.capa(nombre)
        s.fotograma(P.DURACION_MS)
        for i, (nombre, texto, col) in enumerate(capas):
            s.pintar(i, 0, P.capa(nombre, texto, col, 0))
        s.guardar(os.path.join(salida, "persona_" + n + ".ase"))
    vista([[componer(capas_persona(n)) for n in orden]], ruta_vista)
    print("vista", ruta_vista)
