# Crea los .tres de los objetos de broma (y el grano de oro de la gallina) y los mete en el botín
# de los cofres. Los efectos raros los hace el código (Reglas.hpp, EnemyManager, Especiales.gd):
# aquí solo van sus datos básicos.
#   python Tools/Sprites/datos_broma.py

import os, re

RAIZ = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", ".."))

SCRIPTS = {
    "ArmaData": "res://Scripts/GDD/Datos/arma_data.gd",
    "EquipoData": "res://Scripts/GDD/Datos/equipo_data.gd",
    "ObjetoData": "res://Scripts/GDD/Datos/objeto_data.gd",
}

# id, carpeta, clase, campos, modificadores [(stat, plano)]
OBJETOS = [
    ("chancleta_inquisicion", "Armas", "ArmaData", {
        "nombre": "Chancleta de la Inquisición",
        "descripcion": "Nadie se espera la ira de una madre... ni de esta sandalia. +3 de daño a quien te haya atacado (desobediente). Si fallas, rebota: 1 de daño para ti.",
        "dano": 2, "ranura": 0, "tipo": 0, "peso": 0.8, "valor": 25, "coste_pa": 2,
    }, []),
    ("escudo_carton", "Equipo", "EquipoData", {
        "nombre": "Escudo de cartón mojado",
        "descripcion": "Protección de primera... hasta que empieza a lloviznar. +5 de defensa el primer turno de combate; pierde 1 por turno hasta deshacerse y dejarte en -1.",
        "ranura": 1, "tipo": 1, "peso": 1.5, "valor": 4,
    }, []),
    ("pocion_malir_sal", "Consumibles", "ObjetoData", {
        "nombre": "Poción de \\\"¿Qué podría malir sal?\\\"",
        "descripcion": "La etiqueta está borrada, pero brilla en la oscuridad. Tira un d6: 1-2 te vuelves pollo y pierdes el turno; 3-4 vida al máximo y +3 de fuerza; 5-6 explotas y dañas a todos los que estén cerca (tú incluido).",
        "tipo": 5, "peso": 0.5, "valor": 20, "usos": 1,
    }, []),
    ("espada_multiusos", "Armas", "ArmaData", {
        "nombre": "Espada multiusos inútil",
        "descripcion": "Tiene sacacorchos, cortaúñas y tenedor... pero el filo mide 5 centímetros. Al golpear te peinas para distraer al enemigo: su próximo ataque hace 1 menos.",
        "dano": 1, "ranura": 0, "tipo": 0, "peso": 0.6, "valor": 12, "coste_pa": 2,
    }, []),
    ("casco_cubo_basura", "Equipo", "EquipoData", {
        "nombre": "Casco de cubo de basura",
        "descripcion": "Huele a derrota y a pescado de hace tres días. +4 de defensa. Los enemigos prefieren no acercarse... y tus aliados tampoco: no te pueden dar nada.",
        "ranura": 2, "tipo": 1, "peso": 3.0, "valor": 8,
    }, [("defensa", 4)]),
    ("boton_panico", "Consumibles", "ObjetoData", {
        "nombre": "El botón de \\\"¡PÁNICO!\\\"",
        "descripcion": "Para cuando la situación te supera. Un uso: te teletransporta a una sala cualquiera del mapa. Tus compañeros se quedan atrás... y te lo recordarán (-5 de cordura para ellos).",
        "tipo": 5, "peso": 1.0, "valor": 30, "usos": 1,
    }, []),
    ("mascara_no_estoy", "Equipo", "EquipoData", {
        "nombre": "Máscara de \\\"No estoy aquí\\\"",
        "descripcion": "Cartón con hojas pegadas. Los enemigos te ignoran la mitad de las veces y van a por tus compañeros. Pero si te atacan, el daño se duplica: no te lo esperabas.",
        "ranura": 4, "tipo": 2, "peso": 0.3, "valor": 15,
    }, []),
    ("grano_oro", "Consumibles", "ObjetoData", {
        "nombre": "Grano de oro",
        "descripcion": "El pienso favorito de la gallina lanza-huevos. Si lo llevas, cada 5 disparos se lo come en vez de volverse contra ti.",
        "tipo": 8, "peso": 0.1, "valor": 10, "apilable": "true", "max_pila": 10,
    }, []),
]


def tres(oid, carpeta, clase, campos, mods):
    ext = [f'[ext_resource type="Script" path="{SCRIPTS[clase]}" id="1_cls"]',
           f'[ext_resource type="Texture2D" path="res://Assets/Iconos/Objetos/{oid}.png" id="2_ico"]']
    if clase != "ObjetoData":
        ext.append(f'[ext_resource type="Texture2D" path="res://Assets/Sprites/Puesto/{oid}.png" id="3_pue"]')
    if mods:
        ext.append('[ext_resource type="Script" path="res://Scripts/GDD/Datos/modificador_stat.gd" id="4_mod"]')
    subs = "".join(f'[sub_resource type="Resource" id="Mod_{i}"]\nscript = ExtResource("4_mod")\nstat = "{s}"\nplano = {v}\n\n'
                   for i, (s, v) in enumerate(mods))
    lineas = ['script = ExtResource("1_cls")']
    for k, v in campos.items():
        lineas.append(f'{k} = "{v}"' if isinstance(v, str) and k in ("nombre", "descripcion") else f"{k} = {v}")
    lineas.append('icono = ExtResource("2_ico")')
    if clase != "ObjetoData":
        lineas.append('sprite_puesto = ExtResource("3_pue")')
        lineas.append("clases_permitidas = PackedStringArray()")
    if mods:
        lineas.append('modificadores = Array[ExtResource("4_mod")]([%s])' % ", ".join(f'SubResource("Mod_{i}")' for i in range(len(mods))))
    texto = f'[gd_resource type="Resource" script_class="{clase}" format=3]\n\n' + "\n".join(ext) + "\n\n" + subs + "[resource]\n" + "\n".join(lineas) + "\n"
    ruta = os.path.join(RAIZ, "Data", "Objetos", carpeta, oid + ".tres")
    open(ruta, "w", encoding="utf-8").write(texto)
    return f"res://Data/Objetos/{carpeta}/{oid}.tres"


def al_botin(rutas, tabla="cofre", peso=6):
    """Añade los objetos a la tabla de botín (si no están ya)."""
    p = os.path.join(RAIZ, "Data", "Botin", tabla + ".tres")
    t = open(p, encoding="utf-8").read()
    for ruta in rutas:
        if ruta in t:
            continue
        rid = "broma_" + os.path.basename(ruta)[:-5]
        i = t.rfind("[ext_resource")
        j = t.index("\n", i) + 1
        t = t[:j] + f'[ext_resource type="Resource" path="{ruta}" id="{rid}"]\n' + t[j:]
        k = t.index("\n[resource]")
        t = t[:k] + f'\n[sub_resource type="Resource" id="Entrada_{rid}"]\nscript = ExtResource("1_v7n5p")\nobjeto = ExtResource("{rid}")\npeso = {peso}\n' + t[k:]
        t = re.sub(r"(entradas = Array\[[^\]]*\]\(\[)(.*?)(\]\))", lambda m: m.group(1) + m.group(2) + f', SubResource("Entrada_{rid}")' + m.group(3), t, count=1, flags=re.S)
    open(p, "w", encoding="utf-8").write(t)


if __name__ == "__main__":
    rutas = [tres(*o) for o in OBJETOS]
    al_botin([r for r in rutas if "grano_oro" not in r])
    al_botin([r for r in rutas if "grano_oro" in r], peso=10)
    print("\n".join(rutas))
