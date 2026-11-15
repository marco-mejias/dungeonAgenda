extends Node

# Zonas (autoload): lo que da sentido a cada piso, como una cueva de Elden Ring.
#   - Entrada segura con una HOGUERA: descansar cura a todos, recarga las habilidades y pasa el día.
#   - CAMPAMENTOS: cada uno con su pelotón alrededor de un cofre a la vista (y una lumbre).
#     En el cofre del campamento más profundo está la LLAVE de la sala del jefe.
#   - SALA DEL JEFE al fondo, con la puerta cerrada con llave: un enemigo reforzado con habilidades
#     propias, su cofre de premio y la escalera detrás.
#   - Los días cuentan: cada DIAS_AGITACION días explorando la mazmorra se agita y los
#     campamentos que siguen en pie reciben refuerzos. ¿Ir directo al jefe o limpiar zonas?
# La estructura la genera el C++ (Mazmorra.hpp: AsignarZonas) y la publica en metas del mapa:
#   zonas [{indice, tipo, zona, caja, centro, cofre, dist}], casilla_hoguera, casilla_jefe,
#   cofre_llave, cofre_jefe, puertas_jefe, llave_jefe (bool), semilla_piso

const RUTA_DM := "/root/Main/ContenedorMapa/SubViewport/DungeonManager"
const RUTA_PM := "/root/Main/PartyManager"
const DIAS_AGITACION := 3
const MAX_PELOTON := 6
const JEFE_VIDA := 2.5
const JEFE_FUERZA := 2
const JEFE_DEFENSA := 1
const JEFE_FURIA := 3      # +fuerza al bajar de media vida
const COLOR_LLAVE := Color(1.0, 0.82, 0.35)
const COLOR_AGITA := Color(0.95, 0.45, 0.35)

var semilla_vista := -1
var dia_visto := -1
var dias_explorando := 0   # días que han pasado en este piso fuera de combate
var agitacion := 0         # cuántas veces se ha agitado este piso

func _dm() -> Node:
	return get_node_or_null(RUTA_DM)

func _pm() -> Node:
	return get_node_or_null(RUTA_PM)

# ==========================================
# CONSULTAS
# ==========================================
func zonas(tipo: String = "") -> Array:
	var dm := _dm()
	if dm == null:
		return []
	var out := []
	for z in dm.get_meta("zonas", []):
		if tipo == "" or z["tipo"] == tipo:
			out.append(z)
	return out

func tiene_llave() -> bool:
	var dm := _dm()
	return dm != null and bool(dm.get_meta("llave_jefe", false))

func jefe() -> Node:
	for u in Tactica.unidades():
		if not Tactica.es_heroe(u) and u.has_meta("jefe"):
			return u
	return null

## Los vivos de un campamento (o de la sala del jefe)
func miembros(sala: int) -> Array:
	var out := []
	for u in Tactica.unidades():
		if not Tactica.es_heroe(u) and int(u.get_meta("sala", -1)) == sala:
			out.append(u)
	return out

# ==========================================
# CADA FRAME: piso nuevo, días, el jefe
# ==========================================
func _process(_delta: float) -> void:
	var dm := _dm()
	var pm := _pm()
	if dm == null or pm == null:
		semilla_vista = -1
		dia_visto = -1
		return
	var semilla := int(dm.get_meta("semilla_piso", 0))
	if semilla != semilla_vista:
		semilla_vista = semilla
		_nuevo_piso()
	var dia := int(pm.get_meta("turno_actual", 1))
	if dia_visto < 0:
		dia_visto = dia
	elif dia != dia_visto:
		dia_visto = dia
		_nuevo_dia()
	_vigilar_jefe()

func _nuevo_piso() -> void:
	dias_explorando = 0
	agitacion = 0
	dia_visto = -1
	# El cofre del jefe: botín de dos pisos más abajo, doble
	var dm := _dm()
	var celda: Vector2i = dm.get_meta("cofre_jefe", Vector2i(-1, -1))
	if celda.x >= 0:
		var piso := int(_pm().get_meta("piso_actual", 1))
		var botin: Array = Catalogo.tirar_botin("cofre", piso + 2) + Catalogo.tirar_botin("cofre", piso + 2)
		var pendientes: Dictionary = dm.get_meta("botin_cofres", {})
		pendientes[celda] = botin
		dm.set_meta("botin_cofres", pendientes)

func _nuevo_dia() -> void:
	var j := jefe()
	if j and j.get_meta("in_combat", false):
		_pisoton(j)
	if Modo.con_turnos():
		return # los asaltos del combate (y del sigilo) no agitan la mazmorra
	dias_explorando += 1
	if dias_explorando % DIAS_AGITACION == 0:
		agitar()

# ==========================================
# LA MAZMORRA SE AGITA
# ==========================================
## Cada campamento que sigue en pie (y dormido) recibe un refuerzo. Devuelve cuántos llegaron.
func agitar() -> int:
	var dm := _dm()
	if dm == null:
		return 0
	agitacion += 1
	var llegados := 0
	for z in zonas("campamento"):
		var suyos := miembros(int(z["indice"]))
		if suyos.is_empty() or suyos.size() >= MAX_PELOTON:
			continue # limpio (no vuelve) o ya lleno
		if suyos.any(func(e): return e.get_meta("in_combat", false)):
			continue
		var c := _casilla_refuerzo(z)
		if c.x < 0:
			continue
		var nuevo = dm.call("CrearEnemigoEn", c.x, c.y, int(z["indice"]))
		if nuevo:
			llegados += 1
	_avisar("La mazmorra se agita..." if llegados == 0 else "La mazmorra se agita: llegan refuerzos a %d campamento%s" % [llegados, "" if llegados == 1 else "s"], COLOR_AGITA)
	return llegados

func _casilla_refuerzo(z: Dictionary) -> Vector2i:
	var caja: Rect2i = z["caja"]
	var centro: Vector2i = z["cofre"] if z["cofre"].x >= 0 else z["centro"]
	var heroes := []
	for u in Tactica.unidades():
		if Tactica.es_heroe(u):
			heroes.append(Tactica.pos(u))
	var mejor := Vector2i(-1, -1)
	var mejor_d := 999
	for x in range(caja.position.x, caja.end.x):
		for y in range(caja.position.y, caja.end.y):
			var c := Vector2i(x, y)
			var d := maxi(absi(x - centro.x), absi(y - centro.y))
			if d < 1 or d > 3 or Tactica.solido(c) or Tactica.unidad_en(c) != null:
				continue
			if heroes.any(func(h): return Tactica.dist(h, c) <= 4):
				continue # que no aparezca en la cara de nadie
			var puntos := d * 10 + randi() % 10
			if puntos < mejor_d:
				mejor_d = puntos
				mejor = c
	return mejor

# ==========================================
# HOGUERA
# ==========================================
## Descansar en la hoguera: todos curados, habilidades recargadas y pasa el día (la mazmorra cuenta)
func descansar(h: Node) -> bool:
	if Modo.con_turnos():
		_texto(h, "No con enemigos cerca", Color(1.0, 0.5, 0.45))
		return false
	for heroe in Progreso.heroes():
		if heroe.get_meta("esta_muerto", false):
			continue
		heroe.set_meta("hp_actual", int(heroe.get_meta("hp_max", 1)))
		heroe.set_meta("sanidad_actual", int(heroe.get_meta("sanidad_max", heroe.get_meta("sanidad_actual", 0))))
	Progreso.restaurar_todos()
	Modo.descansar()
	_avisar("Descansáis junto a la hoguera", Color(1.0, 0.7, 0.35))
	var audio := get_node_or_null("/root/AudioManager")
	if audio:
		audio.play_sfx("success")
	return true

# ==========================================
# LLAVE Y PUERTA DEL JEFE
# ==========================================
## Lo llama el EventManager al abrir un cofre
func al_abrir_cofre(x: int, y: int, h: Node) -> void:
	var dm := _dm()
	if dm == null or tiene_llave() or Vector2i(x, y) != dm.get_meta("cofre_llave", Vector2i(-1, -1)):
		return
	dm.set_meta("llave_jefe", true)
	_avisar("¡La llave de la sala del jefe!", COLOR_LLAVE)
	_texto(h, "¡Llave!", COLOR_LLAVE)

## ¿Se puede abrir esta puerta? Solo las del jefe piden la llave
func puede_abrir(x: int, y: int, h: Node) -> bool:
	var dm := _dm()
	if dm == null or not Vector2i(x, y) in dm.get_meta("puertas_jefe", []):
		return true
	if tiene_llave():
		_avisar("La puerta del jefe cede...", COLOR_LLAVE)
		return true
	_texto(h, "Cerrada con llave", Color(1.0, 0.5, 0.45))
	var hud := get_tree().get_first_node_in_group("hud")
	if hud:
		hud.registrar("La puerta del jefe está cerrada: la llave está en un campamento", COLOR_LLAVE)
	return false

# ==========================================
# EL JEFE
# ==========================================
## Lo llama el C++ al crear el enemigo de la casilla del jefe
func hacer_jefe(e: Node) -> void:
	e.set_meta("jefe", true)
	e.set_meta("nombre_display", "%s, el Jefe" % str(e.get_meta("nombre_display", "Enemigo")))
	var vida := int(round(int(e.get_meta("hp_max", 10)) * JEFE_VIDA))
	e.set_meta("hp_max", vida)
	e.set_meta("hp_actual", vida)
	e.set_meta("fuerza", int(e.get_meta("fuerza", 4)) + JEFE_FUERZA)
	e.set_meta("defensa", int(e.get_meta("defensa", 1)) + JEFE_DEFENSA)
	e.set_meta("xp", Progreso.xp_de_enemigo(e) * 3)
	e.set_meta("huir_con_vida", 0)
	var cuerpo := e.get_node_or_null("Cuerpo") as Node3D
	if cuerpo:
		cuerpo.scale *= 1.4

# Sus dos habilidades: PISOTÓN (cada día de combate empuja y derriba a quien tenga pegado) y
# FURIA (al bajar de media vida pega más fuerte)
func _pisoton(j: Node) -> void:
	var golpeados := 0
	for h in Tactica.hostiles_adyacentes(j):
		Tactica.empujar(h, Tactica.dir_hacia(Tactica.pos(j), Tactica.pos(h)), 1, j)
		if Tactica.vivo(h):
			Estados.aplicar(h, "derribado", 1, 1, j)
		golpeados += 1
	if golpeados > 0:
		_texto(j, "¡Pisotón!", Color(1.0, 0.6, 0.3))
		var feel := get_node_or_null("/root/Feel")
		if feel:
			feel.sacudir(0.35)

func _vigilar_jefe() -> void:
	var j := jefe()
	if j == null or j.has_meta("furia"):
		return
	if int(j.get_meta("hp_actual", 1)) * 2 <= int(j.get_meta("hp_max", 1)):
		j.set_meta("furia", true)
		j.set_meta("fuerza", int(j.get_meta("fuerza", 4)) + JEFE_FURIA)
		_texto(j, "¡Furia!", Color(1.0, 0.3, 0.25))
		_avisar("%s se enfurece" % str(j.get_meta("nombre_display", "El jefe")), COLOR_AGITA)

## Lo llama Progreso.enemigo_muerto: el gran premio. Todos suben de nivel (habilidad nueva segura)
func jefe_muerto(e: Node, _asesino: Node) -> void:
	_avisar("¡%s ha caído!" % str(e.get_meta("nombre_display", "El jefe")), COLOR_LLAVE)
	for h in Progreso.heroes():
		if h.get_meta("esta_muerto", false) or h.get_meta("ha_escapado", false):
			continue
		var falta := Progreso.xp_para(int(h.get_meta("nivel", 1))) - int(h.get_meta("xp", 0))
		Progreso.ganar_xp(h, maxi(1, falta))

# ==========================================
# AVISOS
# ==========================================
func _avisar(texto: String, color: Color) -> void:
	var banner := get_node_or_null("/root/Main/CanvasLayer/Banner")
	if banner:
		banner.anunciar(texto)
	var hud := get_tree().get_first_node_in_group("hud")
	if hud:
		hud.registrar(texto, color)
	print("[Zonas] ", texto)

func _texto(n: Node, t: String, color: Color) -> void:
	var f := Tactica.ficha(n) if n else null
	var esp := get_node_or_null("/root/Especiales")
	if f and esp:
		esp.texto(f.position + Vector3(0, 0.5, 0), t, color, 26)
