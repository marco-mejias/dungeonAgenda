extends Node

# Progreso (autoload): experience, levels and abilities of the heroes.
#
#   XP        Killing gives XP to whoever lands the last hit (the rest of the party, 25%). On
#             reaching xp_para(nivel) they level up: stats grow by class, abilities recharge and
#             the draft opens (pantalla_nivel.gd): pick 1 of 3 abilities, from their class or global.
#   Slots     3 abilities (the Mage, 4). A repeated passive STACKS (no new slot): combos.
#   Actives   uses + cooldown in days + PA cost. They recharge at the campfire, with the Elixir
#             of wit and when levelling up. Cast from the HUD bar (keys Z X C V).
#   Passives  react on their own: on hit, on critical, on miss, on kill, on taking a hit, at
#             the start of the day or on casting an ability (disparar()).
#
# Per hero (metas): "nivel", "xp", "habilidades" = [{id, usos, enfriamiento, pilas}]. C++ calls:
# disparar(evento, heroe, objetivo, puede_matar) and enemigo_muerto(enemigo, heroe).

const CARPETA := "res://Data/Habilidades"
const CARPETA_ACCIONES := "res://Data/Acciones"
const RUTA_DM := "/root/Main/ContenedorMapa/SubViewport/DungeonManager"
const RUTA_PM := "/root/Main/PartyManager"
const INICIALES := {"Caballero": "tajo_amplio", "Ladron": "punalada", "Mago": "bola_fuego", "Tanque": "provocar"}
# What each class gains per level (over its base stats)
const CRECIMIENTO := {
	"Caballero": {"hp_max": 8, "fuerza": 2, "defensa": 1},
	"Ladron": {"hp_max": 5, "fuerza": 1, "suerte": 2},
	"Mago": {"hp_max": 4, "magia": 3, "suerte": 1},
	"Tanque": {"hp_max": 12, "defensa": 2, "fuerza": 1},
}
const CRECIMIENTO_COMUN := {"hp_max": 6, "fuerza": 1}
const PARTE_GRUPO := 0.25 # XP for the ones who don't land the last hit

signal cambio # Something about abilities/XP changed (HUD)

var habilidades := {}      # id -> HabilidadData
var acciones := {}         # Basic actions everyone has (Data/Acciones): ayudar, retirada...
var pendientes: Array = [] # Level ups waiting for their draft: {heroe, nivel, crecimiento}
var apuntando := {}        # {heroe, indice, celdas} while picking a target
var dia_visto := -1
var _profundidad := 0      # So combos don't call themselves forever

func _ready() -> void:
	process_mode = Node.PROCESS_MODE_ALWAYS
	for r in BaseDatos.cargar_carpeta(CARPETA):
		if r is HabilidadData:
			habilidades[r.obtener_id()] = r
	for r in BaseDatos.cargar_carpeta(CARPETA_ACCIONES):
		if r is HabilidadData:
			acciones[r.obtener_id()] = r
	print("[Progreso] %d habilidades, %d acciones básicas" % [habilidades.size(), acciones.size()])

func _dm() -> Node:
	return get_node_or_null(RUTA_DM)

func _pm() -> Node:
	return get_node_or_null(RUTA_PM)

# Every hero node in the PartyManager
func heroes() -> Array:
	var out := []
	var pm := _pm()
	if pm:
		for h in pm.get_children():
			if h.has_meta("grid_pos") and h.has_meta("hp_actual"):
				out.append(h)
	return out

# Alive and still on the floor
func _vivo(h: Node) -> bool:
	return h != null and is_instance_valid(h) and not h.get_meta("esta_muerto", false) and not h.get_meta("ha_escapado", false)

# The 3D token of a hero or an enemy
func _ficha(n: Node) -> Node3D:
	if n is Node3D:
		return n
	var dm := _dm()
	return dm.get_node_or_null(NodePath(str(n.name))) as Node3D if dm and n else null

# Floating text over a character
func _texto(n: Node, t: String, color: Color, tam: int = 30) -> void:
	var f := _ficha(n)
	var esp := get_node_or_null("/root/Especiales")
	if f and esp:
		esp.texto(f.position + Vector3(0, 0.35, 0), t, color, tam)

func datos(id: String) -> HabilidadData:
	return habilidades.get(id)

# The Mage gets 4 ability slots, everyone else 3
func max_ranuras(h: Node) -> int:
	return 4 if str(h.get_meta("clase", "")) == "Mago" else 3

# The hero's ability list: [{id, usos, enfriamiento, pilas}]
func lista(h: Node) -> Array:
	return h.get_meta("habilidades", [])

## Do any of their abilities have this tag? (rules: escurridizo, guardaespaldas...)
func tiene_etiqueta(n: Node, etiqueta: String) -> bool:
	if n == null or not is_instance_valid(n) or not n.has_meta("habilidades"):
		return false
	for e in lista(n):
		var d: HabilidadData = habilidades.get(e["id"])
		if d and etiqueta in d.etiquetas:
			return true
	return false

# ==========================================
# EVERY FRAME: new heroes and passing days
# ==========================================
func _process(_delta: float) -> void:
	var pm := _pm()
	if pm == null:
		dia_visto = -1
		return
	for h in heroes():
		if not h.has_meta("habilidades"):
			_iniciar(h)
	var dia := int(pm.get_meta("turno_actual", 1))
	if dia_visto < 0:
		dia_visto = dia
	elif dia != dia_visto:
		dia_visto = dia
		_nuevo_dia()

# A new hero: level 1, no XP and their class starting ability
func _iniciar(h: Node) -> void:
	h.set_meta("nivel", int(h.get_meta("nivel", 1)))
	h.set_meta("xp", 0)
	var hab: Array = []
	var inicial: String = INICIALES.get(str(h.get_meta("clase", "")), "")
	if inicial != "" and habilidades.has(inicial):
		hab.append(_entrada(habilidades[inicial]))
	h.set_meta("habilidades", hab)
	cambio.emit()

# A fresh ability record (full uses, no cooldown, 1 stack)
func _entrada(d: HabilidadData) -> Dictionary:
	return {"id": d.obtener_id(), "usos": d.usos, "enfriamiento": 0, "pilas": 1}

# Every day: cooldowns go down, the "start of day" passives trigger and taunts wear off
func _nuevo_dia() -> void:
	for h in heroes():
		for e in lista(h):
			e["enfriamiento"] = maxi(0, int(e["enfriamiento"]) - 1)
		if _vivo(h):
			disparar("al_empezar_dia", h)
	# The taunt lasts until the enemies have played their turn
	var dm := _dm()
	if dm:
		for e in dm.get_children():
			if e.has_meta("provocado_por"):
				var d := int(e.get_meta("provocado_dias", 1)) - 1
				if d <= 0:
					e.remove_meta("provocado_por")
					e.remove_meta("provocado_dias")
				else:
					e.set_meta("provocado_dias", d)
	cambio.emit()

# ==========================================
# EXPERIENCE AND LEVELS
# ==========================================
func xp_para(nivel: int) -> int:
	return 15 * nivel + 5 * (nivel - 1) * (nivel - 1)

# XP an enemy is worth (its own meta, or from its health and strength)
func xp_de_enemigo(e: Node) -> int:
	if e.has_meta("xp"):
		return int(e.get_meta("xp"))
	return 6 + int(e.get_meta("hp_max", 10)) / 3 + int(e.get_meta("fuerza", 4)) / 2

## Called by the CombatManager, Especiales and the abilities when an enemy falls
func enemigo_muerto(e: Node, asesino: Node) -> void:
	if e == null or e.has_meta("xp_repartida"):
		return
	e.set_meta("xp_repartida", true)
	if e.has_meta("jefe"):
		Zonas.jefe_muerto(e, asesino)
	var xp := xp_de_enemigo(e)
	if asesino and _vivo(asesino):
		ganar_xp(asesino, xp)
		disparar("al_matar", asesino, e)
	for h in heroes():
		if h != asesino and _vivo(h):
			ganar_xp(h, maxi(1, ceili(xp * PARTE_GRUPO)))

# Adds XP and queues every level up it reaches
func ganar_xp(h: Node, cantidad: int) -> void:
	if cantidad <= 0:
		return
	h.set_meta("xp", int(h.get_meta("xp", 0)) + cantidad)
	var f := _ficha(h)
	if f and f.visible:
		Indicadores.mostrar_en(f, "xp", cantidad)
	while int(h.get_meta("xp")) >= xp_para(int(h.get_meta("nivel", 1))):
		h.set_meta("xp", int(h.get_meta("xp")) - xp_para(int(h.get_meta("nivel", 1))))
		subir_nivel(h)
	cambio.emit()

# One level up: stats grow by class, the health gained is added, abilities recharge and the draft gets queued
func subir_nivel(h: Node) -> void:
	var nivel := int(h.get_meta("nivel", 1)) + 1
	h.set_meta("nivel", nivel)
	var crec: Dictionary = CRECIMIENTO.get(str(h.get_meta("clase", "")), CRECIMIENTO_COMUN)
	var base: Dictionary = h.get_meta("stats_base", {})
	for s in crec:
		base[s] = int(base.get(s, 0)) + int(crec[s])
	h.set_meta("stats_base", base)
	var pm := _pm()
	if pm:
		pm.call("RecalcularTodo")
	if crec.has("hp_max"):
		h.set_meta("hp_actual", mini(int(h.get_meta("hp_actual", 0)) + int(crec["hp_max"]), int(h.get_meta("hp_max", 1))))
	restaurar(h, false)
	pendientes.append({"heroe": str(h.name), "nivel": nivel, "crecimiento": crec})
	var banner := get_node_or_null("/root/Main/CanvasLayer/Banner")
	if banner:
		banner.anunciar("¡%s sube a nivel %d!" % [str(h.get_meta("nombre_display", h.name)), nivel])
	var audio := get_node_or_null("/root/AudioManager")
	if audio:
		audio.play_sfx("success")
	print("[Progreso] %s sube a nivel %d" % [h.name, nivel])

# ==========================================
# DRAFT AND LEARNING
# ==========================================
## Up to 3 different abilities they can get (from their class or global). Actives they already have
## don't show up; passives they have show up if they can still stack.
func opciones_draft(h: Node, cuantas: int = 3) -> Array:
	var clase := str(h.get_meta("clase", ""))
	var candidatas := []
	for id in habilidades:
		var d: HabilidadData = habilidades[id]
		if d.clase != "" and d.clase != clase:
			continue
		var e := _buscar(h, id)
		if e.size() > 0 and (d.es_activa() or int(e["pilas"]) >= d.max_pilas):
			continue
		candidatas.append(d)
	var elegidas := []
	while elegidas.size() < cuantas and not candidatas.is_empty():
		var total := 0
		for d in candidatas:
			total += maxi(1, d.peso_draft)
		var r := randi_range(1, total)
		for d in candidatas:
			r -= maxi(1, d.peso_draft)
			if r <= 0:
				elegidas.append(d)
				candidatas.erase(d)
				break
	return elegidas

# The record of ability 'id' on that hero ({} if they don't have it)
func _buscar(h: Node, id: String) -> Dictionary:
	for e in lista(h):
		if e["id"] == id:
			return e
	return {}

## Does it take a new slot? (a passive they already have only stacks)
func necesita_ranura(h: Node, id: String) -> bool:
	return _buscar(h, id).is_empty()

# No free slot left?
func ranuras_llenas(h: Node) -> bool:
	return lista(h).size() >= max_ranuras(h)

## Learns 'id' (or stacks it); if room is needed, forgets 'olvidar'
func aprender(h: Node, id: String, olvidar: String = "") -> void:
	var d: HabilidadData = habilidades.get(id)
	if d == null:
		return
	var hab: Array = lista(h)
	var e := _buscar(h, id)
	if e.size() > 0:
		e["pilas"] = mini(int(e["pilas"]) + 1, d.max_pilas)
	else:
		if olvidar != "":
			for i in hab.size():
				if hab[i]["id"] == olvidar:
					hab.remove_at(i)
					break
		if hab.size() < max_ranuras(h):
			hab.append(_entrada(d))
	h.set_meta("habilidades", hab)
	cambio.emit()

# ==========================================
# USES AND RECHARGE
# ==========================================
## Recharges uses and cooldowns (campfire, elixir, level up)
func restaurar(h: Node, con_texto: bool = true) -> void:
	for e in lista(h):
		var d: HabilidadData = habilidades.get(e["id"])
		if d:
			e["usos"] = d.usos
			e["enfriamiento"] = 0
	if con_texto:
		_texto(h, "¡Habilidades listas!", Color(0.55, 0.75, 1.0), 28)
	cambio.emit()

# Recharges every hero (campfire)
func restaurar_todos() -> void:
	for h in heroes():
		restaurar(h, false)

# Can it be cast? "" = yes, otherwise the reason
func motivo_no_usar(h: Node, i: int) -> String:
	var l := lista(h)
	if i < 0 or i >= l.size():
		return "Ranura vacía"
	var d: HabilidadData = habilidades.get(l[i]["id"])
	if d == null:
		return "?"
	return _motivo(h, d, l[i])

# entrada = its uses/cooldown record ({} for the basic actions: no limit)
func _motivo(h: Node, d: HabilidadData, entrada: Dictionary) -> String:
	if not d.es_activa():
		return "Es pasiva: funciona sola"
	if not entrada.is_empty() and d.usos > 0 and int(entrada["usos"]) <= 0:
		return "Sin usos (fogata o elixir)"
	if not entrada.is_empty() and int(entrada["enfriamiento"]) > 0:
		return "Recargando (%d día%s)" % [entrada["enfriamiento"], "" if int(entrada["enfriamiento"]) == 1 else "s"]
	if Estados.tiene(h, "aturdido"):
		return "Aturdido"
	var dm := _dm()
	if dm and (dm.get_meta("is_moving", false) or dm.get_meta("turno_enemigo", false) or dm.get_meta("bloqueo_evento", false) or dm.get_meta("bloqueo_banner", false)):
		return "Ahora no"
	if int(h.get_meta("pa_actual", 0)) < d.coste_pa and not (dm and dm.get_meta("debug_mode", false)):
		return "Faltan PA (%d)" % d.coste_pa
	if d.objetivo != HabilidadData.Objetivo.UNO_MISMO and not hay_objetivo(h, d):
		return "Nada al alcance"
	return ""

## Basic action (HUD): ayudar, retirada... "" = it can be done
func motivo_accion(id: String) -> String:
	var h := _activo()
	var d: HabilidadData = acciones.get(id)
	if h == null or d == null:
		return "?"
	return _motivo(h, d, {})

# A basic action from the bar (help, retreat...): direct or pick a target
func lanzar_accion(id: String) -> void:
	var h := _activo()
	var d: HabilidadData = acciones.get(id)
	if h == null or d == null:
		return
	_lanzar(h, d, {})

# ==========================================
# CAST: pick a target and execute
# ==========================================
func _activo() -> Node:
	var pm := _pm()
	return pm.call("GetActiveCharacter") if pm else null

## The HUD bar: press ability 'i' of the active hero
func pulsar(i: int) -> void:
	var h := _activo()
	if h == null:
		return
	var l := lista(h)
	if i < 0 or i >= l.size() or not habilidades.has(l[i]["id"]):
		return
	_lanzar(h, habilidades[l[i]["id"]], l[i])

# Casts d (ability with its uses 'entrada', or basic action with {}): directly or picking a target
func _lanzar(h: Node, d: HabilidadData, entrada: Dictionary) -> void:
	var motivo := _motivo(h, d, entrada)
	var audio := get_node_or_null("/root/AudioManager")
	if motivo != "":
		if audio:
			audio.play_ui("no")
		_texto(h, motivo, Color(0.85, 0.85, 0.9), 24)
		return
	if d.objetivo == HabilidadData.Objetivo.UNO_MISMO:
		cancelar()
		_ejecutar(h, d, entrada, h.get_meta("grid_pos"))
		return
	# Pick a target: its reach and the valid targets show; click to cast, right click cancels
	apuntando = {"heroe": str(h.name), "datos": d, "entrada": entrada, "celdas": celdas_objetivo(h, d)}
	_pintar(h, d, apuntando["celdas"])
	if audio:
		audio.play_ui("open")

# Stops aiming (right click or ESC) and clears the reach from the map
func cancelar() -> void:
	if apuntando.is_empty():
		return
	apuntando = {}
	var dm := _dm()
	if dm:
		dm.call("OcultarRangoAccion")

func _unhandled_input(event: InputEvent) -> void:
	if apuntando.is_empty():
		return
	if (event is InputEventMouseButton and event.pressed and event.button_index == MOUSE_BUTTON_RIGHT) or (event is InputEventKey and event.pressed and event.physical_keycode == KEY_ESCAPE):
		cancelar()
		get_viewport().set_input_as_handled()

## main.gd: click on the map while aiming
func confirmar(celda: Vector2i) -> void:
	if apuntando.is_empty():
		return
	var h: Node = _pm().get_node_or_null(NodePath(apuntando["heroe"]))
	var d: HabilidadData = apuntando["datos"]
	var entrada: Dictionary = apuntando["entrada"]
	var celdas: Array = apuntando["celdas"]
	cancelar()
	if h == null or not celda in celdas:
		var audio := get_node_or_null("/root/AudioManager")
		if audio:
			audio.play_ui("no")
		return
	_ejecutar(h, d, entrada, celda)

func _dist(a: Vector2i, b: Vector2i) -> int:
	return absi(a.x - b.x) + absi(a.y - b.y)

# Enemies visible on the map
func _enemigos() -> Array:
	var out := []
	for e in Tactica.unidades():
		if not Tactica.es_heroe(e) and e is Node3D and e.visible:
			out.append(e)
	return out

# The visible enemy on that tile, or null
func _enemigo_en(c: Vector2i) -> Node3D:
	for e in _enemigos():
		if e.get_meta("grid_pos") == c:
			return e
	return null

func _ve(desde: Vector2i, hasta: Vector2i) -> bool:
	var dm := _dm()
	return dm == null or Tactica.linea_de_vision(desde, hasta) # Without calling C++ (same algorithm)

## Is there anyone to cast it at? The HUD asks every frame and computing the tiles traces lines of
## sight, so it's remembered for a moment (or until the hero moves)
const VIDA_CACHE_OBJETIVO := 250 # ms
var _cache_objetivo := {}

func hay_objetivo(h: Node, d: HabilidadData) -> bool:
	var clave := "%d|%s|%s" % [h.get_instance_id(), d.obtener_id(), str(h.get_meta("grid_pos", Vector2i.ZERO))]
	var ahora := Time.get_ticks_msec()
	var c = _cache_objetivo.get(clave)
	if c != null and ahora - int(c[0]) < VIDA_CACHE_OBJETIVO:
		return c[1]
	var r := not celdas_objetivo(h, d).is_empty()
	if _cache_objetivo.size() > 64:
		_cache_objetivo.clear()
	_cache_objetivo[clave] = [ahora, r]
	return r

## Tiles where it can be cast
func celdas_objetivo(h: Node, d: HabilidadData) -> Array:
	var pos: Vector2i = h.get_meta("grid_pos")
	var out := []
	match d.objetivo:
		HabilidadData.Objetivo.ENEMIGO:
			for e in _enemigos():
				var c: Vector2i = e.get_meta("grid_pos")
				var dd := _dist(pos, c)
				if dd >= d.alcance_min and dd <= d.alcance_max and (not d.requiere_vision or _ve(pos, c)):
					if d.movimiento == "carga" and not _linea_libre(pos, c):
						continue
					out.append(c)
		HabilidadData.Objetivo.ALIADO:
			for o in heroes():
				if _vivo(o) and (o != h or d.alcance_min == 0):
					var c: Vector2i = o.get_meta("grid_pos")
					var dd := _dist(pos, c)
					if dd >= d.alcance_min and dd <= d.alcance_max:
						out.append(c)
		HabilidadData.Objetivo.CASILLA:
			for c in _celdas_en_rango(pos, d):
				# Teleport: only to free tiles
				if d.movimiento == "teletransporte" and (Tactica.unidad_en(c) != null or Tactica.solido(c)):
					continue
				out.append(c)
	return out

# Charge: in a straight line with nothing in the way
func _linea_libre(desde: Vector2i, hasta: Vector2i) -> bool:
	if desde.x != hasta.x and desde.y != hasta.y:
		return false
	var dir := Tactica.dir_hacia(desde, hasta)
	var c := desde + dir
	while c != hasta:
		if Tactica.solido(c) or Tactica.unidad_en(c) != null:
			return false
		c += dir
	return true

# Tiles within the ability's range with line of sight (and its own rules: free tile, straight line...)
func _celdas_en_rango(pos: Vector2i, d: HabilidadData) -> Array:
	var dm := _dm()
	var out := []
	if dm == null:
		return out
	var libres: PackedByteArray = dm.call("ObtenerTransitables")
	var lado := int(Engine.get_meta("alturas_lado", 40))
	var ocupadas := {}
	for e in _enemigos():
		ocupadas[e.get_meta("grid_pos")] = true
	for o in heroes():
		ocupadas[o.get_meta("grid_pos")] = true
	for x in range(maxi(0, pos.x - d.alcance_max), mini(lado, pos.x + d.alcance_max + 1)):
		for y in range(maxi(0, pos.y - d.alcance_max), mini(lado, pos.y + d.alcance_max + 1)):
			var c := Vector2i(x, y)
			var dd := _dist(pos, c)
			if dd < d.alcance_min or dd > d.alcance_max:
				continue
			if libres[x * lado + y] != 1 and not ocupadas.has(c):
				continue
			if d.requiere_vision and not _ve(pos, c):
				continue
			out.append(c)
	return out

## Paints the reach (blue) and the targets (red) on the map
func _pintar(h: Node, d: HabilidadData, objetivos: Array) -> void:
	var dm := _dm()
	var rend := dm.get_node_or_null("DungeonRenderer") if dm else null
	if rend == null:
		return
	var ternas := PackedInt32Array()
	var marcadas := {}
	for c in objetivos:
		marcadas[c] = true
		_terna(ternas, c, 2 if _enemigo_en(c) else 4)
	if d.objetivo != HabilidadData.Objetivo.CASILLA:
		for c in _celdas_en_rango(h.get_meta("grid_pos"), d):
			if not marcadas.has(c):
				_terna(ternas, c, 4)
	rend.call("PintarAlcance", ternas)

# Appends an (x, y, type) triplet for the renderer
func _terna(ternas: PackedInt32Array, c: Vector2i, tipo: int) -> void:
	ternas.append(c.x)
	ternas.append(c.y)
	ternas.append(tipo)

## HUD: hovering an ability shows its reach
func mostrar_rango(i: int) -> void:
	var h := _activo()
	if h == null or not apuntando.is_empty():
		return
	var l := lista(h)
	if i >= l.size():
		return
	mostrar_rango_de(h, habilidades.get(l[i]["id"]))

# Shows the reach of an ability while its card is hovered (not while aiming another one)
func mostrar_rango_de(h: Node, d: HabilidadData) -> void:
	if h == null or d == null or not d.es_activa() or not apuntando.is_empty():
		return
	var dm := _dm()
	if dm and (dm.get_meta("is_moving", false) or dm.get_meta("turno_enemigo", false)):
		return
	if d.objetivo == HabilidadData.Objetivo.UNO_MISMO:
		# Whatever it reaches around it (area) or its own tile
		var pos: Vector2i = h.get_meta("grid_pos")
		var celdas := []
		for x in range(pos.x - d.area, pos.x + d.area + 1):
			for y in range(pos.y - d.area, pos.y + d.area + 1):
				if _dist(pos, Vector2i(x, y)) <= d.area:
					celdas.append(Vector2i(x, y))
		var rend := dm.get_node_or_null("DungeonRenderer")
		var ternas := PackedInt32Array()
		for c in celdas:
			_terna(ternas, c, 2 if _enemigo_en(c) else 4)
		rend.call("PintarAlcance", ternas)
	else:
		_pintar(h, d, celdas_objetivo(h, d))

# Hides it again when the mouse leaves the card
func ocultar_rango() -> void:
	if apuntando.is_empty():
		var dm := _dm()
		if dm and not dm.get_meta("is_moving", false):
			dm.call("OcultarRangoAccion")

# ------------------------------------------
# Execute
# ------------------------------------------
func _ejecutar(h: Node, d: HabilidadData, e: Dictionary, celda: Vector2i) -> void:
	var dm := _dm()
	var debug: bool = dm.get_meta("debug_mode", false)
	# Cost: PA, one use (except Arcane echo) and its cooldown. Basic actions have no uses.
	if not debug:
		h.set_meta("pa_actual", maxi(0, int(h.get_meta("pa_actual", 0)) - d.coste_pa))
	if not e.is_empty():
		if _proba_pasiva(h, HabilidadData.Reaccion.NO_GASTAR_USO):
			_texto(h, "¡Eco!", Color(0.6, 0.8, 1.0), 26)
		elif d.usos > 0:
			e["usos"] = int(e["usos"]) - 1
		e["enfriamiento"] = d.enfriamiento
	cambio.emit()
	# Sequence: announcement + camera on top, the effect and the end
	dm.set_meta("is_moving", true)
	dm.call("ApagarLucesMapa")
	var banner := get_node_or_null("/root/Main/CanvasLayer/Banner")
	if banner:
		banner.anunciar(d.nombre)
	var cam := dm.get_node_or_null("Camera3D")
	var fh := _ficha(h)
	var objetivo_pos: Vector3 = Vector3(celda.x + 0.5, 0, celda.y + 0.5)
	if cam and fh:
		var puntos := PackedVector3Array([fh.position])
		if celda != h.get_meta("grid_pos"):
			puntos.append(objetivo_pos)
		cam.set_meta("encuadre", puntos)
	await get_tree().create_timer(0.45).timeout
	await _efecto(h, d, celda)
	if not e.is_empty():
		disparar("al_usar_habilidad", h)
	await get_tree().create_timer(0.55).timeout
	if cam and cam.has_meta("encuadre"):
		cam.remove_meta("encuadre")
	dm.set_meta("is_moving", false)
	var ui := get_node_or_null("/root/Main/CanvasLayer")
	if ui:
		ui.call("UpdateWarriorUI")
	dm.call("RefrescarLuces")
	if int(h.get_meta("pa_actual", 0)) <= 0 and not debug:
		dm.call("PedirFinDeTurno")
	cambio.emit()

# What the ability actually does: custom behaviour, movement, targets, damage, states, pushes, hazards
func _efecto(h: Node, d: HabilidadData, celda: Vector2i) -> void:
	var pm := _pm()
	var pos: Vector2i = h.get_meta("grid_pos")
	# 1. Custom behaviours (the ones that don't fit the generic stuff)
	match d.especial:
		"gallina":
			var esp := get_node_or_null("/root/Especiales")
			var err: String = esp.invocar_gallina(h) if esp else "?"
			if err != "":
				_texto(h, err, Color(1, 0.6, 0.5), 22)
		"provocar":
			for en in _enemigos():
				if _dist(pos, en.get_meta("grid_pos")) <= d.area:
					en.set_meta("provocado_por", str(h.name))
					en.set_meta("provocado_dias", 1)
					en.set_meta("in_combat", true)
					_texto(en, "¡Grrr!", Color(1, 0.45, 0.35), 26)
		"grito":
			for o in heroes():
				if _vivo(o) and _dist(pos, o.get_meta("grid_pos")) <= d.area:
					pm.call("AplicarEfectosA", str(o.name), d.efectos_objetivo)
		"muro":
			for o in heroes():
				if o != h and _vivo(o) and _dist(pos, o.get_meta("grid_pos")) <= d.area:
					pm.call("AplicarEfectosA", str(o.name), d.efectos_objetivo)
	if d.especial in ["gallina", "provocar", "grito", "muro"]:
		_propios(h, d)
		return

	# 2. Caster movement (Tactica: reusable)
	var bono_carga := 0
	match d.movimiento:
		"teletransporte":
			Tactica.colocar(h, celda, 0.22)
			await get_tree().create_timer(0.25).timeout
		"intercambio":
			var otro := Tactica.unidad_en(celda, h)
			if otro:
				Tactica.intercambiar(h, otro)
				await get_tree().create_timer(0.25).timeout
		"carga":
			var dir := Tactica.dir_hacia(pos, celda)
			var hasta: Vector2i = celda - dir
			bono_carga = _dist(pos, hasta)
			if hasta != pos:
				Tactica.colocar(h, hasta, 0.07 * bono_carga + 0.08)
				await get_tree().create_timer(0.07 * bono_carga + 0.1).timeout
	pos = h.get_meta("grid_pos")

	# 3. Targets
	var objetivos := []
	var centro := celda
	match d.objetivo:
		HabilidadData.Objetivo.ENEMIGO:
			var primero := _enemigo_en(celda)
			if primero:
				objetivos.append(primero)
			# Chain lightning: jumps to the closest enemy it hasn't touched yet
			var ultimo: Node3D = primero
			for s in d.saltos:
				var mejor: Node3D = null
				var md := 99
				for en in _enemigos():
					if en in objetivos or ultimo == null:
						continue
					var dd := _dist(ultimo.get_meta("grid_pos"), en.get_meta("grid_pos"))
					if dd <= 3 and dd < md:
						md = dd
						mejor = en
				if mejor == null:
					break
				objetivos.append(mejor)
				ultimo = mejor
		HabilidadData.Objetivo.CASILLA:
			if d.area > 0 or d.dano_base > 0:
				for en in _enemigos():
					if _dist(celda, en.get_meta("grid_pos")) <= d.area:
						objetivos.append(en)
		HabilidadData.Objetivo.UNO_MISMO:
			centro = pos
			if d.area > 0:
				for en in _enemigos():
					if _dist(pos, en.get_meta("grid_pos")) <= d.area:
						objetivos.append(en)
		HabilidadData.Objetivo.ALIADO:
			for o in heroes():
				if o.get_meta("grid_pos") == celda and o != h or (o == h and celda == pos and d.alcance_min == 0):
					pm.call("AplicarEfectosA", str(o.name), d.efectos_objetivo)
					if d.estado != "":
						Estados.aplicar(o, d.estado, d.estado_dias, d.estado_potencia, h)

	# 4. Damage (with roll and advantage), states and pushes on the ones hit
	if d.objetivo != HabilidadData.Objetivo.ALIADO:
		if d.objetivo == HabilidadData.Objetivo.CASILLA and d.dano_base > 0 and d.area > 0:
			# The area doesn't discriminate: heroes in the zone get theirs too
			for o in heroes():
				if _vivo(o) and o != h and _dist(celda, o.get_meta("grid_pos")) <= d.area:
					pm.call("RecibirDano", maxi(1, (d.dano_base + int(int(h.get_meta(d.stat_dano, 0)) * d.mult_stat)) / 2), o)
			var esp2 := get_node_or_null("/root/Especiales")
			if esp2 and d.area > 0:
				esp2.texto(Tactica.mundo(celda) + Vector3(0, 0.3, 0), "¡BUM!", Color(1, 0.55, 0.2), 44)
		var alcanzados: Array = await _danar_varios(h, d, objetivos, bono_carga)
		for en in alcanzados:
			if not Tactica.vivo(en):
				continue
			if d.estado != "" and randi_range(1, 100) <= d.estado_prob:
				Estados.aplicar(en, d.estado, d.estado_dias, d.estado_potencia, h)
			if d.empuje != 0:
				var dir := Tactica.dir_hacia(pos, en.get_meta("grid_pos"))
				if d.empuje > 0:
					Tactica.empujar(en, dir, d.empuje, h)
				else:
					Tactica.atraer(en, h, -d.empuje, h)
			if d.movimiento == "atraer":
				Tactica.atraer(en, h, 99, h)
		# 5. Hazards on the area floor
		if d.peligro != "":
			for x in range(centro.x - d.area, centro.x + d.area + 1):
				for y in range(centro.y - d.area, centro.y + d.area + 1):
					var c := Vector2i(x, y)
					if _dist(centro, c) <= d.area:
						Tactica.crear_peligro(c, d.peligro, d.peligro_dias, h)
	_propios(h, d)

# Effects and state on the caster
func _propios(h: Node, d: HabilidadData) -> void:
	if not d.efectos_propios.is_empty():
		_pm().call("AplicarEfectosA", str(h.name), d.efectos_propios)
	if d.estado_propio != "":
		Estados.aplicar(h, d.estado_propio, d.estado_propio_dias, 1, h)

## One roll (d20 over the caster, with advantage/disadvantage) and the damage to every target.
## Returns the enemies hit (for states and pushes). No damage and no roll: all of them.
func _danar_varios(h: Node, d: HabilidadData, objetivos: Array, bono_extra: int = 0) -> Array:
	if objetivos.is_empty():
		if d.objetivo != HabilidadData.Objetivo.UNO_MISMO or d.area > 0:
			_texto(h, "Nadie al alcance", Color(0.8, 0.8, 0.85), 24)
		return []
	var umbral := 15 if d.especial == "punalada" else 20
	var stat := int(h.get_meta(d.stat_dano, 0))
	var critico := false
	if d.tirada:
		var mods: Dictionary = Tactica.modificadores(h, objetivos[0])
		var ventaja := int(mods["ventaja"])
		var d20 := _d20()
		if ventaja != 0:
			var otro := _d20()
			d20 = maxi(d20, otro) if ventaja > 0 else mini(d20, otro)
		Tactica.tras_atacar(h)
		var bono := int(floor(stat * (1.0 + int(h.get_meta("suerte", 0)) * int(h.get_meta("nivel", 1)) * 0.03) / 2.0)) + int(mods["bono"])
		critico = d20 >= umbral
		# Hits if it beats the first one's AC (the rest, with the same roll)
		var ca := 8 + int(objetivos[0].get_meta("defensa", 0))
		var acierta := d20 != 1 and (critico or d20 + bono >= ca)
		var dado := get_node_or_null("/root/DadoCombate")
		if dado:
			var sello := "FALLO" if not acierta else ("¡CRÍTICO!" if critico else "")
			var espera = dado.tirar(d20, 1 if acierta else 0, sello, "", _ficha(h), ventaja)
			await get_tree().create_timer(float(espera)).timeout
		if not acierta:
			disparar("al_fallar", h, objetivos[0])
			return []
	var hace_dano := d.dano_base > 0 or d.mult_stat > 0.0
	for en in objetivos:
		if not is_instance_valid(en) or int(en.get_meta("hp_actual", 0)) <= 0:
			continue
		if hace_dano:
			var dano := maxi(1, d.dano_base + int(stat * d.mult_stat) + bono_extra - int(en.get_meta("defensa", 0)))
			if critico:
				dano *= 2
			golpear_enemigo(h, en, dano)
	var feel := get_node_or_null("/root/Feel")
	if feel and hace_dano:
		feel.sacudir(0.45 if critico else 0.25)
	if critico:
		disparar("al_critico", h, objetivos[0])
	if hace_dano:
		disparar("al_golpear", h, objetivos[0])
	return objetivos

# A d20 (or the forced one the test sets)
func _d20() -> int:
	var tf: int = int(Engine.get_meta("tirada_forzada")) if Engine.has_meta("tirada_forzada") else 0
	return clampi(tf, 1, 20) if tf > 0 else randi_range(1, 20)

## Takes health from an enemy; if it falls, loot and XP for 'h'
func golpear_enemigo(h: Node, en: Node, dano: int, puede_matar: bool = true) -> void:
	if en == null or not is_instance_valid(en) or str(en.name).ends_with("_muerto"):
		return
	var hp := int(en.get_meta("hp_actual", 1)) - dano
	en.set_meta("hp_actual", hp)
	en.set_meta("in_combat", true)
	if hp <= 0 and puede_matar:
		matar(en, h)

# An enemy dies by an ability: last indicator, burst, loot and XP
func matar(en: Node, h: Node) -> void:
	if en == null or not is_instance_valid(en) or str(en.name).ends_with("_muerto"):
		return
	Indicadores.revisar(en)
	enemigo_muerto(en, h)
	var dm := _dm()
	var combate := dm.get_node_or_null("CombatManager") if dm else null
	var c: Vector2i = en.get_meta("grid_pos")
	if combate:
		combate.call("MatarEnemigoPor", str(h.name) if h else "", c.x, c.y)

# ==========================================
# PASSIVES: the combos
# ==========================================
func _evento_a_disparador(evento: String) -> int:
	return {"al_golpear": HabilidadData.Disparador.AL_GOLPEAR, "al_critico": HabilidadData.Disparador.AL_CRITICO,
		"al_fallar": HabilidadData.Disparador.AL_FALLAR, "al_matar": HabilidadData.Disparador.AL_MATAR,
		"al_recibir_golpe": HabilidadData.Disparador.AL_RECIBIR_GOLPE, "al_empezar_dia": HabilidadData.Disparador.AL_EMPEZAR_DIA,
		"al_usar_habilidad": HabilidadData.Disparador.AL_USAR_HABILIDAD}.get(evento, -1)

# Does any passive with this trigger fire? (chance x stacks, capped at 100%)
func _proba_pasiva(h: Node, reaccion: int) -> bool:
	for e in lista(h):
		var d: HabilidadData = habilidades.get(e["id"])
		if d and not d.es_activa() and d.reaccion == reaccion and randf() * 100.0 < minf(100.0, d.probabilidad * int(e["pilas"])):
			return true
	return false

## Called by C++ (hits, criticals, misses, taking hits) and by this script (kills, days, abilities).
## puede_matar = false when the caller already handles the target's death (CombatManager).
func disparar(evento: String, h: Node, objetivo: Node = null, puede_matar: bool = true) -> void:
	if h == null or not is_instance_valid(h) or not h.has_meta("habilidades") or _profundidad > 2:
		return
	var disp := _evento_a_disparador(evento)
	if disp < 0:
		return
	_profundidad += 1
	for e in lista(h).duplicate():
		var d: HabilidadData = habilidades.get(e["id"])
		if d == null or d.es_activa() or d.disparador != disp or d.reaccion in [HabilidadData.Reaccion.NO_GASTAR_USO, HabilidadData.Reaccion.NINGUNA]:
			continue
		if d.condicion_estado != "" and not (objetivo and Estados.tiene(objetivo, d.condicion_estado)):
			continue
		var pilas := int(e["pilas"])
		if randf() * 100.0 >= minf(100.0, d.probabilidad * pilas):
			continue
		_reaccionar(h, d, pilas, objetivo, puede_matar)
	_profundidad -= 1

# One passive firing: what it does depends on its trigger
func _reaccionar(h: Node, d: HabilidadData, pilas: int, objetivo: Node, puede_matar: bool) -> void:
	var pm := _pm()
	var v := d.valor * pilas
	var color := Color(0.75, 0.6, 1.0)
	match d.reaccion:
		HabilidadData.Reaccion.DANO_EXTRA:
			if objetivo and str(objetivo.name).begins_with("Enemy_"):
				golpear_enemigo(h, objetivo, v, puede_matar)
				_texto(objetivo, d.nombre, color, 24)
		HabilidadData.Reaccion.CONTRAGOLPE:
			if objetivo and str(objetivo.name).begins_with("Enemy_"):
				golpear_enemigo(h, objetivo, v + int(h.get_meta("fuerza", 0)) / 3, true)
				_texto(objetivo, d.nombre, color, 24)
		HabilidadData.Reaccion.REBOTE:
			if objetivo and objetivo.has_meta("grid_pos"):
				var mejor: Node3D = null
				var md := 99
				for en in _enemigos():
					if en == objetivo:
						continue
					var dd := _dist(objetivo.get_meta("grid_pos"), en.get_meta("grid_pos"))
					if dd <= 3 and dd < md:
						md = dd
						mejor = en
				if mejor:
					golpear_enemigo(h, mejor, v, true)
					_texto(mejor, "¡%s!" % d.nombre, Color(0.6, 0.85, 1.0), 24)
		HabilidadData.Reaccion.CURAR:
			pm.call("AplicarEfectosA", str(h.name), [_ef(Efecto.Tipo.CURAR, v)])
			_texto(h, d.nombre, Color(0.5, 1.0, 0.55), 24)
		HabilidadData.Reaccion.RECUPERAR_USO:
			var vacias := []
			for e in lista(h):
				var dd: HabilidadData = habilidades.get(e["id"])
				if dd and dd.es_activa() and int(e["usos"]) < dd.usos:
					vacias.append(e)
			if not vacias.is_empty():
				var e2: Dictionary = vacias.pick_random()
				e2["usos"] = int(e2["usos"]) + 1
				_texto(h, "+1 uso", Color(0.55, 0.75, 1.0), 24)
		HabilidadData.Reaccion.BAJAR_ENFRIAMIENTO:
			for e in lista(h):
				e["enfriamiento"] = maxi(0, int(e["enfriamiento"]) - v)
			_texto(h, d.nombre, Color(0.55, 0.75, 1.0), 24)
		HabilidadData.Reaccion.PA_EXTRA:
			h.set_meta("pa_actual", int(h.get_meta("pa_actual", 0)) + v)
			_texto(h, "+%d PA" % v, Color(1.0, 0.85, 0.35), 26)
		HabilidadData.Reaccion.STAT_TEMPORAL:
			var ef := _ef(Efecto.Tipo.MODIFICAR_STAT, v)
			ef.stat = d.stat
			ef.duracion_turnos = d.duracion
			pm.call("AplicarEfectosA", str(h.name), [ef])
		HabilidadData.Reaccion.APLICAR_ESTADO:
			if objetivo and Tactica.vivo(objetivo):
				Estados.aplicar(objetivo, d.estado, d.estado_dias, maxi(1, d.estado_potencia * pilas), h)
		HabilidadData.Reaccion.ORO:
			pm.call("AplicarEfectosA", str(h.name), [_ef(Efecto.Tipo.ORO, v)])
			_texto(h, "+%d oro" % v, Color(1.0, 0.85, 0.3), 24)
	cambio.emit()

# Builds an Efecto on the fly (to reuse the C++ effect engine)
func _ef(tipo: int, valor: int) -> Efecto:
	var ef := Efecto.new()
	ef.tipo = tipo
	ef.valor = valor
	ef.probabilidad = 100
	return ef

# ==========================================
# TEXTS (HUD, draft)
# ==========================================
func describir(h: Node, i: int) -> String:
	var l := lista(h)
	if i >= l.size():
		return "[color=#9a9cab]Ranura libre: al subir de nivel aprenderás algo nuevo.[/color]"
	var e: Dictionary = l[i]
	var d: HabilidadData = habilidades.get(e["id"])
	if d == null:
		return "?"
	var t := "[b]%s[/b]  [color=#9a9cab]%s · %s[/color]\n%s\n[color=#d7b24a]%s[/color]" % [d.nombre, d.etiqueta_clase(), "ACTIVA" if d.es_activa() else "PASIVA", d.descripcion, d.resumen()]
	if d.es_activa():
		t += "\n[color=#d9d6cc]Usos: %d/%d%s[/color]" % [e["usos"], d.usos, ("  · lista en %d día%s" % [e["enfriamiento"], "" if int(e["enfriamiento"]) == 1 else "s"]) if int(e["enfriamiento"]) > 0 else ""]
	elif int(e["pilas"]) > 1:
		t += "\n[color=#8fcf6a]Apilada x%d[/color]" % e["pilas"]
	return t
