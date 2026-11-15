extends Node

# Tactica (autoload): posicionamiento. Piezas reutilizables que usan las habilidades, las pasivas,
# los eventos, el C++ y lo que venga:
#
#   GEOMETRÍA    unidades(), unidad_en(c), pos(n), dist(a,b), dir_hacia(a,b), nivel(c), solido(c),
#                es_heroe(n), hostiles_adyacentes(n), mundo(c), ficha(n)
#   TIRADAS      modificadores(atacante, defensor) -> {ventaja, bono, razones}  (lo usa el d20 del C++)
#                  ventaja: flanqueo, altura, derribado/aturdido · desventaja: desde abajo, cobertura
#                  bono: ayuda de un aliado, estandarte cercano
#                tras_atacar(atacante): gasta lo que dura "una tirada" (la ayuda)
#   MOVER        colocar(n, c) · empujar(n, dir, casillas, fuente) · atraer(n, hacia, casillas, fuente)
#                intercambiar(a, b) · danar(n, cantidad, fuente)
#                Los choques se resuelven igual para todo: pared/columna (daño + aturdido),
#                otra unidad (daño a las dos), caída de una sala alta (daño + derribado), peligros.
#   PELIGROS     crear_peligro(c, tipo, dias, fuente) (fuego en el suelo...), al_entrar(n, c)
#   CADA PASO    al_mover(n, desde, hasta): ataques de oportunidad, sangrado, peligros. Lo llaman el
#                DungeonManager (héroes) y el EnemyManager (enemigos). true = que se pare.
#   GUARDAESPALDAS protector_de(heroe): un aliado pegado con la etiqueta "guardaespaldas"

const RUTA_DM := "/root/Main/ContenedorMapa/SubViewport/DungeonManager"
const RUTA_PM := "/root/Main/PartyManager"
const DIRS4 := [Vector2i(1, 0), Vector2i(-1, 0), Vector2i(0, 1), Vector2i(0, -1)]
const ALTO := 2
const DANO_PARED := 3
const DANO_CHOQUE := 2
const DANO_CAIDA := 3
const T_EMPUJE := 0.16 # segundos por casilla empujada

# Peligros del suelo: Vector2i -> {tipo, dias, fuente, nodo}
const PELIGROS := {
	"fuego": {"estado": "ardiendo", "dias_estado": 2, "color": Color(1.0, 0.45, 0.12)},
}
var peligros := {}
var piso_visto := -1
var dia_visto := -1

func _dm() -> Node:
	return get_node_or_null(RUTA_DM)

func _pm() -> Node:
	return get_node_or_null(RUTA_PM)

# ==========================================
# GEOMETRÍA Y QUIÉN ESTÁ DÓNDE
# ==========================================
func es_heroe(n: Node) -> bool:
	return n != null and is_instance_valid(n) and not str(n.name).begins_with("Enemy_")

func vivo(n: Node) -> bool:
	if n == null or not is_instance_valid(n):
		return false
	if es_heroe(n):
		return not n.get_meta("esta_muerto", false) and not n.get_meta("ha_escapado", false) and n.has_meta("grid_pos")
	return not str(n.name).ends_with("_muerto") and int(n.get_meta("hp_actual", 1)) > 0

## Héroes en el mapa y enemigos vivos. Se calcula una vez por frame (lo piden muchos scripts):
## los enemigos salen de la lista del DungeonManager, no recorriendo todos los nodos del mapa.
var _cache_frame := -1
var _cache: Array = []

func unidades() -> Array:
	var frame := Engine.get_process_frames()
	if frame != _cache_frame:
		_cache_frame = frame
		_cache = []
		var pm := _pm()
		if pm:
			for h in pm.get_children():
				if h.has_meta("grid_pos") and h.has_meta("hp_actual"):
					_cache.append(h)
		var dm := _dm()
		if dm:
			for p in dm.get_meta("lista_enemigos", []):
				var e := dm.get_node_or_null("Enemy_%d_%d" % [p.x, p.y])
				if e:
					_cache.append(e)
	# Alguien puede haber muerto o escapado en este mismo frame
	var out := []
	for u in _cache:
		if vivo(u):
			out.append(u)
	return out

func pos(n: Node) -> Vector2i:
	return n.get_meta("grid_pos", Vector2i(-99, -99)) if n else Vector2i(-99, -99)

func unidad_en(c: Vector2i, excepto: Node = null) -> Node:
	for u in unidades():
		if u != excepto and pos(u) == c:
			return u
	return null

func ficha(n: Node) -> Node3D:
	if n == null or not is_instance_valid(n):
		return null
	if n is Node3D:
		return n
	var dm := _dm()
	return dm.get_node_or_null(NodePath(str(n.name))) as Node3D if dm else null

func dist(a: Vector2i, b: Vector2i) -> int:
	return absi(a.x - b.x) + absi(a.y - b.y)

## Dirección cardinal de a hacia b (por el eje más largo)
func dir_hacia(a: Vector2i, b: Vector2i) -> Vector2i:
	var d := b - a
	if d == Vector2i.ZERO:
		return Vector2i.ZERO
	if absi(d.x) >= absi(d.y):
		return Vector2i(signi(d.x), 0)
	return Vector2i(0, signi(d.y))

func lado() -> int:
	return int(Engine.get_meta("alturas_lado", 40))

func nivel(c: Vector2i) -> int:
	if not Engine.has_meta("alturas"):
		return 0
	var l := lado()
	if c.x < 0 or c.y < 0 or c.x >= l or c.y >= l:
		return 0
	var a: PackedByteArray = Engine.get_meta("alturas")
	var i := c.x * l + c.y
	return int(a[i]) if i < a.size() else 0

func mundo(c: Vector2i) -> Vector3:
	return Vector3(c.x + 0.5, nivel(c) * 0.2, c.y + 0.5)

# ==========================================
# MAPA FIJO (sin llamar al C++ cada vez: cada llamada de GDScript al C++ cuesta ~0.8 ms)
# ==========================================
# Lo que no se mueve (muros, puertas, cofres, braseros, escalera) se pide una vez al C++
# (ObtenerMapaFijo) y se vuelve a pedir solo cuando sube la meta "version_mapa" (piso nuevo,
# puerta abierta). Bit 1 = no se pasa; bit 2 = tapa la vista.
var _fijo := PackedByteArray()
var _version_fijo := -1
var _dm_fijo: Node = null
var _frame_fijo := -1

# OJO: leer una meta del DungeonManager (nodo con script C++) también cuesta ~0.13 ms, así que la
# versión se mira una sola vez por frame
func mapa_fijo() -> PackedByteArray:
	var frame := Engine.get_process_frames()
	if frame == _frame_fijo and not _fijo.is_empty():
		return _fijo
	_frame_fijo = frame
	var dm := _dm()
	if dm == null:
		return PackedByteArray()
	var v := int(dm.get_meta("version_mapa", 0))
	if v != _version_fijo or dm != _dm_fijo or _fijo.is_empty():
		_fijo = dm.call("ObtenerMapaFijo")
		_version_fijo = v
		_dm_fijo = dm
	return _fijo

## Pared, columna, puerta cerrada, cofre, brasero, escalera o fuera del mapa (no cuenta a las unidades)
func solido(c: Vector2i) -> bool:
	var l := lado()
	if c.x < 0 or c.y < 0 or c.x >= l or c.y >= l:
		return true
	var m := mapa_fijo()
	if m.size() < l * l:
		return true
	return (m[c.x * l + c.y] & 1) != 0

func _tapa_vista(x: int, y: int, m: PackedByteArray, l: int) -> bool:
	return (m[x * l + y] & 2) != 0

## Línea de visión entre dos casillas: la MISMA regla que el C++ (Mazmorra::CheckLineOfSight, Bresenham
## con esquinas estrictas), pero sin llamarlo. La casilla de origen no tapa; la de destino se ve
## aunque sea un muro
func linea_de_vision(a: Vector2i, b: Vector2i) -> bool:
	var l := lado()
	var m := mapa_fijo()
	if m.size() < l * l:
		return false
	if a.x < 0 or a.y < 0 or a.x >= l or a.y >= l or b.x < 0 or b.y < 0 or b.x >= l or b.y >= l:
		return false
	var x0 := a.x
	var y0 := a.y
	var dx := absi(b.x - x0)
	var sx := 1 if x0 < b.x else -1
	var dy := -absi(b.y - y0)
	var sy := 1 if y0 < b.y else -1
	var err := dx + dy
	while true:
		if x0 < 0 or y0 < 0 or x0 >= l or y0 >= l:
			return false
		if _tapa_vista(x0, y0, m, l) and not (x0 == a.x and y0 == a.y):
			return x0 == b.x and y0 == b.y
		if x0 == b.x and y0 == b.y:
			break
		var e2 := 2 * err
		var px := x0
		var py := y0
		if e2 >= dy:
			err += dy
			x0 += sx
		if e2 <= dx:
			err += dx
			y0 += sy
		if x0 != px and y0 != py:
			var muro1 := x0 >= 0 and x0 < l and py >= 0 and py < l and _tapa_vista(x0, py, m, l)
			var muro2 := px >= 0 and px < l and y0 >= 0 and y0 < l and _tapa_vista(px, y0, m, l)
			if muro1 or muro2:
				return false
	return true

func bando(n: Node) -> int:
	return 0 if es_heroe(n) else 1

func hostiles_adyacentes(n: Node) -> Array:
	var out := []
	for u in unidades():
		if bando(u) != bando(n) and dist(pos(u), pos(n)) == 1:
			out.append(u)
	return out

# ==========================================
# TIRADAS: ventaja, desventaja y bonos
# ==========================================
## Lo usa el d20 (C++ Reglas::TiradaAtaque, la previsión y las habilidades). ventaja: -1, 0 o 1
func modificadores(a: Node, d: Node) -> Dictionary:
	var razones := PackedStringArray()
	var hay_v := false
	var hay_d := false
	var bono := 0
	if a == null or d == null or not is_instance_valid(a) or not is_instance_valid(d):
		return {"ventaja": 0, "bono": 0, "razones": razones}
	var pa := pos(a)
	var pd := pos(d)
	var cuerpo := dist(pa, pd) <= 1
	# Flanqueo: el defensor tiene pegados a dos o más del bando del atacante
	if cuerpo:
		var cerca := 0
		for u in unidades():
			if bando(u) == bando(a) and dist(pos(u), pd) == 1:
				cerca += 1
		if cerca >= 2:
			hay_v = true
			razones.append("Flanqueo")
	# Altura
	if nivel(pa) >= ALTO and nivel(pd) < ALTO:
		hay_v = true
		razones.append("Altura")
	elif nivel(pa) < ALTO and nivel(pd) >= ALTO:
		hay_d = true
		razones.append("Desde abajo")
	# Cobertura: a distancia, una pared o columna pegada al defensor por el lado del atacante
	if not cuerpo:
		for dd in DIRS4:
			var c: Vector2i = pd + dd
			var hacia := Vector2(pa - pd)
			if Vector2(dd).dot(hacia) > 0.0 and solido(c):
				hay_d = true
				razones.append("Cobertura")
				break
	# Estados
	if Estados.tiene(d, "derribado"):
		if cuerpo:
			hay_v = true
			razones.append("Derribado")
		else:
			hay_d = true
			razones.append("Tumbado (lejos)")
	if Estados.tiene(d, "aturdido"):
		hay_v = true
		razones.append("Aturdido")
	if Estados.tiene(d, "sorprendido"):
		hay_v = true
		razones.append("Sorprendido")
	# Por la espalda (Sigilo.gd: hacia dónde mira): si no os había visto, PUÑALADA (crítico seguro)
	var critico := false
	if Sigilo.por_la_espalda(a, d):
		if Sigilo.desprevenido(d):
			critico = true
			razones.append("Puñalada")
		else:
			hay_v = true
			razones.append("Por la espalda")
	if Estados.tiene(a, "derribado"):
		hay_d = true
		razones.append("Desde el suelo")
	# Bonos: ayuda de un aliado y estandartes cercanos
	if Estados.tiene(a, "ayudado"):
		var p := Estados.potencia(a, "ayudado")
		bono += p
		razones.append("Ayuda +%d" % p)
	if es_heroe(a):
		for u in unidades():
			if u != a and es_heroe(u) and dist(pos(u), pa) <= 2 and Progreso.tiene_etiqueta(u, "estandarte"):
				bono += 1
				razones.append("Estandarte +1")
				break
	var v := (1 if hay_v else 0) - (1 if hay_d else 0)
	return {"ventaja": v, "bono": bono, "razones": razones, "critico": critico}

## Tras tirar un ataque: lo que dura una sola tirada se gasta
func tras_atacar(a: Node) -> void:
	Estados.quitar(a, "ayudado")

# ==========================================
# DAÑO UNIFICADO
# ==========================================
## Daño a cualquiera: un héroe (PartyManager) o un enemigo (si muere, XP y botín para 'fuente')
func danar(n: Node, cantidad: int, fuente: Node = null) -> void:
	if not vivo(n) or cantidad <= 0:
		return
	if es_heroe(n):
		var pm := _pm()
		if pm:
			pm.call("RecibirDano", cantidad, n)
	else:
		Progreso.golpear_enemigo(fuente if fuente and es_heroe(fuente) else null, n, cantidad)

# ==========================================
# MOVER: colocar, empujar, atraer, intercambiar
# ==========================================
## Pone a 'n' en la casilla c (sin comprobar si está libre: eso lo decide quien llama).
## animar: segundos del desplazamiento (0 = de golpe)
func colocar(n: Node, c: Vector2i, animar: float = 0.18) -> void:
	if not vivo(n):
		return
	var dm := _dm()
	var desde := pos(n)
	if desde == c:
		return
	if es_heroe(n):
		dm.call("ColocarHeroe", str(n.name), c.x, c.y, 1)
	else:
		dm.call("MoverEnemigo", desde.x, desde.y, c.x, c.y)
	var f := ficha(n)
	if f:
		if animar > 0.0:
			var tw := f.create_tween()
			tw.tween_property(f, "position", mundo(c), animar).set_trans(Tween.TRANS_QUAD).set_ease(Tween.EASE_OUT)
		else:
			f.position = mundo(c)
	al_entrar(n, c)

## Empuja 'n' en 'dir' hasta 'casillas'. Choques: pared (daño + aturdido), unidad (daño a las dos),
## caída de una sala alta (daño + derribado). Devuelve cuántas casillas se ha movido.
func empujar(n: Node, dir: Vector2i, casillas: int, fuente: Node = null) -> int:
	if not vivo(n) or dir == Vector2i.ZERO or casillas <= 0:
		return 0
	if Progreso.tiene_etiqueta(n, "inamovible"):
		_texto(n, "¡Ni se mueve!", Color(0.8, 0.8, 0.85))
		return 0
	var actual := pos(n)
	var movidas := 0
	var choque := ""
	var otro: Node = null
	var cae := false
	for i in casillas:
		var sig := actual + dir
		otro = unidad_en(sig, n)
		if otro:
			choque = "unidad"
			break
		if solido(sig):
			choque = "pared"
			break
		cae = nivel(actual) >= ALTO and nivel(sig) < ALTO and nivel(sig) == 0
		actual = sig
		movidas += 1
		if cae:
			break
	if movidas > 0:
		colocar(n, actual, T_EMPUJE * movidas)
	# Efectos del final del empujón
	match choque:
		"pared":
			_texto(n, "¡Pum!", Color(1, 0.8, 0.5))
			danar(n, DANO_PARED, fuente)
			Estados.aplicar(n, "aturdido", 1, 1, fuente)
		"unidad":
			_texto(n, "¡Choque!", Color(1, 0.8, 0.5))
			danar(n, DANO_CHOQUE, fuente)
			danar(otro, DANO_CHOQUE, fuente)
	if cae:
		_texto(n, "¡Cae!", Color(1, 0.8, 0.5))
		danar(n, DANO_CAIDA, fuente)
		Estados.aplicar(n, "derribado", 1, 1, fuente)
	var feel := get_node_or_null("/root/Feel")
	if feel and (choque != "" or cae):
		feel.sacudir(0.25)
	return movidas

## Trae 'n' hacia 'hacia' (se para pegado a él)
func atraer(n: Node, hacia: Node, casillas: int, fuente: Node = null) -> int:
	if not vivo(n) or not vivo(hacia):
		return 0
	var d := dist(pos(n), pos(hacia))
	return empujar(n, dir_hacia(pos(n), pos(hacia)), mini(casillas, d - 1), fuente)

## Cambia de sitio a dos unidades (héroe-héroe, héroe-enemigo...)
func intercambiar(a: Node, b: Node) -> void:
	if not vivo(a) or not vivo(b):
		return
	var pa := pos(a)
	var pb := pos(b)
	# Los enemigos mueven su marca en el mapa: primero el que no pisa a nadie
	if not es_heroe(b):
		colocar(b, pa)
		colocar(a, pb)
	else:
		colocar(a, pb)
		colocar(b, pa)

# ==========================================
# PELIGROS DEL SUELO
# ==========================================
func crear_peligro(c: Vector2i, tipo: String, dias: int, fuente: Node = null) -> void:
	if not PELIGROS.has(tipo) or solido(c):
		return
	var dm := _dm()
	if dm == null:
		return
	if peligros.has(c) and is_instance_valid(peligros[c]["nodo"]):
		peligros[c]["nodo"].queue_free()
	var m := MeshInstance3D.new()
	var plano := PlaneMesh.new()
	plano.size = Vector2(0.86, 0.86)
	m.mesh = plano
	var mat := StandardMaterial3D.new()
	mat.shading_mode = BaseMaterial3D.SHADING_MODE_UNSHADED
	mat.transparency = BaseMaterial3D.TRANSPARENCY_ALPHA
	mat.albedo_color = Color(PELIGROS[tipo]["color"], 0.55)
	m.material_override = mat
	m.position = mundo(c) + Vector3(0, 0.02, 0)
	m.name = "Peligro_%s_%d_%d" % [tipo, c.x, c.y]
	dm.add_child(m)
	peligros[c] = {"tipo": tipo, "dias": dias, "fuente": str(fuente.name) if fuente else "", "nodo": m}
	var u := unidad_en(c)
	if u:
		al_entrar(u, c)

func peligro_en(c: Vector2i) -> String:
	return str(peligros[c]["tipo"]) if peligros.has(c) else ""

func al_entrar(n: Node, c: Vector2i) -> void:
	if not peligros.has(c) or not vivo(n):
		return
	var p: Dictionary = peligros[c]
	var def: Dictionary = PELIGROS[p["tipo"]]
	var pm := _pm()
	var fuente: Node = pm.get_node_or_null(NodePath(str(p["fuente"]))) if pm and str(p["fuente"]) != "" else null
	Estados.aplicar(n, def["estado"], def["dias_estado"], 1, fuente)

func _limpiar_peligros() -> void:
	for c in peligros:
		if is_instance_valid(peligros[c]["nodo"]):
			peligros[c]["nodo"].queue_free()
	peligros.clear()

func _process(_delta: float) -> void:
	var pm := _pm()
	if pm == null:
		peligros.clear()
		piso_visto = -1
		dia_visto = -1
		return
	var piso := int(pm.get_meta("piso_actual", 1))
	if piso != piso_visto:
		piso_visto = piso
		_limpiar_peligros()
	var dia := int(pm.get_meta("turno_actual", 1))
	if dia_visto < 0:
		dia_visto = dia
	elif dia != dia_visto:
		dia_visto = dia
		# Quien amanece encima se quema otra vez; el fuego se va apagando
		for c in peligros.keys():
			var u := unidad_en(c)
			if u:
				al_entrar(u, c)
			peligros[c]["dias"] = int(peligros[c]["dias"]) - 1
			if int(peligros[c]["dias"]) <= 0:
				if is_instance_valid(peligros[c]["nodo"]):
					peligros[c]["nodo"].queue_free()
				peligros.erase(c)
	# El fuego parpadea
	var t := Time.get_ticks_msec() / 1000.0
	for c in peligros:
		var m: MeshInstance3D = peligros[c]["nodo"]
		if is_instance_valid(m):
			(m.material_override as StandardMaterial3D).albedo_color.a = 0.42 + 0.16 * sin(t * 7.0 + c.x * 1.3 + c.y)

# ==========================================
# CADA PASO: ataques de oportunidad, sangrado y peligros
# ==========================================
## n se mueve de 'desde' a 'hasta' (una casilla). true = debe pararse (ha caído o está aturdido)
func al_mover(n: Node, desde: Vector2i, hasta: Vector2i) -> bool:
	if not vivo(n):
		return true
	if es_heroe(n):
		Modo.contar_paso(n) # explorando, el tiempo pasa andando
		Modo.paso_lider(n, desde, hasta) # explorando, la fila india avanza un paso tras él
		if Sigilo.paso_oculto(n): # oculto, cada casilla cuesta un PA más
			return true
	var dm := _dm()
	var combate := dm.get_node_or_null("CombatManager") if dm else null
	# Ataques de oportunidad: cada enemigo (despierto) que deja de tenerlo pegado, un golpe gratis
	# (uno por día y atacante). La Retirada o la etiqueta "escurridizo" los evitan.
	if combate and not Estados.tiene(n, "desenganchado") and not Progreso.tiene_etiqueta(n, "escurridizo"):
		var dia := int(_pm().get_meta("turno_actual", 1)) if _pm() else 0
		for u in unidades():
			if bando(u) == bando(n) or dist(pos(u), desde) != 1 or dist(pos(u), hasta) <= 1:
				continue
			if not es_heroe(u) and not u.get_meta("in_combat", false):
				continue
			if Estados.tiene(u, "aturdido") or int(u.get_meta("reaccion_dia", -1)) == dia:
				continue
			u.set_meta("reaccion_dia", dia)
			_texto(u, "¡Oportunidad!", Color(1, 0.7, 0.4))
			combate.call("AtaqueLibre", u, n, 1)
			if not vivo(n):
				return true
	# Sangrado: cada casilla duele
	var sangre := Estados.dano_por_paso(n)
	if sangre > 0:
		danar(n, sangre)
	al_entrar(n, hasta)
	return not vivo(n) or Estados.tiene(n, "aturdido")

# ==========================================
# GUARDAESPALDAS
# ==========================================
## Un aliado pegado al héroe con la etiqueta "guardaespaldas" recibe el golpe por él
func protector_de(h: Node) -> Node:
	if not es_heroe(h):
		return null
	for u in unidades():
		if u != h and es_heroe(u) and dist(pos(u), pos(h)) == 1 and Progreso.tiene_etiqueta(u, "guardaespaldas") and not Estados.tiene(u, "aturdido"):
			_texto(u, "¡Te cubro!", Color(0.6, 0.85, 1.0))
			return u
	return null

func _texto(n: Node, t: String, color: Color) -> void:
	var f := ficha(n)
	var esp := get_node_or_null("/root/Especiales")
	if f and esp:
		esp.texto(f.position + Vector3(0, 0.45, 0), t, color, 26)
