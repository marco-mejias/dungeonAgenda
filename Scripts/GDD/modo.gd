extends Node

# Modo (autoload): exploring, stealth or combat.
#   (STEALTH is handled by Sigilo.gd: there are turns like in combat but the enemies only keep
#    watch. con_turnos() = combat or stealth: PA, end of turn, no single file.)
#
#   EXPLORING    (no squad awake) Walking costs no PA and there are no turns: you can go to any
#                explored tile, faster. The rest walk behind in SINGLE FILE along the leader's
#                trail: on every step of theirs, each follower moves one tile (to where the one
#                ahead was). The day goes by every PASOS_POR_DIA walked tiles, with no enemy turn.
#   THE LINE     Who follows the leader is fixed (it isn't recomputed on every click): a hero joins
#                the line when they're DIST_GRUPO tiles or less from the leader or from someone in
#                the line, and they don't drop out even if they fall behind. They only leave if you
#                move them away (Ctrl+click moves the leader alone); they join again once the leader
#                has left and comes back close. The ones far away don't join on their own.
#   COMBAT       Enemies come in SQUADS (the ones that spawn together). If one wakes up, its whole
#                squad wakes up: surprise animation (the camera focuses on them, they jump with a
#                "!") and combat goes on until the last one falls. The whole party plays in turns
#                with PA, near or far. pasar_lejanos() ends the turn of the ones more than
#                DIST_LEJOS tiles away.
#   WHEN IT ENDS The ones who were fighting (DIST_LEJOS or less from an awake enemy) regroup in
#                line behind the leader; the ones far away stay where they are.
#
# It publishes it in the DungeonManager meta "modo_exploracion" (read by C++ and the HUD).

const RUTA_DM := "/root/Main/ContenedorMapa/SubViewport/DungeonManager"
const RUTA_PM := "/root/Main/PartyManager"
const PASOS_POR_DIA := 30
const DIST_LEJOS := 8
const DIST_PELOTON := 4 # Enemies within this (chained) distance when spawning = same squad
const DIST_GRUPO := 3   # To join the line
const T_REUNION := 0.14 # Seconds per step when regrouping after a fight

signal cambio_de_modo(combate: bool)

var en_combate := false
var pasos := 0
var xp_al_entrar := {}       # hero -> total XP when combat started (for the summary)
var pelotones_activos := {}  # Squad id -> true (awake until the last one falls)
var siguiente_peloton := 0
var piso_visto := -1
# Single file
var fila: Array = []         # Names of the ones following the leader, in order
var lider_visto := ""
var apartados := {}          # name -> has already moved away from the leader (can join again)
var paso_t := 0.15
var rastro: Array = []       # Leader's last tiles (most recent first)
var luchadores := {}         # Names of the ones who were in the fight (they regroup at the end)

func _dm() -> Node:
	return get_node_or_null(RUTA_DM)

func _pm() -> Node:
	return get_node_or_null(RUTA_PM)

func _process(_delta: float) -> void:
	var dm := _dm()
	if dm == null or _pm() == null:
		en_combate = false
		pasos = 0
		fila.clear()
		lider_visto = ""
		return
	var piso := int(_pm().get_meta("piso_actual", 1))
	if piso != piso_visto:
		piso_visto = piso
		pelotones_activos.clear()
		apartados.clear()
		lider_visto = "" # On the new floor the line is rebuilt
	_actualizar_pelotones()
	var combate := hay_combate()
	var explorar := not (combate or Sigilo.en_sigilo)
	if bool(dm.get_meta("modo_exploracion", explorar)) != explorar or not dm.has_meta("modo_exploracion"):
		dm.set_meta("modo_exploracion", explorar)
	if combate != en_combate:
		en_combate = combate
		if combate:
			_empezar_combate()
		else:
			_acabar_combate()
		cambio_de_modo.emit(combate)
	if en_combate:
		_marcar_luchadores()
	elif not Sigilo.en_sigilo:
		var lider = _pm().call("GetActiveCharacter")
		if lider and str(lider.name) != lider_visto:
			lider_visto = str(lider.name)
			_rehacer_fila(lider)

## Is any squad awake with someone alive (or is it the enemy turn)?
func hay_combate() -> bool:
	var dm := _dm()
	if dm and dm.get_meta("turno_enemigo", false):
		return true
	return not pelotones_activos.is_empty()

# No turns: free movement
func explorando() -> bool:
	return not con_turnos()

## Combat or stealth: you play in turns with PA
func con_turnos() -> bool:
	return en_combate or Sigilo.en_sigilo

# ==========================================
# SQUADS
# ==========================================
func _enemigos() -> Array:
	var out := []
	for u in Tactica.unidades():
		if not Tactica.es_heroe(u):
			out.append(u)
	return out

# The ones spawning together form a squad; if one wakes up, they all wake up and the squad stays up
# until the last one dies
func _actualizar_pelotones() -> void:
	var enemigos := _enemigos()
	var sin_peloton := []
	for e in enemigos:
		if not e.has_meta("peloton"):
			sin_peloton.append(e)
	while not sin_peloton.is_empty():
		var id := siguiente_peloton
		siguiente_peloton += 1
		var cola: Array = [sin_peloton.pop_back()]
		while not cola.is_empty():
			var e: Node = cola.pop_back()
			e.set_meta("peloton", id)
			for o in sin_peloton.duplicate():
				if Tactica.dist(e.get_meta("grid_pos"), o.get_meta("grid_pos")) <= DIST_PELOTON:
					sin_peloton.erase(o)
					cola.append(o)
	var miembros := {}
	for e in enemigos:
		var id: int = e.get_meta("peloton")
		if not miembros.has(id):
			miembros[id] = []
		miembros[id].append(e)
		if e.get_meta("in_combat", false):
			pelotones_activos[id] = true
	for id in pelotones_activos.keys():
		if not miembros.has(id):
			pelotones_activos.erase(id) # The last one fell
			continue
		for e in miembros[id]:
			if not e.get_meta("in_combat", false):
				e.set_meta("in_combat", true) # Wakes up with its mates

## Awake enemies (from active squads)
func despiertos() -> Array:
	var out := []
	for e in _enemigos():
		if pelotones_activos.has(e.get_meta("peloton", -1)):
			out.append(e)
	return out

# ==========================================
# COMBAT START: surprise
# ==========================================
func _empezar_combate() -> void:
	pasos = 0
	luchadores.clear()
	xp_al_entrar.clear()
	for h in _heroes():
		h.set_meta("pa_actual", int(h.get_meta("pa_max", h.get_meta("velocidad", 4))))
		xp_al_entrar[str(h.name)] = _xp_total(h)
	_marcar_luchadores()
	var dm := _dm()
	var banner := get_node_or_null("/root/Main/CanvasLayer/Banner")
	# If combat was started by an attack (a sequence is already running), only the banner
	if dm.get_meta("is_moving", false):
		if banner:
			banner.mostrar("¡Combate!")
		_refrescar()
		return
	_animacion_sorpresa()

# The camera focuses on the squad, each one jumps with a "!" facing the heroes, and the banner
func _animacion_sorpresa() -> void:
	var dm := _dm()
	var cam := dm.get_node_or_null("Camera3D")
	var vistos := []
	for e in despiertos():
		if e.visible:
			vistos.append(e)
	dm.set_meta("is_moving", true) # Nobody moves meanwhile
	var puntos := PackedVector3Array()
	for e in vistos:
		puntos.append(e.position)
	if cam and not puntos.is_empty():
		cam.set_meta("encuadre", puntos)
	var audio := get_node_or_null("/root/AudioManager")
	if audio:
		audio.play_sfx("fail")
	await get_tree().create_timer(0.4).timeout
	var esp := get_node_or_null("/root/Especiales")
	var lider = _pm().call("GetActiveCharacter") if _pm() else null
	for e in vistos:
		if not is_instance_valid(e):
			continue
		var cuerpo := e.get_node_or_null("Cuerpo") as Sprite3D
		if cuerpo:
			# It turns to face the hero that surprised it
			var fl := Tactica.ficha(lider) if lider else null
			if fl and cam:
				var derecha: Vector3 = cam.global_transform.basis.x
				cuerpo.flip_h = (fl.position - e.position).dot(derecha) < 0.0
			var y0: float = cuerpo.get_meta("y_base", cuerpo.position.y)
			var tw := cuerpo.create_tween()
			tw.tween_property(cuerpo, "position:y", y0 + 0.32, 0.12).set_trans(Tween.TRANS_QUAD).set_ease(Tween.EASE_OUT)
			tw.tween_property(cuerpo, "position:y", y0, 0.14).set_trans(Tween.TRANS_QUAD).set_ease(Tween.EASE_IN)
		if esp:
			esp.texto(e.position + Vector3(0, 0.55, 0), "!", Color(1.0, 0.85, 0.25), 64)
	await get_tree().create_timer(0.75).timeout
	var banner := get_node_or_null("/root/Main/CanvasLayer/Banner")
	if banner:
		banner.mostrar("¡Combate!")
	if cam and cam.has_meta("encuadre") and cam.get_meta("encuadre") == puntos:
		cam.remove_meta("encuadre")
	dm.set_meta("is_moving", false)
	_refrescar()

# Remembers who was fighting (close to an awake enemy), to regroup them at the end
func _marcar_luchadores() -> void:
	var en_pie := despiertos()
	for h in _heroes():
		for e in en_pie:
			if Tactica.dist(h.get_meta("grid_pos"), e.get_meta("grid_pos")) <= DIST_LEJOS:
				luchadores[str(h.name)] = true
				break

# ==========================================
# COMBAT END: the ones fighting regroup
# ==========================================
func _acabar_combate() -> void:
	var ganado := 0
	for h in _heroes():
		h.set_meta("pa_actual", int(h.get_meta("pa_max", h.get_meta("velocidad", 4))))
		ganado += maxi(0, _xp_total(h) - int(xp_al_entrar.get(str(h.name), _xp_total(h))))
	var banner := get_node_or_null("/root/Main/CanvasLayer/Banner")
	if banner:
		banner.mostrar("Fin del combate" + ("  ·  +%d XP" % ganado if ganado > 0 else ""), 0.85, false)
	_reunir()

# After combat the fighters walk back into line behind the leader
func _reunir() -> void:
	var dm := _dm()
	var lider = _pm().call("GetActiveCharacter")
	if lider == null or dm == null:
		return
	lider_visto = str(lider.name)
	var lp: Vector2i = lider.get_meta("grid_pos")
	var reunir := []
	for h in _heroes():
		if h != lider and luchadores.has(str(h.name)):
			reunir.append(h)
	reunir.sort_custom(func(a, b): return Tactica.dist(a.get_meta("grid_pos"), lp) < Tactica.dist(b.get_meta("grid_pos"), lp))
	luchadores.clear()
	fila.clear()
	rastro.clear()
	apartados.clear()
	if reunir.is_empty():
		_refrescar()
		return
	# Free spots around the leader, from closest to furthest (where you can walk)
	var sitios := _casillas_cerca(lp, reunir.size(), reunir)
	var path_mgr := dm.get_node_or_null("PathManager")
	var mas_larga := 0
	dm.set_meta("is_moving", true)
	for i in reunir.size():
		var h: Node = reunir[i]
		fila.append(str(h.name))
		if i >= sitios.size() or path_mgr == null:
			continue
		var desde: Vector2i = h.get_meta("grid_pos")
		var ruta: Array = path_mgr.call("CalcularRuta", desde.x, desde.y, sitios[i].x, sitios[i].y)
		if ruta.size() < 2:
			continue
		mas_larga = maxi(mas_larga, ruta.size() - 1)
		_andar(h, ruta.slice(1), T_REUNION)
	await get_tree().create_timer(mas_larga * T_REUNION + 0.15).timeout
	dm.set_meta("is_moving", false)
	dm.call("ColocarHeroe", str(lider.name), lp.x, lp.y, 1) # Vision recomputed with everyone together
	_refrescar()

# BFS from the leader over free floor: the 'n' closest tiles to stand on
func _casillas_cerca(desde: Vector2i, n: int, que_se_mueven: Array) -> Array:
	var dm := _dm()
	var libres: PackedByteArray = dm.call("ObtenerTransitables")
	var lado := Tactica.lado()
	var quietos := {}
	for h in _heroes():
		if not h in que_se_mueven:
			quietos[h.get_meta("grid_pos")] = true
	var visto := {desde: true}
	var cola := [desde]
	var out := []
	while not cola.is_empty() and out.size() < n:
		var c: Vector2i = cola.pop_front()
		for d in Tactica.DIRS4:
			var s: Vector2i = c + d
			if visto.has(s) or s.x < 0 or s.y < 0 or s.x >= lado or s.y >= lado:
				continue
			visto[s] = true
			# Free or with one of the ones about to move (they'll step aside)
			var ocupada_por_quien_se_mueve := false
			for h in que_se_mueven:
				if h.get_meta("grid_pos") == s:
					ocupada_por_quien_se_mueve = true
			if (libres[s.x * lado + s.y] == 1 or ocupada_por_quien_se_mueve) and not quietos.has(s):
				out.append(s)
				cola.append(s)
	return out

# ==========================================
# THE SINGLE FILE
# ==========================================
func _heroe(nombre: String) -> Node:
	var pm := _pm()
	var h = pm.get_node_or_null(NodePath(nombre)) if pm else null
	return h if h and Tactica.vivo(h) else null

# When the leader changes: the line is the ones close to them (chained), from closest to furthest
func _rehacer_fila(lider: Node) -> void:
	fila.clear()
	rastro.clear()
	var lp: Vector2i = lider.get_meta("grid_pos")
	var dentro := [lider]
	var cambio := true
	while cambio:
		cambio = false
		for h in _heroes():
			if h in dentro or apartados.has(str(h.name)):
				continue
			for d in dentro:
				if Tactica.dist(h.get_meta("grid_pos"), d.get_meta("grid_pos")) <= DIST_GRUPO:
					dentro.append(h)
					cambio = true
					break
	dentro.erase(lider)
	dentro.sort_custom(func(a, b): return Tactica.dist(a.get_meta("grid_pos"), lp) < Tactica.dist(b.get_meta("grid_pos"), lp))
	for h in dentro:
		fila.append(str(h.name))

## PathManager: the leader starts walking (exploring). With Ctrl, they go alone: the line stays
## apart.
func lider_se_mueve(_ruta: Array, t: float) -> void:
	paso_t = t
	if con_turnos():
		return
	if Input.is_key_pressed(KEY_CTRL):
		for n in fila:
			apartados[n] = false # Hasn't moved away from the leader yet
		fila.clear()
		rastro.clear()

## Tactica.al_mover: hero 'h' takes a step from 'desde' to 'hasta'. If it's the leader while
## exploring, the line moves one tile: each one to where the one ahead was.
func paso_lider(h: Node, desde: Vector2i, hasta: Vector2i) -> void:
	if con_turnos() or _pm() == null or h != _pm().call("GetActiveCharacter"):
		return
	_unirse_a_la_fila(h, hasta)
	# The leader's trail: the last tiles they stepped on (the most recent first). Follower i goes to
	# rastro[i]: contiguous tiles, so they stay glued one behind the other.
	rastro.push_front(desde)
	while rastro.size() > fila.size() + 1:
		rastro.pop_back()
	var dm := _dm()
	var path_mgr := dm.get_node_or_null("PathManager")
	var ocupadas := {hasta: true}
	var i := 0
	for nombre in fila.duplicate():
		var f := _heroe(nombre)
		if f == null:
			fila.erase(nombre)
			continue
		var objetivo: Vector2i = rastro[mini(i, rastro.size() - 1)]
		i += 1
		var antes: Vector2i = f.get_meta("grid_pos")
		var pasos_f := []
		if antes == objetivo:
			pasos_f = []
		elif Tactica.dist(antes, objetivo) == 1:
			pasos_f = [objetivo]
		elif path_mgr:
			# It fell behind (just joined, or something cut it off): up to two steps, faster
			var ruta: Array = path_mgr.call("CalcularRuta", antes.x, antes.y, objetivo.x, objetivo.y)
			for k in range(1, mini(3, ruta.size())):
				pasos_f.append(ruta[k])
		# Don't end where the leader or someone else in the line is going on this step
		while not pasos_f.is_empty() and ocupadas.has(pasos_f[pasos_f.size() - 1]):
			pasos_f.pop_back()
		if pasos_f.is_empty():
			ocupadas[antes] = true
		else:
			_andar(f, pasos_f, paso_t / float(pasos_f.size()))
			ocupadas[pasos_f[pasos_f.size() - 1]] = true

# The ones close to the leader or to someone in the line join it (the ones set apart with Ctrl, only
# after the leader has moved away from them and comes back)
func _unirse_a_la_fila(lider: Node, lider_en: Vector2i) -> void:
	for h in _heroes():
		var n := str(h.name)
		if h == lider or n in fila:
			continue
		var hp: Vector2i = h.get_meta("grid_pos")
		var cerca := Tactica.dist(hp, lider_en) <= DIST_GRUPO
		if not cerca:
			for m in fila:
				var f := _heroe(m)
				if f and Tactica.dist(hp, f.get_meta("grid_pos")) <= DIST_GRUPO:
					cerca = true
					break
		if apartados.has(n):
			if not apartados[n]:
				if Tactica.dist(hp, lider_en) > DIST_GRUPO:
					apartados[n] = true # Already moved away: the next time it passes close, it joins
				continue
			if not cerca:
				continue
			apartados.erase(n)
		if cerca:
			fila.append(n)

# Walks tile by tile along 'pasos' (with a little hop and facing where it goes)
func _andar(h: Node, pasos_ruta: Array, t: float) -> void:
	var ficha := Tactica.ficha(h)
	var dm := _dm()
	if ficha == null or pasos_ruta.is_empty():
		return
	var nombre := str(h.name)
	var cuerpo := ficha.get_node_or_null("Cuerpo") as Sprite3D
	var cam := dm.get_node_or_null("Camera3D") as Camera3D
	var derecha: Vector3 = cam.global_transform.basis.x if cam else Vector3.RIGHT
	var y0: float = cuerpo.get_meta("y_base", cuerpo.position.y) if cuerpo else 0.0
	var tw := ficha.create_tween()
	var previa: Vector2i = h.get_meta("grid_pos")
	for i in pasos_ruta.size():
		var c: Vector2i = pasos_ruta[i]
		var ultimo := i == pasos_ruta.size() - 1
		if cuerpo:
			var dx := Vector3(c.x - previa.x, 0, c.y - previa.y).dot(derecha)
			if absf(dx) > 0.01:
				tw.tween_callback(func(): cuerpo.flip_h = dx < 0.0)
		tw.tween_property(ficha, "position", Tactica.mundo(c), t).set_trans(Tween.TRANS_LINEAR)
		if cuerpo:
			tw.parallel().tween_property(cuerpo, "position:y", y0 + 0.12, t * 0.5).set_trans(Tween.TRANS_SINE).set_ease(Tween.EASE_OUT)
			tw.parallel().tween_property(cuerpo, "position:y", y0, t * 0.5).set_delay(t * 0.5).set_trans(Tween.TRANS_SINE).set_ease(Tween.EASE_IN)
		# The tile is stored on arrival; vision (expensive) only on the last step
		tw.tween_callback(func(): dm.call("ColocarHeroe", nombre, c.x, c.y, 1 if ultimo else 0))
		previa = c
	# The destination tile is reserved right away (so nobody else picks it on this same step)
	h.set_meta("grid_pos", pasos_ruta[pasos_ruta.size() - 1])

## PathManager (exploring): is there an ally on that tile who WON'T step aside? (the ones in the
## line step aside when following the leader; with Ctrl held nobody does)
func aliado_quieto_en(x: int, y: int) -> bool:
	var pm := _pm()
	var lider = pm.call("GetActiveCharacter") if pm else null
	var c := Vector2i(x, y)
	var ctrl := Input.is_key_pressed(KEY_CTRL)
	for h in _heroes():
		if h != lider and h.get_meta("grid_pos") == c and (ctrl or not str(h.name) in fila):
			return true
	return false

## Groups for the HUD bar: the leader with their line, and the rest grouping the ones close together
func grupos_heroes() -> Array:
	var pm := _pm()
	var lider = pm.call("GetActiveCharacter") if pm else null
	var restantes := _heroes()
	var grupos := []
	if lider and lider in restantes:
		var g := [lider]
		restantes.erase(lider)
		for n in fila:
			var f := _heroe(n)
			if f and f in restantes:
				g.append(f)
				restantes.erase(f)
		grupos.append(g)
	while not restantes.is_empty():
		var grupo := [restantes.pop_front()]
		var i := 0
		while i < grupo.size():
			for o in restantes.duplicate():
				if Tactica.dist(grupo[i].get_meta("grid_pos"), o.get_meta("grid_pos")) <= DIST_GRUPO:
					restantes.erase(o)
					grupo.append(o)
			i += 1
		grupos.append(grupo)
	return grupos

# The group of a hero (the leader's line or the ones close to them)
func grupo_de(h: Node) -> Array:
	for g in grupos_heroes():
		if h in g:
			return g
	return [h]

# ==========================================
# TIME WHILE EXPLORING
# ==========================================
## Tactica.al_mover calls it on every hero step; only the leader's count
func contar_paso(h: Node) -> void:
	if con_turnos() or _pm() == null or h != _pm().call("GetActiveCharacter"):
		return
	pasos += 1
	if pasos >= PASOS_POR_DIA:
		pasos = 0
		_pm().call("PasarDiaExplorando")

## Rest (sun button while exploring): the day goes by without walking
func descansar() -> void:
	if con_turnos():
		return
	pasos = 0
	_pm().call("PasarDiaExplorando")

# ==========================================
# COMBAT: the far ones
# ==========================================
## Heroes with PA more than DIST_LEJOS tiles away from any awake enemy
func lejanos() -> Array:
	var out := []
	if not con_turnos():
		return out
	# In combat, far from the awake ones; in stealth, far from the ones on watch
	var en_pie := despiertos() if en_combate else Sigilo.acechados()
	for h in _heroes():
		if int(h.get_meta("pa_actual", 0)) <= 0:
			continue
		var cerca := 999
		for e in en_pie:
			cerca = mini(cerca, Tactica.dist(h.get_meta("grid_pos"), e.get_meta("grid_pos")))
		if cerca > DIST_LEJOS:
			out.append(h)
	return out

## Ends the turn of all the far ones (HUD button, L key)
func pasar_lejanos() -> void:
	var lista := lejanos()
	if lista.is_empty():
		return
	for h in lista:
		h.set_meta("pa_actual", 0)
	var act = _pm().call("GetActiveCharacter")
	if act in lista:
		_dm().call("PedirFinDeTurno")
	_refrescar()

# ==========================================
func _refrescar() -> void:
	var dm := _dm()
	if dm and not dm.get_meta("is_moving", false):
		dm.call("RefrescarLuces")
	var ui := get_node_or_null("/root/Main/CanvasLayer")
	if ui:
		ui.call("UpdateWarriorUI")

# Heroes on the map
func _heroes() -> Array:
	var out := []
	for u in Tactica.unidades():
		if Tactica.es_heroe(u):
			out.append(u)
	return out

# Total XP of a hero across all their levels (for the combat summary)
func _xp_total(h: Node) -> int:
	var total := int(h.get_meta("xp", 0))
	for n in range(1, int(h.get_meta("nivel", 1))):
		total += Progreso.xp_para(n)
	return total
