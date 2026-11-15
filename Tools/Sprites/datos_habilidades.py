# Genera las habilidades (Data/Habilidades/*.tres) y sus iconos placeholder (Assets/Habilidades/*.png).
# Uso: python Tools/Sprites/datos_habilidades.py   (¡sobrescribe! retoca luego en el editor)
#
# Tipos (HabilidadData): ACTIVA=0 PASIVA=1 · Objetivo: UNO_MISMO=0 ENEMIGO=1 ALIADO=2 CASILLA=3
# Disparador: AL_GOLPEAR=0 AL_CRITICO=1 AL_FALLAR=2 AL_MATAR=3 AL_RECIBIR_GOLPE=4 AL_EMPEZAR_DIA=5 AL_USAR_HABILIDAD=6
# Reaccion: DANO_EXTRA=0 REBOTE=1 CURAR=2 RECUPERAR_USO=3 BAJAR_ENFRIAMIENTO=4 PA_EXTRA=5 STAT_TEMPORAL=6 ORO=7 NO_GASTAR_USO=8 CONTRAGOLPE=9
# Efecto.Tipo: CURAR=0 DANAR=1 CORDURA=2 HAMBRE=3 PA=4 ORO=5 COMIDA=6 MODIFICAR_STAT=7
import os, sys
sys.path.insert(0, os.path.dirname(__file__))
import png

RAIZ = os.path.normpath(os.path.join(os.path.dirname(__file__), "..", ".."))
DATOS = os.path.join(RAIZ, "Data", "Habilidades")
ICONOS = os.path.join(RAIZ, "Assets", "Habilidades")

A, P = 0, 1
YO, ENEMIGO, ALIADO, CASILLA = 0, 1, 2, 3
GOLPEAR, CRITICO, FALLAR, MATAR, RECIBIR, DIA, USAR = range(7)
DANO, REBOTE, CURAR, REC_USO, BAJAR_CD, PA, STAT, ORO, NO_GASTAR, CONTRA, APLICAR, NINGUNA = range(12)
ACCIONES = os.path.join(RAIZ, "Data", "Acciones")

def stat(s, v, dias):
    return {"tipo": 7, "valor": v, "stat": s, "duracion_turnos": dias}

HABILIDADES = [
    # ---------------- Iniciales de clase (cada héroe empieza con la suya) ----------------
    dict(id="tajo_amplio", nombre="Tajo amplio", clase="Caballero", tipo=A, inicial=True,
         descripcion="Un barrido con todo el peso de la armadura: golpea a todos los enemigos pegados a ti.",
         coste_pa=2, usos=2, enfriamiento=1, objetivo=YO, area=1, dano_base=2, stat_dano="fuerza", mult_stat=1.0),
    dict(id="punalada", nombre="Puñalada", clase="Ladron", tipo=A, inicial=True,
         descripcion="A traición y sin avisar: daño enorme a un enemigo pegado, que se queda sangrando. Crítico con 15 o más en el d20.",
         coste_pa=2, usos=2, enfriamiento=1, objetivo=ENEMIGO, alcance_min=1, alcance_max=1, dano_base=4, stat_dano="fuerza", mult_stat=1.5, especial="punalada",
         estado="sangrando", estado_dias=2),
    dict(id="bola_fuego", nombre="Bola de fuego", clase="Mago", tipo=A, inicial=True,
         descripcion="Explota en una casilla y quema a todo lo que esté a su lado (aliados incluidos, ojo). Deja el suelo ardiendo 2 días.",
         coste_pa=3, usos=2, enfriamiento=1, objetivo=CASILLA, alcance_min=2, alcance_max=5, area=1, dano_base=3, stat_dano="magia", mult_stat=1.0,
         estado="ardiendo", estado_dias=2, peligro="fuego", peligro_dias=2),
    dict(id="provocar", nombre="Provocar", clase="Tanque", tipo=A, inicial=True,
         descripcion="Insultas a su madre. Los enemigos a 4 casillas solo te atacarán a ti mañana, y te pones +3 de defensa.",
         coste_pa=1, usos=2, enfriamiento=2, objetivo=YO, area=4, tirada=False, especial="provocar",
         efectos_propios=[stat("defensa", 3, 1)]),

    # ---------------- Caballero ----------------
    dict(id="grito_guerra", nombre="Grito de guerra", clase="Caballero", tipo=A,
         descripcion="Los aliados a 3 casillas (tú incluido) ganan +2 de fuerza durante 2 días.",
         coste_pa=1, usos=1, enfriamiento=3, objetivo=YO, area=3, tirada=False, especial="grito",
         efectos_objetivo=[stat("fuerza", 2, 2)]),
    dict(id="represalia", nombre="Represalia", clase="Caballero", tipo=P,
         descripcion="Cuando te golpean, puedes devolver el golpe (4 de daño por pila).",
         disparador=RECIBIR, reaccion=CONTRA, probabilidad=35, valor=4),
    dict(id="filo_sangriento", nombre="Filo sangriento", clase="Caballero", tipo=P,
         descripcion="Tus críticos hacen 4 de daño extra por pila.",
         disparador=CRITICO, reaccion=DANO, probabilidad=100, valor=4),

    # ---------------- Ladrón ----------------
    dict(id="bomba_humo", nombre="Bomba de humo", clase="Ladron", tipo=A,
         descripcion="Desapareces entre el humo: +4 de defensa 2 días y +2 PA ahora mismo.",
         coste_pa=1, usos=1, enfriamiento=2, objetivo=YO, tirada=False,
         efectos_propios=[stat("defensa", 4, 2), {"tipo": 4, "valor": 3}]),
    dict(id="dedos_largos", nombre="Dedos largos", clase="Ladron", tipo=P,
         descripcion="Al matar, le registras los bolsillos: 5 de oro por pila (50%).",
         disparador=MATAR, reaccion=ORO, probabilidad=50, valor=5),
    dict(id="golpe_bajo", nombre="Golpe bajo", clase="Ladron", tipo=P,
         descripcion="Al golpear, a veces (20% por pila) recuperas 1 PA.",
         disparador=GOLPEAR, reaccion=PA, probabilidad=20, valor=1),

    # ---------------- Mago ----------------
    dict(id="rayo_cadena", nombre="Rayo en cadena", clase="Mago", tipo=A,
         descripcion="Un rayo a un enemigo que salta a otros dos cercanos.",
         coste_pa=2, usos=2, enfriamiento=1, objetivo=ENEMIGO, alcance_min=1, alcance_max=5, saltos=2, dano_base=3, stat_dano="magia", mult_stat=0.8),
    dict(id="sobrecarga", nombre="Sobrecarga", clase="Mago", tipo=P,
         descripcion="Cada habilidad que lanzas te da +1 de magia durante 2 días (por pila). Se acumula.",
         disparador=USAR, reaccion=STAT, probabilidad=100, valor=1, stat="magia", duracion=2),
    dict(id="mente_afilada", nombre="Mente afilada", clase="Mago", tipo=P,
         descripcion="Con un crítico recuperas un uso de una habilidad (50% por pila).",
         disparador=CRITICO, reaccion=REC_USO, probabilidad=50, valor=1),
    dict(id="eco_arcano", nombre="Eco arcano", clase="Mago", tipo=P,
         descripcion="Al lanzar una habilidad, 20% por pila de que no gaste el uso.",
         disparador=USAR, reaccion=NO_GASTAR, probabilidad=20, valor=1),

    # ---------------- Tanque ----------------
    dict(id="muro_escudos", nombre="Muro de escudos", clase="Tanque", tipo=A,
         descripcion="+5 de defensa 2 días; los aliados pegados a ti, +2.",
         coste_pa=1, usos=1, enfriamiento=2, objetivo=YO, area=1, tirada=False, especial="muro",
         efectos_propios=[stat("defensa", 5, 2)], efectos_objetivo=[stat("defensa", 2, 2)]),
    dict(id="piel_piedra", nombre="Piel de piedra", clase="Tanque", tipo=P,
         descripcion="Cada golpe que recibes te da +1 de defensa hasta mañana (por pila).",
         disparador=RECIBIR, reaccion=STAT, probabilidad=100, valor=1, stat="defensa", duracion=1),
    dict(id="terremoto", nombre="Terremoto", clase="Tanque", tipo=A,
         descripcion="Das un pisotón: todos los enemigos a 2 casillas reciben daño y caen al suelo. No falla.",
         coste_pa=3, usos=1, enfriamiento=3, objetivo=YO, area=2, tirada=False, dano_base=2, stat_dano="fuerza", mult_stat=0.7,
         estado="derribado", estado_dias=1),

    # ---------------- Posicionamiento ----------------
    dict(id="carga", nombre="Carga", clase="Caballero", tipo=A,
         descripcion="Corres en línea recta hacia un enemigo (2-4 casillas, sin nada en medio) y lo tiras al suelo. +1 de daño por casilla recorrida.",
         coste_pa=2, usos=2, enfriamiento=1, objetivo=ENEMIGO, alcance_min=2, alcance_max=4, movimiento="carga", dano_base=2, stat_dano="fuerza", mult_stat=1.0,
         estado="derribado", estado_dias=1),
    dict(id="estandarte", nombre="Estandarte", clase="Caballero", tipo=P,
         descripcion="Llevas el estandarte: los aliados a 2 casillas de ti tienen +1 en sus tiradas de ataque.",
         reaccion=NINGUNA, etiquetas=["estandarte"], max_pilas=1),
    dict(id="escurridizo", nombre="Escurridizo", clase="Ladron", tipo=P,
         descripcion="Te escabulles: alejarte de un enemigo nunca provoca ataques de oportunidad.",
         reaccion=NINGUNA, etiquetas=["escurridizo"], max_pilas=1),
    dict(id="salto", nombre="Salto mortal", clase="Ladron", tipo=A,
         descripcion="Saltas a una casilla libre a 2-3 casillas, por encima de lo que haya en medio. No provoca ataques de oportunidad.",
         coste_pa=1, usos=2, enfriamiento=1, objetivo=CASILLA, alcance_min=2, alcance_max=3, requiere_vision=False, tirada=False, movimiento="teletransporte"),
    dict(id="veneno_hoja", nombre="Veneno en la hoja", clase="Ladron", tipo=P,
         descripcion="Al golpear, 30% por pila de envenenar 3 días (2 de daño al día).",
         disparador=GOLPEAR, reaccion=APLICAR, probabilidad=30, estado="envenenado", estado_dias=3),
    dict(id="parpadeo", nombre="Parpadeo", clase="Mago", tipo=A,
         descripcion="Te teletransportas a una casilla libre que veas, a 2-4 casillas. Sin ataques de oportunidad.",
         coste_pa=1, usos=2, enfriamiento=1, objetivo=CASILLA, alcance_min=2, alcance_max=4, tirada=False, movimiento="teletransporte"),
    dict(id="onda_choque", nombre="Onda de choque", clase="Mago", tipo=A,
         descripcion="Una explosión de fuerza empuja 2 casillas a todos los enemigos pegados a ti. Contra una pared: aturdidos.",
         coste_pa=2, usos=2, enfriamiento=2, objetivo=YO, area=1, tirada=False, dano_base=1, stat_dano="magia", mult_stat=0.4, empuje=2),
    dict(id="piromano", nombre="Pirómano", clase="Mago", tipo=P,
         descripcion="Tus golpes hacen 4 de daño extra por pila a los enemigos que están ardiendo.",
         disparador=GOLPEAR, reaccion=DANO, probabilidad=100, valor=4, condicion_estado="ardiendo"),
    dict(id="empujon_escudo", nombre="Empujón de escudo", clase="Tanque", tipo=A,
         descripcion="Golpeas con el escudo a un enemigo pegado y lo empujas 2 casillas. Contra una pared o contra otro, se hacen daño.",
         coste_pa=2, usos=2, enfriamiento=1, objetivo=ENEMIGO, alcance_min=1, alcance_max=1, dano_base=1, stat_dano="fuerza", mult_stat=0.8, empuje=2),
    dict(id="gancho", nombre="Gancho", clase="Tanque", tipo=A,
         descripcion="Lanzas una cadena a un enemigo a 2-4 casillas y lo traes hasta ti.",
         coste_pa=2, usos=2, enfriamiento=2, objetivo=ENEMIGO, alcance_min=2, alcance_max=4, dano_base=1, stat_dano="fuerza", mult_stat=0.5, movimiento="atraer"),
    dict(id="guardaespaldas", nombre="Guardaespaldas", clase="Tanque", tipo=P,
         descripcion="Cuando golpean a un aliado pegado a ti, recibes tú el golpe.",
         reaccion=NINGUNA, etiquetas=["guardaespaldas"], max_pilas=1),
    dict(id="intercambio", nombre="Intercambio", clase="", tipo=A,
         descripcion="Cambias de sitio con un aliado a 3 casillas o menos. Para sacar a alguien de un apuro.",
         coste_pa=1, usos=2, enfriamiento=1, objetivo=ALIADO, alcance_min=1, alcance_max=3, requiere_vision=False, tirada=False, movimiento="intercambio"),
    dict(id="patada_caido", nombre="Patada al caído", clase="", tipo=P,
         descripcion="Tus golpes hacen 3 de daño extra por pila a los enemigos derribados.",
         disparador=GOLPEAR, reaccion=DANO, probabilidad=100, valor=3, condicion_estado="derribado"),
    dict(id="desangrar", nombre="Desangrar", clase="", tipo=P,
         descripcion="Tus críticos dejan sangrando al enemigo 2 días (pierde vida al moverse).",
         disparador=CRITICO, reaccion=APLICAR, probabilidad=100, estado="sangrando", estado_dias=2),

    # ---------------- Globales ----------------
    dict(id="gallina", nombre="Gallina lanza-huevos", clase="", tipo=A,
         descripcion="Invocas a una gallina a tu lado que lanza huevos a los enemigos en su turno (ciega). Quiere grano de oro.",
         coste_pa=0, usos=1, enfriamiento=3, objetivo=YO, tirada=False, especial="gallina", peso_draft=6),
    dict(id="segundo_aliento", nombre="Segundo aliento", clase="", tipo=A,
         descripcion="Respiras hondo y recuperas 25 de vida.",
         coste_pa=1, usos=1, enfriamiento=3, objetivo=YO, tirada=False, efectos_propios=[{"tipo": 0, "valor": 25}]),
    dict(id="sed_sangre", nombre="Sed de sangre", clase="", tipo=P,
         descripcion="Al matar, recuperas 6 de vida por pila.",
         disparador=MATAR, reaccion=CURAR, probabilidad=100, valor=6),
    dict(id="chispa_estatica", nombre="Chispa estática", clase="", tipo=P,
         descripcion="Al golpear, 25% por pila de que salte una chispa a otro enemigo cercano (3 de daño).",
         disparador=GOLPEAR, reaccion=REBOTE, probabilidad=25, valor=3),
    dict(id="zancada", nombre="Zancada", clase="", tipo=P,
         descripcion="Al matar, +1 PA por pila. Encadena muertes.",
         disparador=MATAR, reaccion=PA, probabilidad=100, valor=1),
    dict(id="trebol", nombre="Trébol de cuatro hojas", clase="", tipo=P,
         descripcion="Al empezar el día, 35% por pila de que tus habilidades se recarguen un día antes.",
         disparador=DIA, reaccion=BAJAR_CD, probabilidad=35, valor=1),
    dict(id="adrenalina", nombre="Adrenalina", clase="", tipo=P,
         descripcion="Cuando fallas un ataque, 50% por pila de recuperar 1 PA.",
         disparador=FALLAR, reaccion=PA, probabilidad=50, valor=1),
    dict(id="venganza", nombre="Venganza", clase="", tipo=P,
         descripcion="Cuando te golpean, 50% por pila de ganar +2 de fuerza hasta mañana.",
         disparador=RECIBIR, reaccion=STAT, probabilidad=50, valor=2, stat="fuerza", duracion=1),
]

# Acciones básicas: sin usos (usos=0), sin enfriamiento
BASICAS = [
    dict(id="ayudar", nombre="Ayudar", clase="", tipo=A,
         descripcion="Echas una mano a un aliado pegado a ti: +3 a su próxima tirada de ataque.",
         coste_pa=1, usos=0, enfriamiento=0, objetivo=ALIADO, alcance_min=1, alcance_max=1, requiere_vision=False, tirada=False,
         estado="ayudado", estado_dias=1, estado_potencia=3),
    dict(id="retirada", nombre="Retirada", clase="", tipo=A,
         descripcion="Te mueves con cuidado: hasta mañana, alejarte de un enemigo no provoca ataques de oportunidad.",
         coste_pa=1, usos=0, enfriamiento=0, objetivo=YO, tirada=False, estado_propio="desenganchado", estado_propio_dias=1),
]

COLOR_CLASE = {"": (150, 110, 200), "Caballero": (215, 178, 74), "Ladron": (111, 154, 70), "Mago": (90, 140, 230), "Tanque": (150, 150, 165)}

def icono(h):
    c = COLOR_CLASE[h["clase"]]
    osc = tuple(max(0, v // 3) for v in c)
    k = (21, 22, 27)
    filas = []
    sem = sum(ord(ch) for ch in h["id"])
    for y in range(16):
        fila = []
        for x in range(16):
            dx, dy = x - 7.5, y - 7.5
            borde = max(abs(dx), abs(dy))
            if borde > 7:
                fila.append((0, 0, 0, 0)); continue
            if borde > 6:
                fila.append(k); continue
            px = osc
            if h["tipo"] == A:
                d = abs(dx) + abs(dy)  # rombo
                if d < 5.5: px = c
                if d < 2.5: px = tuple(min(255, v + 70) for v in c)
            else:
                r = (dx * dx + dy * dy) ** 0.5  # anillo
                if 2.5 < r < 5.2: px = c
                if r < 1.6: px = tuple(min(255, v + 70) for v in c)
            # Un par de píxeles "únicos" por habilidad, para distinguirlas un poco
            if (x * 7 + y * 13 + sem) % 23 == 0 and borde < 6: px = tuple(min(255, v + 40) for v in c)
            fila.append(px)
        filas.append(fila)
    return filas

def valor(v):
    if isinstance(v, list): return "PackedStringArray(%s)" % ", ".join('"%s"' % s for s in v)
    if isinstance(v, bool): return "true" if v else "false"
    if isinstance(v, str): return '"%s"' % v.replace('"', '\\"')
    if isinstance(v, float): return repr(v)
    return str(v)

def tres(h):
    ext = ['[ext_resource type="Script" path="res://Scripts/GDD/Datos/habilidad_data.gd" id="1_hab"]',
           '[ext_resource type="Texture2D" path="res://Assets/Habilidades/%s.png" id="2_ico"]' % h["id"]]
    subs = []
    listas = {}
    for campo in ("efectos_objetivo", "efectos_propios"):
        ids = []
        for i, e in enumerate(h.get(campo, [])):
            sid = "Efecto_%s_%d" % (campo, i)
            lineas = ['[sub_resource type="Resource" id="%s"]' % sid, 'script = ExtResource("3_ef")']
            for k2, v2 in e.items():
                lineas.append("%s = %s" % (k2, valor(v2)))
            subs.append("\n".join(lineas))
            ids.append('SubResource("%s")' % sid)
        if ids:
            listas[campo] = ids
    if subs:
        ext.append('[ext_resource type="Script" path="res://Scripts/GDD/Datos/efecto.gd" id="3_ef"]')
    out = ['[gd_resource type="Resource" script_class="HabilidadData" format=3]', ""]
    out += ext + [""]
    for s in subs:
        out += [s, ""]
    out += ["[resource]", 'script = ExtResource("1_hab")', 'icono = ExtResource("2_ico")']
    for k, v in h.items():
        if k in ("efectos_objetivo", "efectos_propios", "inicial"):
            continue
        out.append("%s = %s" % (k, valor(v)))
    for campo, ids in listas.items():
        out.append('%s = Array[ExtResource("3_ef")]([%s])' % (campo, ", ".join(ids)))
    return "\n".join(out) + "\n"

if __name__ == "__main__":
    os.makedirs(DATOS, exist_ok=True)
    os.makedirs(ICONOS, exist_ok=True)
    for h in HABILIDADES:
        png.escribir(os.path.join(ICONOS, h["id"] + ".png"), icono(h))
        open(os.path.join(DATOS, h["id"] + ".tres"), "w", encoding="utf-8").write(tres(h))
    # Acciones básicas (las tiene todo el mundo, en la rueda del HUD)
    os.makedirs(ACCIONES, exist_ok=True)
    for h in BASICAS:
        png.escribir(os.path.join(ICONOS, h["id"] + ".png"), icono(h))
        open(os.path.join(ACCIONES, h["id"] + ".tres"), "w", encoding="utf-8").write(tres(h))
    # Iniciales por clase (las lee progreso.gd)
    iniciales = {h["clase"]: h["id"] for h in HABILIDADES if h.get("inicial")}
    print("ok", len(HABILIDADES), "habilidades; iniciales:", iniciales)
