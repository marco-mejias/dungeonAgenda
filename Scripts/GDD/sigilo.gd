extends Node

# Sigilo (autoload): hacia dónde mira cada enemigo, quién os ve y el modo SIGILO.
#
#   MIRADA       Cada enemigo mira hacia una dirección (meta "mirada": una de DIRS) y es de un tipo
#                (meta "vigilancia"):
#                  - CENTINELA: no se gira nunca (mira a la entrada / a la puerta del jefe).
#                  - VIGÍA: gira 90° (sentido horario) al final de cada ronda. Su siguiente cono se ve
#                    en fantasma: se puede planear.
#                  - DORMIDO: sin cono; solo despierta si un héroe acaba pegado a él.
#                En combate miran hacia donde se mueven y hacia quien les pega.
#   VER          Binario: te ve si estás en su cono delantero (90°, su radio de detección, con línea
#                de visión) o pegado a su costado. Por la espalda, nunca. Un héroe OCULTO no se ve
#                pegado al costado si tiene una pared o columna al lado, ni al fondo del cono (últimas
#                2 casillas) con la antorcha apagada. Si te ve: combate al instante con su pelotón.
#   MODO SIGILO  Empieza cuando un héroe ve a un enemigo que no os ha visto. Hay turnos (PA) como
#                en combate, pero los enemigos solo VIGILAN: al empezar cada ronda los vigías giran
#                y se mira otra vez quién os ve (si os ven así, ellos juegan primero). Si pasa una
#                ronda sin ningún enemigo desprevenido a la vista, se vuelve a explorar.
#   ROMPERLO     Pegar a un enemigo desprevenido empieza el combate con SORPRESA (los pelotones que no
#                os habían visto pierden su primer turno). Por la espalda de uno desprevenido: PUÑALADA
#                (crítico seguro). Por la espalda en combate: ventaja.
#   ACCIONES     ocultarse (1 PA; andar oculto cuesta 2 PA por casilla) y antorcha (gratis).
#
# El C++ llama a: comprobar(heroe, x, y) en cada paso (0 nada, 1 empieza el sigilo, 2 te han visto)
# y despertar_quien_vea() antes del turno enemigo. Tactica.modificadores pregunta por_la_espalda().

const RUTA_DM := "/root/Main/ContenedorMapa/SubViewport/DungeonManager"
const RUTA_PM := "/root/Main/PartyManager"
const CENTINELA := "centinela"
const VIGIA := "vigia"
const DORMIDO := "dormido"
const DIRS := [Vector2i(0, -1), Vector2i(1, 0), Vector2i(0, 1), Vector2i(-1, 0)] # en sentido horario
const NOMBRES := {CENTINELA: "Centinela", VIGIA: "Vigía", DORMIDO: "Dormido"}
const COLOR_SIGILO := Color(0.74, 0.58, 1.0)
# Los héroes ven su luz + VISION_EXTRA (= Mazmorra.hpp); un enemigo solo ve a un héroe hasta
# VENTAJA_VISTA casillas menos que lo que ve ese héroe: siempre los ves tú antes
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
var _hp := {}    # instance_id -> vida (para girarse hacia quien le pega)
var _pos := {}   # instance_id -> casilla (para mirar hacia donde anda)

func _dm() -> Node:
	return get_node_or_null(RUTA_DM)

func _pm() -> Node:
	return get_node_or_null(RUTA_PM)

# ==========================================
# CONSULTAS
# ==========================================
func tipo(e: Node) -> String:
	return str(e.get_meta("vigilancia", CENTINELA))

func mirada(e: Node) -> Vector2i:
	return e.get_meta("mirada", Vector2i(1, 0))

func _enemigos() -> Array:
	var out := []
	for u in Tactica.unidades():
		if not Tactica.es_heroe(u):
			out.append(u)
	return out

func _heroes() -> Array:
	var out := []
	for u in Tactica.unidades():
		if Tactica.es_heroe(u):
			out.append(u)
	return out

## No os ha visto (o acaba de: la sorpresa le dura su primer turno)
func desprevenido(e: Node) -> bool:
	return e != null and is_instance_valid(e) and not Tactica.es_heroe(e) and (bool(e.get_meta("desprevenido", false)) or Estados.tiene(e, "sorprendido"))

## Enemigos que no os han visto y que los héroes ven ahora mismo (lo que empieza el sigilo)
func vigilantes() -> Array:
	var out := []
	for e in _enemigos():
		if not e.get_meta("in_combat", false) and e is Node3D and e.visible:
			out.append(e)
	return out

const DIST_SIGILO := 8 # el sigilo sigue mientras un desprevenido esté así de cerca y a la vista

## Desprevenidos cerca de algún héroe (a DIST_SIGILO o menos, con línea de visión), aunque no se
## vean por la luz: apagar la antorcha no te saca del sigilo
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

## ¿'a' está a la espalda de 'd' (y pegado)? Solo cuenta para quien tiene mirada (enemigos)
func por_la_espalda(a: Node, d: Node) -> bool:
	if a == null or d == null or not Tactica.es_heroe(a) or Tactica.es_heroe(d) or not d.has_meta("mirada"):
		return false
	var rel: Vector2i = Tactica.pos(a) - Tactica.pos(d)
	if Tactica.dist(Tactica.pos(a), Tactica.pos(d)) != 1:
		return false
	var f := mirada(d)
	return rel.x * f.x + rel.y * f.y < 0

func _linea(desde: Vector2i, hasta: Vector2i) -> bool:
	return Tactica.linea_de_vision(desde, hasta) # sin llamar al C++ (mismo algoritmo)

func _cobertura(c: Vector2i) -> bool:
	for d in DIRS:
		if Tactica.solido(c + d):
			return true
	return false

## Hasta dónde ve 'e': su radio de detección, pero nunca más allá de lo que ve el héroe que MENOS ve
## (con la antorcha encendida) menos VENTAJA_VISTA. Igual para todos los héroes: el cono que se
## dibuja es justo donde te ven, seas quien seas, y siempre los veis vosotros antes
func alcance(e: Node, _h: Node = null) -> int:
	return mini(int(e.get_meta("radio_deteccion", 6)), _luz_minima() + VISION_EXTRA - VENTAJA_VISTA)

# La luz del héroe que menos ve (una vez por frame: se pregunta muchísimo)
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

# EL CONO es una sola forma para todo: RAYOS_CONO+1 rayos en abanico (90°) desde el centro de la
# casilla del enemigo, cada uno hasta su alcance o hasta lo primero que tapa la vista. Con esos rayos
# se dibuja la luz Y se decide si te ve: te ve si el centro de tu casilla queda dentro de la luz.
var _rayos_de := {} # firma -> PackedFloat32Array (largo de cada rayo)

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

## ¿Está el centro de la casilla 'c' dentro de la luz del cono de 'e' mirando hacia 'f'?
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

## ¿Ve 'e' al héroe 'h' en la casilla 'c'? (binario)
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
		# Oculto y sin antorcha: no te ve al fondo de su luz (las 2 últimas casillas)
		var lejos := Vector2(c - p).length() > radio - 1.5
		return not (oculto and h.get_meta("antorcha_apagada", false) and lejos)
	var d := c - p
	var pegado := maxi(absi(d.x), absi(d.y)) <= 1
	if pegado and d.x * f.x + d.y * f.y >= 0:
		return not (oculto and _cobertura(c))
	return false

# ==========================================
# DETECCIÓN
# ==========================================
# Mira todos los enemigos desprevenidos contra todos los héroes ('mueve' está en 'en'). Los que os ven
# se despiertan (con un "!"): el Modo despierta a su pelotón y empieza el combate.
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

## C++ (DungeonManager, en cada paso de un héroe): 0 = nada, 1 = empieza el sigilo (se para), 2 = le han visto
func comprobar(act: Node, x: int, y: int) -> int:
	if _detectar(act, Vector2i(x, y)):
		if en_sigilo:
			_hud("¡Os han visto!", Color(1.0, 0.45, 0.35))
		return 2
	if not en_sigilo and not Modo.en_combate and not vigilantes().is_empty():
		_empezar()
		return 1
	return 0

## C++ (EnemyManager, antes del turno enemigo)
func despertar_quien_vea() -> bool:
	return _detectar()

# ==========================================
# EL MODO SIGILO
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

func _salir(motivo: String) -> void:
	en_sigilo = false
	for h in _heroes():
		Estados.quitar(h, "oculto")
	_hud(motivo, COLOR_SIGILO)
	cambio.emit(false)

# Al empezar cada ronda (día): los vigías giran y se mira otra vez quién os ve
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

# Os han visto al girarse: el combate empieza con su turno (después de la animación de sorpresa)
func _juegan_ellos_primero() -> void:
	await get_tree().create_timer(1.6).timeout
	var dm := _dm()
	if dm == null or dm.get_meta("turno_enemigo", false) or not Modo.en_combate:
		return
	var em := dm.get_node_or_null("EnemyManager")
	if em:
		em.call("IniciarTurnoEnemigos")

func _siguiente(f: Vector2i) -> Vector2i:
	return DIRS[(DIRS.find(f) + 1) % DIRS.size()]

func _hacia(desde: Vector2i, hasta: Vector2i) -> Vector2i:
	var d := hasta - desde
	if d == Vector2i.ZERO:
		return Vector2i(1, 0)
	if absi(d.x) >= absi(d.y):
		return Vector2i(signi(d.x), 0)
	return Vector2i(0, signi(d.y))

# ==========================================
# ACCIONES
# ==========================================
const COSTE_OCULTARSE := 1

## Ocultarse (1 PA) o levantarse (gratis). Solo con turnos (sigilo o combate)
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

## Encender / apagar la antorcha (gratis): menos luz, te ven menos (y ves menos)
func antorcha(h: Node) -> void:
	if h == null:
		return
	var apagada := not bool(h.get_meta("antorcha_apagada", false))
	h.set_meta("antorcha_apagada", apagada)
	_texto(h, "Antorcha apagada" if apagada else "Antorcha encendida", Color(1.0, 0.75, 0.35))
	_refrescar()

## Tactica.al_mover: oculto, cada casilla cuesta 1 PA más. Devuelve true si se queda sin PA (frena)
func paso_oculto(h: Node) -> bool:
	if not Tactica.es_heroe(h) or not Estados.tiene(h, "oculto") or not Modo.con_turnos():
		return false
	var dados := int(h.get_meta("_pasos_mov", 0)) + 1
	h.set_meta("_pasos_mov", dados)
	var pa := int(h.get_meta("pa_actual", 0)) - 1
	h.set_meta("pa_actual", maxi(0, pa))
	return pa - dados <= 0 # el C++ cobra al final las casillas andadas

# ==========================================
# CADA FRAME
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
	# ¿Ha empezado el combate? Si veníamos del sigilo y nadie os había visto, es una SORPRESA
	var combate: bool = Modo.en_combate
	if combate != combate_visto:
		combate_visto = combate
		if combate:
			_al_empezar_combate()
	# Rondas del sigilo
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
	# Lo ha empezado un golpe vuestro: los que no os habían visto pierden su primer turno
	var n := 0
	for e in Modo.despiertos():
		Estados.aplicar(e, "sorprendido", 1, 1, null)
		n += 1
	if n > 0:
		_hud("¡Sorpresa! Pierden su primer turno", COLOR_SIGILO)

# Cada enemigo: si anda, mira hacia donde va; si le pegan, hacia el héroe más cercano. Y la marca de
# "desprevenido" (la lee la tirada del golpe que lo despierta, antes de que se actualice)
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
# QUIÉN ES CADA UNO Y HACIA DÓNDE MIRA
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
			# El jefe vigila su puerta
			t = CENTINELA
			var puertas: Array = dm.get_meta("puertas_jefe", [])
			f = _hacia(p, puertas[0] if not puertas.is_empty() else inicio)
		elif sala >= 0 and not con_centinela.has(sala):
			# El primero de cada campamento vigila por donde vienen los héroes
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

# El sprite mira hacia su lado de la pantalla; los dormidos llevan "Zzz"
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
# LOS CONOS DE LUZ EN EL SUELO
# ==========================================
# Cada enemigo que no os ha visto proyecta un cono de luz suave (Shaders/cono_vista.gdshader) hacia
# donde mira: un abanico de RAYOS_CONO rayos que se cortan en las paredes (solo ilumina el suelo:
# no enseña enemigos ni cofres). Los vigías llevan además, muy tenue, el cono de su siguiente giro.
const SHADER_CONO := preload("res://Shaders/cono_vista.gdshader")
const RAYOS_CONO := 28
const PASO_RAYO := 0.12
const INTENSIDAD_CONO := 0.26
const INTENSIDAD_FANTASMA := 0.08

var _material_cono: ShaderMaterial
var _cono_de := {} # instance_id del enemigo -> {firma, nodos}

func _material() -> ShaderMaterial:
	if _material_cono == null:
		_material_cono = ShaderMaterial.new()
		_material_cono.shader = SHADER_CONO
	return _material_cono

# Solo se rehace el cono del enemigo que ha cambiado (casilla, mirada, alcance, tipo o el mapa)
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

# Un cono: abanico de triángulos desde el enemigo; cada rayo llega hasta la primera pared
func _cono(e: Node, f: Vector2i, intensidad: float, semilla: float) -> MeshInstance3D:
	var p: Vector2i = e.get_meta("grid_pos")
	var radio := float(alcance(e)) + 0.5
	var origen := Vector2(p) + Vector2(0.5, 0.5)
	var y := Tactica.mundo(p).y + 0.04
	var base := atan2(float(f.y), float(f.x))
	var largos := _rayos(e, f)
	var puntas := []   # [punto, distancia]
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

# El cono se para en lo que tapa la vista (muros, columnas, puertas cerradas), no en cofres o braseros
func _tapa(c: Vector2i) -> bool:
	var l := Tactica.lado()
	if c.x < 0 or c.y < 0 or c.x >= l or c.y >= l:
		return true
	return (Tactica.mapa_fijo()[c.x * l + c.y] & 2) != 0

# Al empezar un piso: un cono y un "Zzz" invisibles un momento, para que Godot compile sus shaders
# entonces y no a mitad de partida (el primer cono daba un tirón)
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

func _hud(texto: String, color: Color) -> void:
	var hud := get_tree().get_first_node_in_group("hud")
	if hud:
		hud.registrar(texto, color)
	print("[Sigilo] ", texto)

func _texto(n: Node, t: String, color: Color) -> void:
	var f := Tactica.ficha(n)
	var esp := get_node_or_null("/root/Especiales")
	if f and esp:
		esp.texto(f.position + Vector3(0, 0.5, 0), t, color, 26)
