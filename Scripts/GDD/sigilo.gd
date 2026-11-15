extends Node

# Sigilo (autoload): where each enemy is looking, who sees you and the STEALTH mode.
#
#   FACING       Every enemy looks in one direction (meta "mirada": one of DIRS) and has a type
#                (meta "vigilancia"):
#                  - CENTINELA: never turns (faces the entrance / the boss door).
#                  - VIGIA: turns 90° (clockwise) at the end of every round. Its next cone shows
#                    as a ghost: you can plan around it.
#                  - DORMIDO: no cone; only wakes up if a hero ends right next to it.
#                In combat they face where they move and whoever hits them.
#   SEEING       Binary: it sees you if you're in its front cone (90°, its detection radius, with
#                line of sight) or right next to its side. From behind, never. A HIDDEN hero isn't
#                seen next to its side if there's a wall or column beside them, nor at the far end
#                of the cone (last 2 tiles) with the torch out. If it sees you: instant combat.
#   STEALTH MODE Starts when a hero sees an enemy that hasn't seen you. There are turns (PA) like
#                in combat, but the enemies only KEEP WATCH: at the start of every round the
#                lookouts turn and we check again who sees you (if they see you that way, they
#                play first). If a round goes by with no unaware enemy in view, back to exploring.
#   BREAKING IT  Hitting an unaware enemy starts combat with SURPRISE (the squads that hadn't seen
#                you lose their first turn). From behind an unaware one: BACKSTAB (guaranteed
#                critical). From behind in combat: advantage.
#   ACTIONS      hide (1 PA; walking hidden costs 2 PA per tile) and torch (free).
#
# C++ calls: comprobar(heroe, x, y) on every step (0 nothing, 1 stealth starts, 2 you've been seen)
# and despertar_quien_vea() before the enemy turn. Tactica.modificadores asks por_la_espalda().

const RUTA_DM := "/root/Main/ContenedorMapa/SubViewport/DungeonManager"
const RUTA_PM := "/root/Main/PartyManager"
const CENTINELA := "centinela"
const VIGIA := "vigia"
const DORMIDO := "dormido"
const DIRS := [Vector2i(0, -1), Vector2i(1, 0), Vector2i(0, 1), Vector2i(-1, 0)] # Clockwise
const NOMBRES := {CENTINELA: "Centinela", VIGIA: "Vigía", DORMIDO: "Dormido"}
const COLOR_SIGILO := Color(0.74, 0.58, 1.0)
# Heroes see their light + VISION_EXTRA (= Mazmorra.hpp); an enemy only sees a hero up to
# VENTAJA_VISTA tiles less than that hero sees: you always see them first
const VISION_EXTRA := 3
const VENTAJA_VISTA := 2

signal cambio(sigilo: bool)

var en_sigilo := false
var dia_visto := -1
var semilla_vista := -1
var combate_visto := false
var _conos: Node3D
var _firma_conos := ""
var _t_conos := 0.0
var _hp := {}    # instance_id -> health (to turn towards whoever hits it)
var _pos := {}   # instance_id -> tile (to face where it walks)

func _dm() -> Node:
	return get_node_or_null(RUTA_DM)

func _pm() -> Node:
	return get_node_or_null(RUTA_PM)

# ==========================================
# QUERIES
# ==========================================
func tipo(e: Node) -> String:
	return str(e.get_meta("vigilancia", CENTINELA))

# The direction an enemy is looking (east by default)
func mirada(e: Node) -> Vector2i:
	return e.get_meta("mirada", Vector2i(1, 0))

# Enemies on the map
func _enemigos() -> Array:
	var out := []
	for u in Tactica.unidades():
		if not Tactica.es_heroe(u):
			out.append(u)
	return out

# Heroes on the map
func _heroes() -> Array:
	var out := []
	for u in Tactica.unidades():
		if Tactica.es_heroe(u):
			out.append(u)
	return out

## It hasn't seen you (or just did: the surprise lasts its first turn)
func desprevenido(e: Node) -> bool:
	return e != null and is_instance_valid(e) and not Tactica.es_heroe(e) and (bool(e.get_meta("desprevenido", false)) or Estados.tiene(e, "sorprendido"))

## Enemies that haven't seen you and that the heroes see right now (what starts the stealth)
func vigilantes() -> Array:
	var out := []
	for e in _enemigos():
		if not e.get_meta("in_combat", false) and e is Node3D and e.visible:
			out.append(e)
	return out

const DIST_SIGILO := 8 # Stealth goes on while an unaware one is this close and in view

## Unaware ones close to some hero (DIST_SIGILO or less, with line of sight), even if the light
## doesn't show them: putting out the torch doesn't take you out of stealth
func acechados() -> Array:
	var out := []
	var heroes := _heroes()
	for e in _enemigos():
		if e.get_meta("in_combat", false):
			continue
		var p: Vector2i = e.get_meta("grid_pos")
		for h in heroes:
			if Tactica.dist(Tactica.pos(h), p) <= DIST_SIGILO and _linea(Tactica.pos(h), p):
				out.append(e)
				break
	return out

## Is 'a' behind 'd' (and right next to it)? Only counts for whoever has a facing (enemies)
func por_la_espalda(a: Node, d: Node) -> bool:
	if a == null or d == null or not Tactica.es_heroe(a) or Tactica.es_heroe(d) or not d.has_meta("mirada"):
		return false
	var rel: Vector2i = Tactica.pos(a) - Tactica.pos(d)
	if Tactica.dist(Tactica.pos(a), Tactica.pos(d)) != 1:
		return false
	var f := mirada(d)
	return rel.x * f.x + rel.y * f.y < 0

func _linea(desde: Vector2i, hasta: Vector2i) -> bool:
	return Tactica.linea_de_vision(desde, hasta) # Without calling C++ (same algorithm)

# Is there a wall or column next to this tile? (cover to hide against)
func _cobertura(c: Vector2i) -> bool:
	for d in DIRS:
		if Tactica.solido(c + d):
			return true
	return false

## How far 'e' sees: its detection radius, but never beyond what the hero who sees the LEAST sees
## (with the torch lit) minus VENTAJA_VISTA. Same for every hero: the drawn cone is exactly where
## they see you, whoever you are, and you always see them first
func alcance(e: Node, _h: Node = null) -> int:
	return mini(int(e.get_meta("radio_deteccion", 6)), _luz_minima() + VISION_EXTRA - VENTAJA_VISTA)

# The light of the hero who sees the least (once per frame: it's asked a lot)
var _luz_frame := -1
var _luz_min := 4

func _luz_minima() -> int:
	var frame := Engine.get_process_frames()
	if frame != _luz_frame:
		_luz_frame = frame
		var luz := -1
		for o in _heroes():
			var l := int(o.get_meta("radio_luz", 4))
			luz = l if luz < 0 else mini(luz, l)
		_luz_min = luz if luz >= 0 else 4
	return _luz_min

# THE CONE is one single shape for everything: RAYOS_CONO+1 rays fanned out (90°) from the centre of
# the enemy's tile, each one up to its reach or to the first thing that blocks sight. Those rays
# draw the light AND decide whether it sees you: it sees you if the centre of your tile is inside
# the light.
var _rayos_de := {} # signature -> PackedFloat32Array (length of every ray)

# Length of every ray of the cone of 'e' looking towards 'f' (cached by a signature)
func _rayos(e: Node, f: Vector2i) -> PackedFloat32Array:
	var p: Vector2i = e.get_meta("grid_pos")
	var radio := float(alcance(e)) + 0.5
	var firma := str([p, f, radio, Tactica.mapa_fijo().size(), Tactica._version_fijo])
	if _rayos_de.has(firma):
		return _rayos_de[firma]
	if _rayos_de.size() > 200:
		_rayos_de.clear()
	var origen := Vector2(p) + Vector2(0.5, 0.5)
	var base := atan2(float(f.y), float(f.x))
	var largos := PackedFloat32Array()
	for i in RAYOS_CONO + 1:
		var a := base + lerpf(-PI * 0.25, PI * 0.25, float(i) / RAYOS_CONO)
		var dir := Vector2(cos(a), sin(a))
		var dist := 0.0
		while dist < radio:
			var siguiente := dist + PASO_RAYO
			var q := origen + dir * siguiente
			var c := Vector2i(floori(q.x), floori(q.y))
			if c != p and _tapa(c):
				break
			dist = siguiente
		largos.append(minf(dist, radio))
	_rayos_de[firma] = largos
	return largos

## Is the centre of tile 'c' inside the light of the cone of 'e' looking towards 'f'?
func en_luz(e: Node, c: Vector2i, f: Vector2i) -> bool:
	var p: Vector2i = e.get_meta("grid_pos")
	var v := Vector2(c - p)
	var dist := v.length()
	if dist < 0.01:
		return false
	var ang := wrapf(v.angle() - atan2(float(f.y), float(f.x)), -PI, PI)
	if absf(ang) > PI * 0.25 + 0.001:
		return false
	var largos := _rayos(e, f)
	var k := clampf((ang + PI * 0.25) / (PI * 0.5), 0.0, 1.0) * RAYOS_CONO
	var i := mini(floori(k), RAYOS_CONO - 1)
	var largo := lerpf(largos[i], largos[i + 1], k - i)
	return dist <= largo + 0.05

## Does 'e' see hero 'h' on tile 'c'? (binary)
func ve(e: Node, c: Vector2i, h: Node = null) -> bool:
	if e == null or not is_instance_valid(e) or e.get_meta("in_combat", false):
		return false
	var p: Vector2i = e.get_meta("grid_pos")
	var oculto := h != null and Estados.tiene(h, "oculto")
	if tipo(e) == DORMIDO:
		return Tactica.dist(p, c) <= 1
	var f := mirada(e)
	var radio := alcance(e, h)
	if en_luz(e, c, f):
		# Hidden and without a torch: it doesn't see you at the far end of its light (the last 2
		# tiles)
		var lejos := Vector2(c - p).length() > radio - 1.5
		return not (oculto and h.get_meta("antorcha_apagada", false) and lejos)
	var d := c - p
	var pegado := maxi(absi(d.x), absi(d.y)) <= 1
	if pegado and d.x * f.x + d.y * f.y >= 0:
		return not (oculto and _cobertura(c))
	return false

# ==========================================
# DETECTION
# ==========================================
# Checks every unaware enemy against every hero ('mueve' is at 'en'). The ones that see you wake up
# (with a "!"): Modo wakes their squad up and combat starts.
func _detectar(mueve: Node = null, en: Vector2i = Vector2i(-99, -99)) -> bool:
	var visto := false
	var heroes := _heroes()
	for e in _enemigos():
		if e.get_meta("in_combat", false):
			continue
		for h in heroes:
			var c: Vector2i = en if h == mueve else Tactica.pos(h)
			if ve(e, c, h):
				e.set_meta("in_combat", true)
				e.set_meta("me_ha_visto", true)
				if mueve == null or e is Node3D:
					e.set_meta("mirada", _hacia(e.get_meta("grid_pos"), c))
				visto = true
				break
	if visto:
		var audio := get_node_or_null("/root/AudioManager")
		if audio:
			audio.play_sfx("fail")
	return visto

## C++ (DungeonManager, on every hero step): 0 = nothing, 1 = stealth starts (it stops), 2 = they've
## been seen
func comprobar(act: Node, x: int, y: int) -> int:
	if _detectar(act, Vector2i(x, y)):
		if en_sigilo:
			_hud("¡Os han visto!", Color(1.0, 0.45, 0.35))
		return 2
	if not en_sigilo and not Modo.en_combate and not vigilantes().is_empty():
		_empezar()
		return 1
	return 0

## C++ (EnemyManager, before the enemy turn)
func despertar_quien_vea() -> bool:
	return _detectar()

# ==========================================
# STEALTH MODE
# ==========================================
func _empezar() -> void:
	en_sigilo = true
	for h in _heroes():
		h.set_meta("pa_actual", int(h.get_meta("pa_max", h.get_meta("velocidad", 4))))
	var banner := get_node_or_null("/root/Main/CanvasLayer/Banner")
	if banner:
		banner.mostrar("Sigilo")
	_hud("No os han visto: moveos con cuidado", COLOR_SIGILO)
	cambio.emit(true)

# Back to exploring: everyone stands up and the HUD says why
func _salir(motivo: String) -> void:
	en_sigilo = false
	for h in _heroes():
		Estados.quitar(h, "oculto")
	_hud(motivo, COLOR_SIGILO)
	cambio.emit(false)

# At the start of every round (day): the lookouts turn and we check again who sees you
func _vigilar() -> void:
	for e in _enemigos():
		if e.get_meta("in_combat", false):
			continue
		if tipo(e) == VIGIA:
			e.set_meta("mirada", _siguiente(mirada(e)))
	if _detectar():
		_hud("¡Al girarse os ha visto!", Color(1.0, 0.45, 0.35))
		_juegan_ellos_primero()
	elif acechados().is_empty():
		_salir("Fuera de su vista")
	_firma_conos = ""

# They saw you when turning: combat starts with their turn (after the surprise animation)
func _juegan_ellos_primero() -> void:
	await get_tree().create_timer(1.6).timeout
	var dm := _dm()
	if dm == null or dm.get_meta("turno_enemigo", false) or not Modo.en_combate:
		return
	var em := dm.get_node_or_null("EnemyManager")
	if em:
		em.call("IniciarTurnoEnemigos")

# Next direction clockwise (lookouts)
func _siguiente(f: Vector2i) -> Vector2i:
	return DIRS[(DIRS.find(f) + 1) % DIRS.size()]

# Cardinal direction that best points from 'desde' to 'hasta'
func _hacia(desde: Vector2i, hasta: Vector2i) -> Vector2i:
	var d := hasta - desde
	if d == Vector2i.ZERO:
		return Vector2i(1, 0)
	if absi(d.x) >= absi(d.y):
		return Vector2i(signi(d.x), 0)
	return Vector2i(0, signi(d.y))

# ==========================================
# ACTIONS
# ==========================================
const COSTE_OCULTARSE := 1

## Hide (1 PA) or stand up (free). Only with turns (stealth or combat)
func ocultarse(h: Node) -> bool:
	if h == null:
		return false
	if Estados.tiene(h, "oculto"):
		Estados.quitar(h, "oculto")
		_texto(h, "Se levanta", COLOR_SIGILO)
		return true
	if not Modo.con_turnos():
		_texto(h, "Solo con turnos", Color(1.0, 0.5, 0.45))
		return false
	var pa := int(h.get_meta("pa_actual", 0))
	if pa < COSTE_OCULTARSE:
		_texto(h, "Faltan PA", Color(1.0, 0.5, 0.45))
		return false
	h.set_meta("pa_actual", pa - COSTE_OCULTARSE)
	Estados.aplicar(h, "oculto", 999, 1, h)
	_refrescar()
	return true

## Light / put out the torch (free): less light, they see you less (and you see less)
func antorcha(h: Node) -> void:
	if h == null:
		return
	var apagada := not bool(h.get_meta("antorcha_apagada", false))
	h.set_meta("antorcha_apagada", apagada)
	_texto(h, "Antorcha apagada" if apagada else "Antorcha encendida", Color(1.0, 0.75, 0.35))
	_refrescar()

## Tactica.al_mover: hidden, every tile costs 1 PA more. Returns true if they run out of PA (stops)
func paso_oculto(h: Node) -> bool:
	if not Tactica.es_heroe(h) or not Estados.tiene(h, "oculto") or not Modo.con_turnos():
		return false
	var dados := int(h.get_meta("_pasos_mov", 0)) + 1
	h.set_meta("_pasos_mov", dados)
	var pa := int(h.get_meta("pa_actual", 0)) - 1
	h.set_meta("pa_actual", maxi(0, pa))
	return pa - dados <= 0 # C++ charges the walked tiles at the end

# ==========================================
# EVERY FRAME
# ==========================================
func _process(delta: float) -> void:
	var dm := _dm()
	var pm := _pm()
	if dm == null or pm == null:
		en_sigilo = false
		dia_visto = -1
		_conos = null
		return
	var semilla := int(dm.get_meta("semilla_piso", 0))
	if semilla != semilla_vista:
		semilla_vista = semilla
		if en_sigilo:
			en_sigilo = false
			cambio.emit(false)
		_hp.clear()
		_pos.clear()
		_cono_de.clear()
		_precalentar(dm)
	_asignar_miradas(dm)
	# Has combat started? If we came from stealth and nobody had seen you, it's a SURPRISE
	var combate: bool = Modo.en_combate
	if combate != combate_visto:
		combate_visto = combate
		if combate:
			_al_empezar_combate()
	# Stealth rounds
	var dia := int(pm.get_meta("turno_actual", 1))
	if dia_visto < 0:
		dia_visto = dia
	elif dia != dia_visto:
		dia_visto = dia
		if en_sigilo and not combate:
			_vigilar()
	if not dm.get_meta("is_moving", false):
		for h in _heroes():
			if h.has_meta("_pasos_mov"):
				h.remove_meta("_pasos_mov")
	_seguir_en_combate()
	_girar_sprites(dm)
	_t_conos -= delta
	if _t_conos <= 0.0:
		_t_conos = 0.2
		_pintar_conos(dm)

# Combat started: stealth ends, and if nobody had seen you it's a surprise
func _al_empezar_combate() -> void:
	var de_sigilo := en_sigilo
	if en_sigilo:
		en_sigilo = false
		cambio.emit(false)
	for h in _heroes():
		Estados.quitar(h, "oculto")
	if not de_sigilo:
		return
	var me_vieron := false
	for e in Modo.despiertos():
		if e.get_meta("me_ha_visto", false):
			me_vieron = true
	if me_vieron:
		return
	# One of your hits started it: the ones that hadn't seen you lose their first turn
	var n := 0
	for e in Modo.despiertos():
		Estados.aplicar(e, "sorprendido", 1, 1, null)
		n += 1
	if n > 0:
		_hud("¡Sorpresa! Pierden su primer turno", COLOR_SIGILO)

# Every enemy: if it walks, it faces where it goes; if it gets hit, the closest hero. And the
# "unaware" mark (read by the roll of the hit that wakes it up, before it gets updated)
func _seguir_en_combate() -> void:
	for e in _enemigos():
		var id: int = e.get_instance_id()
		var p: Vector2i = e.get_meta("grid_pos")
		var hp := int(e.get_meta("hp_actual", 0))
		e.set_meta("desprevenido", not e.get_meta("in_combat", false))
		if _pos.has(id) and _pos[id] != p:
			e.set_meta("mirada", _hacia(_pos[id], p))
		if _hp.has(id) and hp < int(_hp[id]) and e.get_meta("in_combat", false):
			var cerca: Node = null
			for h in _heroes():
				if cerca == null or Tactica.dist(Tactica.pos(h), p) < Tactica.dist(Tactica.pos(cerca), p):
					cerca = h
			if cerca:
				e.set_meta("mirada", _hacia(p, Tactica.pos(cerca)))
		_pos[id] = p
		_hp[id] = hp

# ==========================================
# WHO EACH ONE IS AND WHERE IT LOOKS
# ==========================================
func _asignar_miradas(dm: Node) -> void:
	var con_centinela := {}
	for e in _enemigos():
		if e.has_meta("vigilancia"):
			if tipo(e) == CENTINELA:
				con_centinela[int(e.get_meta("sala", -1))] = true
	var inicio: Vector2i = dm.get_meta("casilla_inicio", Vector2i.ZERO)
	for e in _enemigos():
		if e.has_meta("vigilancia"):
			continue
		var p: Vector2i = e.get_meta("grid_pos")
		var sala := int(e.get_meta("sala", -1))
		var t := VIGIA
		var f: Vector2i = DIRS[randi() % DIRS.size()]
		if e.has_meta("jefe"):
			# The boss guards its door
			t = CENTINELA
			var puertas: Array = dm.get_meta("puertas_jefe", [])
			f = _hacia(p, puertas[0] if not puertas.is_empty() else inicio)
		elif sala >= 0 and not con_centinela.has(sala):
			# The first one in every camp watches where the heroes come from
			t = CENTINELA
			f = _hacia(p, inicio)
			con_centinela[sala] = true
		elif sala >= 0:
			var r := randf()
			t = DORMIDO if r < 0.3 else (VIGIA if r < 0.8 else CENTINELA)
			if t == CENTINELA:
				f = _hacia(p, inicio)
		e.set_meta("vigilancia", t)
		e.set_meta("mirada", f)

# The sprite faces its side of the screen; the sleeping ones carry "Zzz"
func _girar_sprites(dm: Node) -> void:
	var cam := dm.get_node_or_null("Camera3D") as Camera3D
	var derecha: Vector3 = cam.global_transform.basis.x if cam else Vector3.RIGHT
	for e in _enemigos():
		if not (e is Node3D) or not e.visible:
			continue
		var cuerpo := e.get_node_or_null("Cuerpo") as Sprite3D
		var f := mirada(e)
		var x := Vector3(f.x, 0, f.y).dot(derecha)
		if cuerpo and absf(x) > 0.15:
			cuerpo.flip_h = x < 0.0
		var zzz := e.get_node_or_null("Zzz") as Label3D
		var dormido: bool = tipo(e) == DORMIDO and not e.get_meta("in_combat", false)
		if dormido and zzz == null:
			zzz = Label3D.new()
			zzz.name = "Zzz"
			zzz.text = "Zzz"
			zzz.font_size = 40
			zzz.outline_size = 10
			zzz.pixel_size = 0.004
			zzz.modulate = Color(0.75, 0.82, 1.0)
			zzz.billboard = BaseMaterial3D.BILLBOARD_ENABLED
			zzz.no_depth_test = true
			zzz.position = Vector3(0.2, 1.05, 0)
			e.add_child(zzz)
		elif not dormido and zzz:
			zzz.queue_free()

# ==========================================
# THE LIGHT CONES ON THE FLOOR
# ==========================================
# Every enemy that hasn't seen you casts a soft light cone (Shaders/cono_vista.gdshader) where it
# looks: a fan of RAYOS_CONO rays cut by the walls (it only lights the floor: it doesn't reveal
# enemies or chests). Lookouts also carry, very faint, the cone of their next turn.
const SHADER_CONO := preload("res://Shaders/cono_vista.gdshader")
const RAYOS_CONO := 28
const PASO_RAYO := 0.12
const INTENSIDAD_CONO := 0.26
const INTENSIDAD_FANTASMA := 0.08

var _material_cono: ShaderMaterial
var _cono_de := {} # Enemy instance_id -> {firma, nodos}

# One material shared by every cone (each one sets its own instance uniforms)
func _material() -> ShaderMaterial:
	if _material_cono == null:
		_material_cono = ShaderMaterial.new()
		_material_cono.shader = SHADER_CONO
	return _material_cono

# Only the cone of the enemy that changed is rebuilt (tile, facing, reach, type or the map)
func _pintar_conos(dm: Node) -> void:
	if _conos == null or not is_instance_valid(_conos):
		_conos = Node3D.new()
		_conos.name = "ConosSigilo"
		dm.add_child(_conos)
		_cono_de.clear()
	var version := int(dm.get_meta("version_mapa", 0))
	var vistos := {}
	for e in vigilantes():
		if tipo(e) == DORMIDO:
			continue
		var id: int = e.get_instance_id()
		vistos[id] = true
		var firma := str([e.get_meta("grid_pos"), mirada(e), tipo(e), alcance(e), version])
		var previo: Dictionary = _cono_de.get(id, {})
		if previo.get("firma", "") == firma:
			continue
		for n in previo.get("nodos", []):
			if is_instance_valid(n):
				n.queue_free()
		var semilla := float(id % 997) * 0.37
		var nodos := [_cono(e, mirada(e), INTENSIDAD_CONO, semilla)]
		if tipo(e) == VIGIA:
			nodos.append(_cono(e, _siguiente(mirada(e)), INTENSIDAD_FANTASMA, semilla + PI))
		_cono_de[id] = {"firma": firma, "nodos": nodos}
	for id in _cono_de.keys():
		if not vistos.has(id):
			for n in _cono_de[id]["nodos"]:
				if is_instance_valid(n):
					n.queue_free()
			_cono_de.erase(id)

# One cone: a fan of triangles from the enemy; each ray reaches the first wall
func _cono(e: Node, f: Vector2i, intensidad: float, semilla: float) -> MeshInstance3D:
	var p: Vector2i = e.get_meta("grid_pos")
	var radio := float(alcance(e)) + 0.5
	var origen := Vector2(p) + Vector2(0.5, 0.5)
	var y := Tactica.mundo(p).y + 0.04
	var base := atan2(float(f.y), float(f.x))
	var largos := _rayos(e, f)
	var puntas := []   # [point, distance]
	for i in RAYOS_CONO + 1:
		var a := base + lerpf(-PI * 0.25, PI * 0.25, float(i) / RAYOS_CONO)
		puntas.append([origen + Vector2(cos(a), sin(a)) * largos[i], largos[i]])
	var st := SurfaceTool.new()
	st.begin(Mesh.PRIMITIVE_TRIANGLES)
	for i in RAYOS_CONO:
		var t0 := float(i) / RAYOS_CONO * 2.0 - 1.0
		var t1 := float(i + 1) / RAYOS_CONO * 2.0 - 1.0
		var a: Array = puntas[i]
		var b: Array = puntas[i + 1]
		st.set_uv(Vector2(0.0, (t0 + t1) * 0.5))
		st.add_vertex(Vector3(origen.x, y, origen.y))
		st.set_uv(Vector2(a[1] / radio, t0))
		st.add_vertex(Vector3(a[0].x, y, a[0].y))
		st.set_uv(Vector2(b[1] / radio, t1))
		st.add_vertex(Vector3(b[0].x, y, b[0].y))
	var mi := MeshInstance3D.new()
	mi.mesh = st.commit()
	mi.cast_shadow = GeometryInstance3D.SHADOW_CASTING_SETTING_OFF
	mi.material_override = _material()
	mi.set_instance_shader_parameter("intensidad", intensidad)
	mi.set_instance_shader_parameter("semilla", semilla)
	_conos.add_child(mi)
	return mi

# The cone stops at what blocks sight (walls, columns, closed doors), not at chests or braziers
func _tapa(c: Vector2i) -> bool:
	var l := Tactica.lado()
	if c.x < 0 or c.y < 0 or c.x >= l or c.y >= l:
		return true
	return (Tactica.mapa_fijo()[c.x * l + c.y] & 2) != 0

# When a floor starts: an invisible cone and "Zzz" for a moment, so Godot compiles their shaders
# then and not mid-game (the first cone caused a hitch)
func _precalentar(dm: Node) -> void:
	var cam := dm.get_node_or_null("Camera3D") as Camera3D
	if cam == null:
		return
	var delante := cam.global_position - cam.global_transform.basis.z * 3.0
	var q := QuadMesh.new()
	q.size = Vector2(0.1, 0.1)
	var mi := MeshInstance3D.new()
	mi.mesh = q
	mi.material_override = _material()
	mi.set_instance_shader_parameter("intensidad", 0.0)
	dm.add_child(mi)
	mi.global_position = delante
	var z := Label3D.new()
	z.text = "Zzz"
	z.font_size = 40
	z.outline_size = 10
	z.pixel_size = 0.004
	z.modulate = Color(1, 1, 1, 0.01)
	z.billboard = BaseMaterial3D.BILLBOARD_ENABLED
	z.no_depth_test = true
	dm.add_child(z)
	z.global_position = delante
	await get_tree().create_timer(0.3).timeout
	if is_instance_valid(mi):
		mi.queue_free()
	if is_instance_valid(z):
		z.queue_free()

# ==========================================
func _refrescar() -> void:
	var dm := _dm()
	if dm and not dm.get_meta("is_moving", false):
		dm.call("RefrescarLuces")
	var ui := get_node_or_null("/root/Main/CanvasLayer")
	if ui:
		ui.call("UpdateWarriorUI")

# Writes a line in the HUD log
func _hud(texto: String, color: Color) -> void:
	var hud := get_tree().get_first_node_in_group("hud")
	if hud:
		hud.registrar(texto, color)
	print("[Sigilo] ", texto)

# Floating text over a character ("!", "Zzz"...)
func _texto(n: Node, t: String, color: Color) -> void:
	var f := Tactica.ficha(n)
	var esp := get_node_or_null("/root/Especiales")
	if f and esp:
		esp.texto(f.position + Vector3(0, 0.5, 0), t, color, 26)
