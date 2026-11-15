extends Node

# Tactica (autoload): positioning. Reusable pieces used by abilities, passives, events, C++ and
# whatever comes next:
#
#   GEOMETRY     unidades(), unidad_en(c), pos(n), dist(a,b), dir_hacia(a,b), nivel(c), solido(c),
#                es_heroe(n), hostiles_adyacentes(n), mundo(c), ficha(n)
#   ROLLS        modificadores(atacante, defensor) -> {ventaja, bono, razones}  (used by the C++ d20)
#                  advantage: flanking, height, knocked down/stunned · disadvantage: from below, cover
#                  bonus: an ally's help, a nearby banner
#                tras_atacar(atacante): spends whatever lasts "one roll" (the help)
#   MOVE         colocar(n, c) · empujar(n, dir, casillas, fuente) · atraer(n, hacia, casillas, fuente)
#                intercambiar(a, b) · danar(n, cantidad, fuente)
#                Collisions are solved the same way for everything: wall/column (damage + stunned),
#                another unit (damage to both), falling from a high room (damage + knocked down).
#   HAZARDS      crear_peligro(c, tipo, dias, fuente) (fire on the floor...), al_entrar(n, c)
#   EVERY STEP   al_mover(n, desde, hasta): attacks of opportunity, bleeding, hazards. Called by
#                the DungeonManager (heroes) and the EnemyManager (enemies). true = it should stop.
#   BODYGUARD    protector_de(heroe): an adjacent ally with the "guardaespaldas" tag

const RUTA_DM := "/root/Main/ContenedorMapa/SubViewport/DungeonManager"
const RUTA_PM := "/root/Main/PartyManager"
const DIRS4 := [Vector2i(1, 0), Vector2i(-1, 0), Vector2i(0, 1), Vector2i(0, -1)]
const ALTO := 2
const DANO_PARED := 3
const DANO_CHOQUE := 2
const DANO_CAIDA := 3
const T_EMPUJE := 0.16 # Seconds per pushed tile

# Floor hazards: Vector2i -> {tipo, dias, fuente, nodo}
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
# GEOMETRY AND WHO IS WHERE
# ==========================================
func es_heroe(n: Node) -> bool:
	return n != null and is_instance_valid(n) and not str(n.name).begins_with("Enemy_")

# Alive and on the map (heroes) or alive (enemies)
func vivo(n: Node) -> bool:
	if n == null or not is_instance_valid(n):
		return false
	if es_heroe(n):
		return not n.get_meta("esta_muerto", false) and not n.get_meta("ha_escapado", false) and n.has_meta("grid_pos")
	return not str(n.name).ends_with("_muerto") and int(n.get_meta("hp_actual", 1)) > 0

## Heroes on the map and living enemies. Computed once per frame (lots of scripts ask for it):
## enemies come from the DungeonManager list, not by walking every node on the map.
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
	# Someone may have died or escaped this very frame
	var out := []
	for u in _cache:
		if vivo(u):
			out.append(u)
	return out

# Grid tile of a unit
func pos(n: Node) -> Vector2i:
	return n.get_meta("grid_pos", Vector2i(-99, -99)) if n else Vector2i(-99, -99)

# The unit standing on that tile (ignoring 'excepto'), or null
func unidad_en(c: Vector2i, excepto: Node = null) -> Node:
	for u in unidades():
		if u != excepto and pos(u) == c:
			return u
	return null

# The 3D token of a unit (enemies are their own token, heroes have one on the map)
func ficha(n: Node) -> Node3D:
	if n == null or not is_instance_valid(n):
		return null
	if n is Node3D:
		return n
	var dm := _dm()
	return dm.get_node_or_null(NodePath(str(n.name))) as Node3D if dm else null

# Manhattan distance in tiles
func dist(a: Vector2i, b: Vector2i) -> int:
	return absi(a.x - b.x) + absi(a.y - b.y)

## Cardinal direction from a to b (along the longest axis)
func dir_hacia(a: Vector2i, b: Vector2i) -> Vector2i:
	var d := b - a
	if d == Vector2i.ZERO:
		return Vector2i.ZERO
	if absi(d.x) >= absi(d.y):
		return Vector2i(signi(d.x), 0)
	return Vector2i(0, signi(d.y))

# Grid size of the current floor
func lado() -> int:
	return int(Engine.get_meta("alturas_lado", 40))

# Height level of a tile (0 low floor, 1 step, 2 high room)
func nivel(c: Vector2i) -> int:
	if not Engine.has_meta("alturas"):
		return 0
	var l := lado()
	if c.x < 0 or c.y < 0 or c.x >= l or c.y >= l:
		return 0
	var a: PackedByteArray = Engine.get_meta("alturas")
	var i := c.x * l + c.y
	return int(a[i]) if i < a.size() else 0

# World position of a tile's centre, at its floor height
func mundo(c: Vector2i) -> Vector3:
	return Vector3(c.x + 0.5, nivel(c) * 0.2, c.y + 0.5)

# ==========================================
# FIXED MAP (without calling C++ every time: every GDScript -> C++ call costs ~0.8 ms)
# ==========================================
# Whatever doesn't move (walls, doors, chests, braziers, stairs) is asked from C++ once
# (ObtenerMapaFijo) and asked again only when the meta "version_mapa" goes up (new floor, door
# opened). Bit 1 = can't be crossed; bit 2 = blocks sight.
var _fijo := PackedByteArray()
var _version_fijo := -1
var _dm_fijo: Node = null
var _frame_fijo := -1

# CAREFUL: reading a meta from the DungeonManager (node with a C++ script) also costs ~0.13 ms, so
# the version is checked only once per frame
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

## Wall, column, closed door, chest, brazier, stairs or off the map (units don't count)
func solido(c: Vector2i) -> bool:
	var l := lado()
	if c.x < 0 or c.y < 0 or c.x >= l or c.y >= l:
		return true
	var m := mapa_fijo()
	if m.size() < l * l:
		return true
	return (m[c.x * l + c.y] & 1) != 0

# Bit 2 of the fixed map: blocks sight
func _tapa_vista(x: int, y: int, m: PackedByteArray, l: int) -> bool:
	return (m[x * l + y] & 2) != 0

## Line of sight between two tiles: the SAME rule as C++ (Mazmorra::CheckLineOfSight, Bresenham with
## strict corners), but without calling it. The origin tile doesn't block; the destination is seen
## even if it's a wall
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

# 0 = heroes, 1 = enemies
func bando(n: Node) -> int:
	return 0 if es_heroe(n) else 1

# Units of the other side right next to 'n'
func hostiles_adyacentes(n: Node) -> Array:
	var out := []
	for u in unidades():
		if bando(u) != bando(n) and dist(pos(u), pos(n)) == 1:
			out.append(u)
	return out

# ==========================================
# ROLLS: advantage, disadvantage and bonuses
# ==========================================
## Used by the d20 (C++ Reglas::TiradaAtaque, the preview and the abilities). ventaja: -1, 0 or 1
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
	# Flanking: the defender has two or more of the attacker's side right next to them
	if cuerpo:
		var cerca := 0
		for u in unidades():
			if bando(u) == bando(a) and dist(pos(u), pd) == 1:
				cerca += 1
		if cerca >= 2:
			hay_v = true
			razones.append("Flanqueo")
	# Height
	if nivel(pa) >= ALTO and nivel(pd) < ALTO:
		hay_v = true
		razones.append("Altura")
	elif nivel(pa) < ALTO and nivel(pd) >= ALTO:
		hay_d = true
		razones.append("Desde abajo")
	# Cover: at range, a wall or column next to the defender on the attacker's side
	if not cuerpo:
		for dd in DIRS4:
			var c: Vector2i = pd + dd
			var hacia := Vector2(pa - pd)
			if Vector2(dd).dot(hacia) > 0.0 and solido(c):
				hay_d = true
				razones.append("Cobertura")
				break
	# States
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
	# From behind (Sigilo.gd: where it's looking): if it hadn't seen you, BACKSTAB (guaranteed
	# critical)
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
	# Bonuses: an ally's help and nearby banners
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

## After rolling an attack: whatever lasts a single roll is spent
func tras_atacar(a: Node) -> void:
	Estados.quitar(a, "ayudado")

# ==========================================
# UNIFIED DAMAGE
# ==========================================
## Damage to anyone: a hero (PartyManager) or an enemy (if it dies, XP and loot for 'fuente')
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
# MOVE: place, push, pull, swap
# ==========================================
## Puts 'n' on tile c (without checking whether it's free: the caller decides that). animar: seconds
## of the movement (0 = instantly)
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

## Pushes 'n' along 'dir' up to 'casillas'. Collisions: wall (damage + stunned), unit (damage to
## both), falling from a high room (damage + knocked down). Returns how many tiles it moved.
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
	# Effects at the end of the push
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

## Pulls 'n' towards 'hacia' (stops right next to it)
func atraer(n: Node, hacia: Node, casillas: int, fuente: Node = null) -> int:
	if not vivo(n) or not vivo(hacia):
		return 0
	var d := dist(pos(n), pos(hacia))
	return empujar(n, dir_hacia(pos(n), pos(hacia)), mini(casillas, d - 1), fuente)

## Swaps two units (hero-hero, hero-enemy...)
func intercambiar(a: Node, b: Node) -> void:
	if not vivo(a) or not vivo(b):
		return
	var pa := pos(a)
	var pb := pos(b)
	# Enemies move their mark on the map: first the one that doesn't step on anyone
	if not es_heroe(b):
		colocar(b, pa)
		colocar(a, pb)
	else:
		colocar(a, pb)
		colocar(b, pa)

# ==========================================
# FLOOR HAZARDS
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

# Hazard type on that tile ("" if none)
func peligro_en(c: Vector2i) -> String:
	return str(peligros[c]["tipo"]) if peligros.has(c) else ""

# Someone steps into a hazard: it hurts (and may apply a state)
func al_entrar(n: Node, c: Vector2i) -> void:
	if not peligros.has(c) or not vivo(n):
		return
	var p: Dictionary = peligros[c]
	var def: Dictionary = PELIGROS[p["tipo"]]
	var pm := _pm()
	var fuente: Node = pm.get_node_or_null(NodePath(str(p["fuente"]))) if pm and str(p["fuente"]) != "" else null
	Estados.aplicar(n, def["estado"], def["dias_estado"], 1, fuente)

# New floor: the old hazards go away
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
		# Whoever wakes up on top burns again; the fire slowly dies out
		for c in peligros.keys():
			var u := unidad_en(c)
			if u:
				al_entrar(u, c)
			peligros[c]["dias"] = int(peligros[c]["dias"]) - 1
			if int(peligros[c]["dias"]) <= 0:
				if is_instance_valid(peligros[c]["nodo"]):
					peligros[c]["nodo"].queue_free()
				peligros.erase(c)
	# The fire flickers
	var t := Time.get_ticks_msec() / 1000.0
	for c in peligros:
		var m: MeshInstance3D = peligros[c]["nodo"]
		if is_instance_valid(m):
			(m.material_override as StandardMaterial3D).albedo_color.a = 0.42 + 0.16 * sin(t * 7.0 + c.x * 1.3 + c.y)

# ==========================================
# EVERY STEP: attacks of opportunity, bleeding and hazards
# ==========================================
## n moves from 'desde' to 'hasta' (one tile). true = it must stop (it fell or it's stunned)
func al_mover(n: Node, desde: Vector2i, hasta: Vector2i) -> bool:
	if not vivo(n):
		return true
	if es_heroe(n):
		Modo.contar_paso(n) # Exploring: time passes by walking
		Modo.paso_lider(n, desde, hasta) # Exploring: the single file moves one step behind them
		if Sigilo.paso_oculto(n): # Hidden: every tile costs one more PA
			return true
	var dm := _dm()
	var combate := dm.get_node_or_null("CombatManager") if dm else null
	# Attacks of opportunity: every (awake) enemy that stops having them next to it gets a free hit
	# (one per day and attacker). Retreat or the "escurridizo" tag avoid them.
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
	# Bleeding: every tile hurts
	var sangre := Estados.dano_por_paso(n)
	if sangre > 0:
		danar(n, sangre)
	al_entrar(n, hasta)
	return not vivo(n) or Estados.tiene(n, "aturdido")

# ==========================================
# BODYGUARD
# ==========================================
## An ally next to the hero with the "guardaespaldas" tag takes the hit for them
func protector_de(h: Node) -> Node:
	if not es_heroe(h):
		return null
	for u in unidades():
		if u != h and es_heroe(u) and dist(pos(u), pos(h)) == 1 and Progreso.tiene_etiqueta(u, "guardaespaldas") and not Estados.tiene(u, "aturdido"):
			_texto(u, "¡Te cubro!", Color(0.6, 0.85, 1.0))
			return u
	return null

# Floating text over a unit
func _texto(n: Node, t: String, color: Color) -> void:
	var f := ficha(n)
	var esp := get_node_or_null("/root/Especiales")
	if f and esp:
		esp.texto(f.position + Vector3(0, 0.45, 0), t, color, 26)
