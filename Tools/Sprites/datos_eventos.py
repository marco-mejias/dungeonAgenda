# Eventos: crea sus .tres (Data/Events) y los placeholders de su arte (Assets/Eventos/<evento>/).
# Los placeholders tienen nombres fijos: para poner un dibujo de verdad basta con sobrescribir el PNG
# con el mismo nombre y tamaño (ver Assets/Eventos/LEEME.md). Si un PNG ya existe y NO es un
# placeholder (lo has dibujado tú), este script no lo toca.
#   python Tools/Sprites/datos_eventos.py

import os, sys, hashlib
sys.path.insert(0, os.path.dirname(__file__))
import png

RAIZ = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", ".."))
ARTE = os.path.join(RAIZ, "Assets", "Eventos")
DATOS = os.path.join(RAIZ, "Data", "Events")

# Efecto.Tipo
CURAR, DANAR, CORDURA, HAMBRE, PA, ORO, COMIDA, MODIFICAR_STAT, DAR_OBJETO = range(9)


def ef(tipo, valor, stat=None, turnos=None, objeto=None):
    return {"tipo": tipo, "valor": valor, "stat": stat, "turnos": turnos, "objeto": objeto}


def op(boton, stat, mult, exito, fracaso, dano_fracaso=0, ef_exito=(), ef_fracaso=(), ef_critico=(), ef_pifia=()):
    return dict(boton=boton, stat=stat, mult=mult, exito=exito, fracaso=fracaso, dano=dano_fracaso,
                ef_exito=list(ef_exito), ef_fracaso=list(ef_fracaso), ef_critico=list(ef_critico), ef_pifia=list(ef_pifia))


# archivo .tres, id de la carpeta de arte, datos
EVENTOS = [
    ("CultoDeErmitaños", "culto_ermitanos", dict(
        titulo="Culto de Ermitaños",
        descripcion="Te encuentras unos pequeños ermitaños con sombreros puntiagudos: Demuestranos tu poder! Estan pirados pero...creo que seran peores si no les haces caso...",
        gatillo=1, radio=3,
        opciones=[
            op("Canaliza tu magia!", "magia", 1.4,
               "Sacas una chispa de tus manos y quedan anonadados con tu gran poder! Creo que ayudan que sean pequeñitos...",
               "Intentas canalizar tu poder pero creo que te da algo de vergüenza. CABRON! Empiezan a pegarte todos en unisono, por suerte, no es que sean muy fuertes tampoco...",
               2, ef_exito=[ef(CORDURA, 10)], ef_critico=[ef(DAR_OBJETO, 1, objeto="res://Data/Objetos/Consumibles/tonico_cordura.tres")], ef_pifia=[ef(CORDURA, -10)]),
            op("Pegales una patada", "fuerza", 1.2,
               "Empiezas a pegarle poco a poco patadas a cada uno de ellos, lograndoles mandar a la mierda. Te vas corriendo por si acaso...",
               "Al pegarle una patada al primero, los demas se empiezan a abalanzar encima tuyo, cayendo rápidamente mientras lloras del dolor. Quien diria que algo tan pequeño podia ser tan doloroso...",
               6, ef_exito=[ef(ORO, 15)], ef_critico=[ef(ORO, 25)], ef_pifia=[ef(DANAR, 4)]),
        ])),
    ("GoblinFuerte", "goblin_fortachon", dict(
        titulo="Goblin Fortachon",
        descripcion="En una esquina ves un stand de 'El Goblin Fortachon TM'. Te mira con una mirada pícara y te llama: No te dejaré pasar por mi barrio a menos que demuestres que eres un verdadero fortachón...",
        gatillo=0,
        opciones=[
            op("Hagamos el pulso!", "fuerza", 1.4,
               "3,2,1....PAM! Al que parecía ser un fortachón resulto ser un....Es igual, le ganaste, ahora de vuelta al ruedo!",
               "3,2,1....PAM! Al que parecía ser un fortachón resulto ser un....realmente fuerte....Creo que te has roto algo....",
               5, ef_exito=[ef(ORO, 25)], ef_critico=[ef(MODIFICAR_STAT, 2, stat="fuerza", turnos=3)], ef_pifia=[ef(DANAR, 5)]),
            op("Salir corriendo", "velocidad", 1.2,
               "Mas vale fuerza que maña....O es al revés? Mmmm...Lo importante es que por suerte parecia que el goblin se saltaba el dia de piernas, bien hecho!",
               "A DONDE VAS? Esuchas mientras intentas salir corriendo. Miras para atrás y ves al Goblin volando apunto de hacerte un Súplex. Oh oh.",
               10, ef_critico=[ef(PA, 2)]),
        ])),
    ("Roca", "roca_rodante", dict(
        titulo="Roca Rodante",
        descripcion="Desde arriba del techo, una roca gigante desciende! Yo te recomendaría salir corriendo....a no ser que quieras convertirte en una nueva baldosa de la mazmorra!",
        gatillo=2, stat_objetivo="sanidad_actual", valor_critico=50,
        opciones=[
            op("CORRE HA TODA OSTIA", "velocidad", 1.3, "Echas a correr y...milagrosamente escapas!", "Te tropiezas con una piedra y...auch", 3,
               ef_critico=[ef(PA, 1)]),
            op("Parala con ese pechamen", "fuerza", 0.7, "Sorprendentemente le metes una ostia y la paras en seco, genial...", "Enserio creías que.....joder tio...", 3,
               ef_exito=[ef(CORDURA, 10)], ef_critico=[ef(MODIFICAR_STAT, 2, stat="defensa", turnos=3)], ef_pifia=[ef(DANAR, 6)]),
            op("Quedarse quieto", "ninguna", 0.2,
               "La roca te va a golpear y...PUM! Papel maché con mejunje Art Attack! Casualmente colocado en la mazmorra más peligrosa del mundo!",
               "Eh...deberías apreciar más tu vida", 3, ef_exito=[ef(CORDURA, 5)]),
        ])),
    ("FuenteMisteriosa", "fuente_misteriosa", dict(
        titulo="Fuente Misteriosa",
        descripcion="En mitad de la sala brota una fuente de agua que brilla con un tono verdoso. Huele a menta... y un poco a calcetín. Alguien ha tallado en la piedra: 'BEBE SI TE ATREVES'.",
        gatillo=0,
        opciones=[
            op("Beber un trago", "suerte", 1.0,
               "Sabe a menta y a gloria. Notas cómo se te cierran las heridas.", "Sabe a calcetín. Mucho. Te pasas un rato muy malo.", 6,
               ef_exito=[ef(CURAR, 25)], ef_critico=[ef(CURAR, 25), ef(CORDURA, 10)], ef_fracaso=[ef(CORDURA, -5)]),
            op("Lanzar una moneda y pedir un deseo", "ninguna", 1.2,
               "La fuente escupe la moneda... y otras treinta detrás. ¡Deseo concedido!", "La moneda se hunde. Y tu dignidad con ella.", 0,
               ef_exito=[ef(ORO, 30)], ef_fracaso=[ef(ORO, -5)]),
            op("Mirar tu reflejo", "magia", 1.2,
               "Tu reflejo te guiña un ojo y te sientes mucho más tranquilo.", "Tu reflejo se ríe de ti. Y tú no te estabas riendo.", 0,
               ef_exito=[ef(CORDURA, 15)], ef_fracaso=[ef(CORDURA, -10)]),
        ])),
    ("MercaderSospechoso", "mercader_sospechoso", dict(
        titulo="Mercader Sospechoso",
        descripcion="Un tipo encapuchado abre su gabardina llena de frascos: 'Psst... ¿pociones? ¿Granos de oro? Todo de primerísima calidad, recién caído de un carro'.",
        gatillo=1, radio=3,
        opciones=[
            op("Regatear", "suerte", 1.0,
               "Le sacas una poción a precio de risa. Se va murmurando algo sobre su familia.", "Acabas pagando el doble y sin saber muy bien por qué.", 0,
               ef_exito=[ef(DAR_OBJETO, 1, objeto="res://Data/Objetos/Consumibles/pocion_vida.tres")], ef_fracaso=[ef(ORO, -15)],
               ef_critico=[ef(DAR_OBJETO, 1, objeto="res://Data/Objetos/Consumibles/pocion_malir_sal.tres")]),
            op("Robarle la bolsa", "velocidad", 1.0,
               "Ni se entera. La bolsa pesa bastante más de lo que esperabas.", "Te pilla con la mano dentro y te da con un frasco en la cabeza.", 8,
               ef_exito=[ef(ORO, 40)], ef_pifia=[ef(ORO, -20)]),
            op("Comprarle un 'grano de oro'", "ninguna", 2.0,
               "Diez monedas por un grano que brilla de verdad. La gallina estará contenta.", "Diez monedas por un garbanzo pintado. Genial.", 0,
               ef_exito=[ef(ORO, -10), ef(DAR_OBJETO, 1, objeto="res://Data/Objetos/Consumibles/grano_oro.tres")], ef_fracaso=[ef(ORO, -10)]),
        ])),
    ("CofreMimico", "cofre_mimico", dict(
        titulo="Cofre Sospechoso",
        descripcion="Un cofre precioso, solo en mitad del pasillo. Juraría que acaba de respirar. Y que tiene dientes.",
        gatillo=0,
        opciones=[
            op("Abrirlo con cuidado", "suerte", 1.0,
               "Era un cofre normal y corriente, lleno de monedas. Qué paranoia.", "¡Era un mímico! Te muerde la mano y no la suelta en un buen rato.", 8,
               ef_exito=[ef(ORO, 35)], ef_critico=[ef(ORO, 35)]),
            op("Darle una patada primero", "fuerza", 1.2,
               "El cofre suelta un gemido y escupe unas monedas para que lo dejes en paz.", "Te haces daño en el pie. Y el cofre se ríe. Los cofres no deberían reírse.", 5,
               ef_exito=[ef(ORO, 20)]),
            op("Hacerle cosquillas", "ninguna", 0.8,
               "El mímico se parte de risa y te regala una máscara rarísima que tenía dentro.", "Al mímico no le hacen gracia las cosquillas. Nada de gracia.", 3,
               ef_exito=[ef(DAR_OBJETO, 1, objeto="res://Data/Objetos/Equipo/mascara_no_estoy.tres")]),
        ])),
]

# ---------------- Placeholders ----------------
GLIFOS = {
    "?": [".###.", "#...#", "....#", "...#.", "..#..", ".....", "..#.."],
    "V": ["....#", "....#", "...#.", "#..#.", ".##..", ".#...", "....."],
    "X": ["#...#", ".#.#.", "..#..", ".#.#.", "#...#", ".....", "....."],
    "1": [".#.", "##.", ".#.", ".#.", "###"],
    "2": ["##.", "..#", ".#.", "#..", "###"],
    "3": ["##.", "..#", ".#.", "..#", "##."],
}
MARCA = (37, 19, 26, 255)  # el contorno de los sprites


def lienzo(n, fondo):
    return [[fondo] * n for _ in range(n)]


def glifo(img, g, x0, y0, esc, color):
    for y, fila in enumerate(GLIFOS[g]):
        for x, ch in enumerate(fila):
            if ch == "#":
                for yy in range(esc):
                    for xx in range(esc):
                        img[y0 + y * esc + yy][x0 + x * esc + xx] = color


def placeholder(tipo, numero=None):
    """96x96: escena (?), exito (V verde) o fracaso (X roja); el número de la opción arriba."""
    fondo, borde, simbolo, g = {
        "escena": ((42, 40, 48, 255), (122, 112, 104, 255), (154, 144, 136, 255), "?"),
        "exito": ((31, 58, 36, 255), (90, 160, 96, 255), (140, 210, 130, 255), "V"),
        "fracaso": ((58, 31, 34, 255), (192, 80, 80, 255), (230, 120, 110, 255), "X"),
    }[tipo]
    img = lienzo(96, fondo)
    for i in range(96):  # borde discontinuo
        if (i // 4) % 2 == 0:
            for a, b in ((i, 1), (i, 94), (1, i), (94, i)):
                img[b][a] = borde
    glifo(img, g, 33, 27, 6, simbolo)
    if numero:
        glifo(img, str(numero), 6, 6, 3, borde)
    return img


def placeholder_mapa():
    """16x16: un poste de piedra con una interrogación (lo que se ve en el mapa)."""
    img = [[(0, 0, 0, 0)] * 16 for _ in range(16)]
    for y in range(3, 15):
        for x in range(4, 12):
            img[y][x] = (110, 104, 116, 255) if x < 9 else (78, 74, 88, 255)
    for y in range(2, 16):
        for x in range(3, 13):
            if img[y][x][3] == 0 and any(0 <= y + dy < 16 and img[y + dy][x + dx][3] and img[y + dy][x + dx] != MARCA
                                         for dx, dy in ((1, 0), (-1, 0), (0, 1), (0, -1))):
                img[y][x] = MARCA
    for y, fila in enumerate(["##.", "..#", ".#.", "...", ".#."]):
        for x, ch in enumerate(fila):
            if ch == "#":
                img[5 + y][7 + x - 1] = (240, 200, 90, 255)
    return img


def firma(img):
    return hashlib.md5(repr(img).encode()).hexdigest()


FIRMAS_PH = set()


def escribir_ph(ruta, img):
    """Escribe el placeholder solo si no hay ya un dibujo de verdad en esa ruta."""
    FIRMAS_PH.add(firma(img))
    if os.path.exists(ruta):
        try:
            _, _, actual = png.leer(ruta)
            if firma([[tuple(c) for c in f] for f in actual]) not in FIRMAS_PH:
                return False  # lo has dibujado tú: no se toca
        except Exception:
            return False
    png.escribir(ruta, img)
    return True


# ---------------- .tres ----------------

def esc(s):
    return s.replace("\\", "\\\\").replace('"', '\\"')


def tres(nombre, carpeta, d):
    ext = ['[ext_resource type="Script" path="res://Scripts/GDD/OpcionEvento.gd" id="s_op"]',
           '[ext_resource type="Script" path="res://Scripts/GDD/EventoData.gd" id="s_ev"]',
           '[ext_resource type="Script" path="res://Scripts/GDD/Datos/efecto.gd" id="s_ef"]']
    texturas = {}

    def tex(rel):
        if rel not in texturas:
            texturas[rel] = f"t{len(texturas)}"
            ext.append(f'[ext_resource type="Texture2D" path="res://Assets/Eventos/{carpeta}/{rel}" id="{texturas[rel]}"]')
        return f'ExtResource("{texturas[rel]}")'

    objetos = {}

    def obj(ruta):
        if ruta not in objetos:
            objetos[ruta] = f"o{len(objetos)}"
            ext.append(f'[ext_resource type="Resource" path="{ruta}" id="{objetos[ruta]}"]')
        return f'ExtResource("{objetos[ruta]}")'

    subs = []
    n_ef = [0]

    def efecto(e):
        n_ef[0] += 1
        sid = f"Ef_{n_ef[0]}"
        lineas = [f'[sub_resource type="Resource" id="{sid}"]', 'script = ExtResource("s_ef")', f"tipo = {e['tipo']}", f"valor = {e['valor']}"]
        if e["stat"]:
            lineas.append(f'stat = "{e["stat"]}"')
        if e["turnos"]:
            lineas.append(f"duracion_turnos = {e['turnos']}")
        if e["objeto"]:
            lineas.append(f"objeto = {obj(e['objeto'])}")
        subs.append("\n".join(lineas))
        return f'SubResource("{sid}")'

    def lista(es):
        return 'Array[ExtResource("s_ef")]([%s])' % ", ".join(efecto(e) for e in es)

    ops = []
    for i, o in enumerate(d["opciones"], 1):
        sid = f"Op_{i}"
        lineas = [f'[sub_resource type="Resource" id="{sid}"]', 'script = ExtResource("s_op")',
                  f'texto_boton = "{esc(o["boton"])}"', f'stat_usada = "{o["stat"]}"', f"multiplicador_dificultad = {o['mult']}",
                  f"imagen_exito = {tex(f'opcion{i}_exito.png')}", f"imagen_fracaso = {tex(f'opcion{i}_fracaso.png')}",
                  f'texto_exito = "{esc(o["exito"])}"', f'texto_fracaso = "{esc(o["fracaso"])}"']
        if o["dano"]:
            lineas += ['funcion_fracaso = "RecibirDano"', f"parametro_fracaso = {o['dano']}"]
        for campo, clave in (("efectos_exito", "ef_exito"), ("efectos_fracaso", "ef_fracaso"), ("efectos_critico", "ef_critico"), ("efectos_pifia", "ef_pifia")):
            if o[clave]:
                lineas.append(f"{campo} = {lista(o[clave])}")
        subs.append("\n".join(lineas))
        ops.append(f'SubResource("{sid}")')
    recurso = ['script = ExtResource("s_ev")', f'titulo = "{esc(d["titulo"])}"', f'descripcion = "{esc(d["descripcion"])}"',
               f"ilustracion = {tex('escena.png')}", f"sprite_mapa = {tex('mapa.png')}",
               f"imagen_exito = {tex('exito.png')}", f"imagen_fracaso = {tex('fracaso.png')}",
               'opciones = Array[ExtResource("s_op")]([%s])' % ", ".join(ops), f"gatillo = {d['gatillo']}"]
    if d["gatillo"] == 1:
        recurso.append(f"radio_activacion = {d.get('radio', 3)}")
    if d["gatillo"] == 2:
        recurso += [f'stat_objetivo = "{d["stat_objetivo"]}"', f"valor_critico = {d['valor_critico']}"]
    texto = '[gd_resource type="Resource" script_class="EventoData" format=3]\n\n' + "\n".join(ext) + "\n\n" + "\n\n".join(subs) + "\n\n[resource]\n" + "\n".join(recurso) + "\n"
    open(os.path.join(DATOS, nombre + ".tres"), "w", encoding="utf-8").write(texto)


LEEME = """# Arte de los eventos

Cada evento tiene su carpeta (`Assets/Eventos/<evento>/`) con estos PNG. Ahora mismo son
**placeholders**: sobrescribe el archivo con tu dibujo **con el mismo nombre y tamaño** y el juego lo usa
sin tocar nada más. `python Tools/Sprites/datos_eventos.py` no pisa los que hayas dibujado tú.

| Archivo | Tamaño | Qué es |
|---|---|---|
| `mapa.png` | 16x16 (o una tira de 16 de alto: 64x16 = 4 fotogramas, se anima) | Cómo se ve el evento en el mapa (manuales y de proximidad; las trampas no se ven) |
| `escena.png` | 96x96 | La ilustración al abrir el evento (se ve a 2x, píxel nítido) |
| `opcionN_exito.png` | 96x96 | Lo que se ve al GANAR con la opción N (1, 2, 3...) |
| `opcionN_fracaso.png` | 96x96 | Lo que se ve al PERDER con la opción N |
| `exito.png` / `fracaso.png` | 96x96 | Las de reserva del evento (si una opción no tiene la suya) |

Estilo: el de los héroes (contorno #25131a de 1 px, luz arriba a la izquierda, paleta apagada).
"""

if __name__ == "__main__":
    os.makedirs(ARTE, exist_ok=True)
    ph = os.path.join(ARTE, "_placeholder")
    os.makedirs(ph, exist_ok=True)
    for t in ("escena", "exito", "fracaso"):
        png.escribir(os.path.join(ph, t + ".png"), placeholder(t))
    png.escribir(os.path.join(ph, "mapa.png"), placeholder_mapa())
    open(os.path.join(ARTE, "LEEME.md"), "w", encoding="utf-8").write(LEEME)
    for nombre, carpeta, d in EVENTOS:
        c = os.path.join(ARTE, carpeta)
        os.makedirs(c, exist_ok=True)
        escritos = 0
        escritos += escribir_ph(os.path.join(c, "escena.png"), placeholder("escena"))
        escritos += escribir_ph(os.path.join(c, "exito.png"), placeholder("exito"))
        escritos += escribir_ph(os.path.join(c, "fracaso.png"), placeholder("fracaso"))
        escritos += escribir_ph(os.path.join(c, "mapa.png"), placeholder_mapa())
        for i in range(1, len(d["opciones"]) + 1):
            escritos += escribir_ph(os.path.join(c, f"opcion{i}_exito.png"), placeholder("exito", i))
            escritos += escribir_ph(os.path.join(c, f"opcion{i}_fracaso.png"), placeholder("fracaso", i))
        tres(nombre, carpeta, d)
        print(nombre, "->", carpeta, f"({escritos} placeholders)")
