# Placeholders del HUD (Assets/UI/HUD): iconos 16x16 para que el usuario los redibuje.
#   corazon / carne / ojo  -> medidores de líquido del retrato (versión _lleno y _vacio)
#   sol / mochila          -> rueda de acciones
# Uso: python Tools/Sprites/hud_placeholders.py
import os, sys
sys.path.insert(0, os.path.dirname(__file__))
import png

RAIZ = os.path.join(os.path.dirname(__file__), "..", "..", "Assets", "UI", "HUD")

PALETA = {
    ".": (0, 0, 0, 0),
    "k": (21, 22, 27),        # contorno (sombra)
    "r": (184, 67, 58), "R": (232, 110, 96), "d": (120, 38, 34),      # corazón
    "m": (176, 92, 60), "M": (222, 140, 96), "h": (232, 222, 200), "b": (130, 62, 40),  # carne + hueso
    "w": (217, 214, 204), "p": (138, 99, 201), "P": (186, 156, 236), "n": (30, 24, 40),  # ojo
    "y": (240, 210, 122), "o": (215, 178, 74), "O": (160, 120, 40),   # sol
    "c": (140, 96, 60), "C": (180, 130, 82), "e": (90, 60, 40), "g": (215, 178, 74),  # mochila
}

ICONOS = {
    "corazon": [
        "................",
        "................",
        "...kkk....kkk...",
        "..kRRrk..krrrk..",
        ".kRRrrrkkrrrrrk.",
        ".kRrrrrrrrrrrrk.",
        ".krrrrrrrrrrrrk.",
        ".krrrrrrrrrrrdk.",
        "..krrrrrrrrrdk..",
        "...krrrrrrrdk...",
        "....krrrrrdk....",
        ".....krrrdk.....",
        "......krdk......",
        ".......kk.......",
        "................",
        "................",
    ],
    "carne": [
        "................",
        "..........kk....",
        ".........khhk...",
        "........khhhk...",
        "....kkkkkhhk....",
        "...kMMmmmkk.....",
        "..kMMmmmmmmk....",
        ".kMMmmmmmmmmk...",
        ".kMmmmmmmmmmk...",
        ".kmmmmmmmmmbk...",
        ".kmmmmmmmmbbk...",
        "..kmmmmmmbbk....",
        "...kbbbbbbk.....",
        "....kkkkkk......",
        "................",
        "................",
    ],
    "ojo": [
        "................",
        "................",
        "................",
        ".....kkkkkk.....",
        "...kkwwwwwwkk...",
        "..kwwwwppwwwwk..",
        ".kwwwwpPPpwwwwk.",
        "kwwwwpPnnppwwwwk",
        "kwwwwpnnnnpwwwwk",
        ".kwwwwpnnpwwwwk.",
        "..kwwwwppwwwwk..",
        "...kkwwwwwwkk...",
        ".....kkkkkk.....",
        "................",
        "................",
        "................",
    ],
    "sol": [
        ".......y........",
        "...y...y...y....",
        "....y.....y.....",
        "......kkkk......",
        ".....kyyyyk.....",
        "....kyyyoook....",
        "yy..kyyoooOk..yy",
        "....kyoooOOk....",
        "....kooooOOk....",
        ".....kooOOk.....",
        "......kkkk......",
        "....y.....y.....",
        "...y...y...y....",
        ".......y........",
        "................",
        "................",
    ],
    "flecha": [
        "................",
        "................",
        "................",
        "........kk......",
        "........kwk.....",
        "........kwwk....",
        ".kkkkkkkkwwwk...",
        ".kwwwwwwwwwwwk..",
        ".kwwwwwwwwwwwk..",
        ".kkkkkkkkwwwk...",
        "........kwwk....",
        "........kwk.....",
        "........kk......",
        "................",
        "................",
        "................",
    ],
    "mochila": [
        "................",
        ".....kkkkkk.....",
        "....kk....kk....",
        "...kkkkkkkkkk...",
        "..kCCCCCCCCCck..",
        "..kCCCCCCCCCck..",
        "..kkkkkkkkkkkk..",
        "..kCCCkggkCCck..",
        "..kCCCkggkCCck..",
        "..kCCCCkkCCCck..",
        "..kCcccccccCck..",
        "..kCckkkkkkcck..",
        "..kCckeeeekcck..",
        "..kccceeeeccck..",
        "...kkkkkkkkkk...",
        "................",
    ],
}


def filas(dibujo, apagar=False):
    out = []
    for linea in dibujo:
        fila = []
        for ch in linea:
            c = PALETA[ch]
            if apagar and c[3 if len(c) == 4 else 0] != 0 and ch != "k":
                # Vacío: el hueco del icono, oscuro y desaturado
                g = (c[0] + c[1] + c[2]) // 3
                c = (g // 4 + 18, g // 4 + 18, g // 4 + 24)
            fila.append(c)
        out.append(fila)
    return out


if __name__ == "__main__":
    os.makedirs(RAIZ, exist_ok=True)
    for nombre, dibujo in ICONOS.items():
        if nombre in ("corazon", "carne", "ojo"):
            png.escribir(os.path.join(RAIZ, nombre + "_lleno.png"), filas(dibujo))
            png.escribir(os.path.join(RAIZ, nombre + "_vacio.png"), filas(dibujo, True))
        else:
            png.escribir(os.path.join(RAIZ, nombre + ".png"), filas(dibujo))
    print("ok", sorted(os.listdir(RAIZ)))
