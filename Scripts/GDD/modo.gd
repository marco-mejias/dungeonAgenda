extends Node

# Modo (autoload): exploración, sigilo o combate.
#   (SIGILO lo lleva Sigilo.gd: hay turnos como en combate pero los enemigos solo vigilan.
#    con_turnos() = combate o sigilo: PA, fin del turno, sin fila india.)
#
#   EXPLORACIÓN  (ningún pelotón despierto) Andar no gasta PA y no hay turnos: se va a cualquier
#                casilla explorada, más deprisa. El resto va detrás en FILA INDIA por el rastro del
#                líder: en cada paso suyo, cada seguidor avanza una casilla (a donde estaba el de
#                delante). El día pasa cada PASOS_POR_DIA casillas andadas, sin turno enemigo.
#   LA FILA      Quién sigue al líder es fijo (no se recalcula en cada clic): un héroe entra en la
#                fila al quedar a DIST_GRUPO casillas o menos del líder o de alguien de la fila, y ya
#                no se cae aunque se retrase. Solo sale si lo apartas tú (Ctrl+clic mueve al líder
#                solo); vuelve a unirse cuando el líder se haya ido y vuelva a pasar cerca.
#                Los que están lejos no se unen solos.
#   COMBATE      Los enemigos van en PELOTONES (los que aparecen juntos). Si uno despierta, despierta
#                todo su pelotón: animación de sorpresa (la cámara los enfoca, saltan con un "!") y
#                el combate sigue hasta que caiga el último. Todo el grupo juega por turnos con PA,
#                esté cerca o lejos. pasar_lejanos() acaba el turno de los que están a más de
#                DIST_LEJOS casillas.
#   AL ACABAR    Los que estaban luchando (a DIST_LEJOS o menos de un enemigo despierto) se reúnen
#                en fila detrás del líder; los que estaban lejos se quedan donde están.
#
# Lo publica en la meta "modo_exploracion" del DungeonManager (la leen el C++ y el HUD).

const RUTA_DM := "/root/Main/ContenedorMapa/SubViewport/DungeonManager"
const RUTA_PM := "/root/Main/PartyManager"
const PASOS_POR_DIA := 30
const DIST_LEJOS := 8
const DIST_PELOTON := 4 # enemigos a esta distancia (encadenada) al aparecer = mismo pelotón
const DIST_GRUPO := 3   # para entrar en la fila
const T_REUNION := 0.14 # segundos por paso al reunirse tras un combate

signal cambio_de_modo(combate: bool)

var en_combate := false
var pasos := 0
var xp_al_entrar := {}       # héroe -> XP total al empezar el combate (para el resumen)
var pelotones_activos := {}  # id de pelotón -> true (despierto hasta que caiga el último)
var siguiente_peloton := 0
var piso_visto := -1
# Fila india
var fila: Array = []         # nombres de los que siguen al líder, en orden
var lider_visto := ""
var apartados := {}          # nombre -> ya se ha alejado del líder (puede volver a unirse)
var paso_t := 0.15
var rastro: Array = []       # últimas casillas del líder (la más reciente primero)
var luchadores := {}         # nombres de los que han estado en la pelea (se reúnen al acabar)

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
		lider_visto = "" # en el piso nuevo la fila se rehace
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

## ¿Algún pelotón despierto con alguien vivo (o es el turno enemigo)?
func hay_combate() -> bool:
	var dm := _dm()
	if dm and dm.get_meta("turno_enemigo", false):
		return true
	return not pelotones_activos.is_empty()

func explorando() -> bool:
	return not con_turnos()

## Combate o sigilo: se juega por turnos con PA
func con_turnos() -> bool:
	return en_combate or Sigilo.en_sigilo

# ==========================================
# PELOTONES
# ==========================================
func _enemigos() -> Array:
	var out := []
	for u in Tactica.unidades():
		if not Tactica.es_heroe(u):
			out.append(u)
	return out

# Los que aparecen juntos forman un pelotón; si uno despierta, despiertan todos y el pelotón sigue
# en pie hasta que muera el último
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
			pelotones_activos.erase(id) # cayó el último
			continue
		for e in miembros[id]:
			if not e.get_meta("in_combat", false):
				e.set_meta("in_combat", true) # despierta con los suyos

## Enemigos despiertos (de pelotones activos)
func despiertos() -> Array:
	var out := []
	for e in _enemigos():
		if pelotones_activos.has(e.get_meta("peloton", -1)):
			out.append(e)
	return out

# ==========================================
# INICIO DEL COMBATE: sorpresa
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
	# Si el combate lo ha empezado un ataque (ya hay secuencia en marcha), solo el cartel
	if dm.get_meta("is_moving", false):
		if banner:
			banner.mostrar("¡Combate!")
		_refrescar()
		return
	_animacion_sorpresa()

# La cámara enfoca al pelotón, cada uno da un salto con un "!" mirando a los héroes, y el cartel
func _animacion_sorpresa() -> void:
	var dm := _dm()
	var cam := dm.get_node_or_null("Camera3D")
	var vistos := []
	for e in despiertos():
		if e.visible:
			vistos.append(e)
	dm.set_meta("is_moving", true) # nadie se mueve mientras tanto
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
			# Se gira hacia el héroe que le ha sorprendido
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

func _marcar_luchadores() -> void:
	var en_pie := despiertos()
	for h in _heroes():
		for e in en_pie:
			if Tactica.dist(h.get_meta("grid_pos"), e.get_meta("grid_pos")) <= DIST_LEJOS:
				luchadores[str(h.name)] = true
				break

# ==========================================
# FIN DEL COMBATE: los que luchaban se reúnen
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
	# Sitios libres alrededor del líder, del más cercano al más lejano (por donde se puede andar)
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
	dm.call("ColocarHeroe", str(lider.name), lp.x, lp.y, 1) # visión recalculada con todos ya juntos
	_refrescar()

# BFS desde el líder por suelo libre: las 'n' casillas más cercanas donde ponerse
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
			# Libre o con uno de los que se van a mover (se apartará)
			var ocupada_por_quien_se_mueve := false
			for h in que_se_mueven:
				if h.get_meta("grid_pos") == s:
					ocupada_por_quien_se_mueve = true
			if (libres[s.x * lado + s.y] == 1 or ocupada_por_quien_se_mueve) and not quietos.has(s):
				out.append(s)
				cola.append(s)
	return out

# ==========================================
# LA FILA INDIA
# ==========================================
func _heroe(nombre: String) -> Node:
	var pm := _pm()
	var h = pm.get_node_or_null(NodePath(nombre)) if pm else null
	return h if h and Tactica.vivo(h) else null

# Al cambiar de líder: la fila son los que están cerca de él (encadenados), del más cercano al más lejano
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

## PathManager: el líder empieza a andar (explorando). Con Ctrl, se va solo: la fila se queda apartada.
func lider_se_mueve(_ruta: Array, t: float) -> void:
	paso_t = t
	if con_turnos():
		return
	if Input.is_key_pressed(KEY_CTRL):
		for n in fila:
			apartados[n] = false # todavía no se ha alejado del líder
		fila.clear()
		rastro.clear()

## Tactica.al_mover: el héroe 'h' da un paso de 'desde' a 'hasta'. Si es el líder explorando, la
## fila avanza una casilla: cada uno a donde estaba el de delante.
func paso_lider(h: Node, desde: Vector2i, hasta: Vector2i) -> void:
	if con_turnos() or _pm() == null or h != _pm().call("GetActiveCharacter"):
		return
	_unirse_a_la_fila(h, hasta)
	# El rastro del líder: las últimas casillas que ha pisado (la más reciente, la primera).
	# El seguidor i va a rastro[i]: casillas contiguas, así que quedan pegados uno tras otro.
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
			# Se ha quedado atrás (acaba de unirse, o algo le cortó): hasta dos pasos, más deprisa
			var ruta: Array = path_mgr.call("CalcularRuta", antes.x, antes.y, objetivo.x, objetivo.y)
			for k in range(1, mini(3, ruta.size())):
				pasos_f.append(ruta[k])
		# No acabar donde ya va el líder u otro de la fila en este paso
		while not pasos_f.is_empty() and ocupadas.has(pasos_f[pasos_f.size() - 1]):
			pasos_f.pop_back()
		if pasos_f.is_empty():
			ocupadas[antes] = true
		else:
			_andar(f, pasos_f, paso_t / float(pasos_f.size()))
			ocupadas[pasos_f[pasos_f.size() - 1]] = true

# Entran en la fila los que están cerca del líder o de alguien de la fila (los apartados con Ctrl,
# solo después de que el líder se haya alejado de ellos y vuelva)
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
					apartados[n] = true # ya se ha alejado: la próxima vez que pase cerca, se une
				continue
			if not cerca:
				continue
			apartados.erase(n)
		if cerca:
			fila.append(n)

# Anda casilla a casilla por 'pasos' (con saltito y mirando hacia donde va)
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
		# La casilla se apunta al llegar; la visión (cara) solo en el último paso
		tw.tween_callback(func(): dm.call("ColocarHeroe", nombre, c.x, c.y, 1 if ultimo else 0))
		previa = c
	# La casilla de destino se reserva ya (para que nadie más la elija en este mismo paso)
	h.set_meta("grid_pos", pasos_ruta[pasos_ruta.size() - 1])

## PathManager (explorando): ¿hay en esa casilla un aliado que NO va a apartarse? (los de la fila
## se apartan al seguir al líder; con Ctrl pulsado no se aparta nadie)
func aliado_quieto_en(x: int, y: int) -> bool:
	var pm := _pm()
	var lider = pm.call("GetActiveCharacter") if pm else null
	var c := Vector2i(x, y)
	var ctrl := Input.is_key_pressed(KEY_CTRL)
	for h in _heroes():
		if h != lider and h.get_meta("grid_pos") == c and (ctrl or not str(h.name) in fila):
			return true
	return false

## Grupos para la barra del HUD: el líder con su fila, y el resto juntando a los que están cerca
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

func grupo_de(h: Node) -> Array:
	for g in grupos_heroes():
		if h in g:
			return g
	return [h]

# ==========================================
# EL TIEMPO EXPLORANDO
# ==========================================
## Tactica.al_mover lo llama en cada paso de un héroe; solo cuentan los del que va delante
func contar_paso(h: Node) -> void:
	if con_turnos() or _pm() == null or h != _pm().call("GetActiveCharacter"):
		return
	pasos += 1
	if pasos >= PASOS_POR_DIA:
		pasos = 0
		_pm().call("PasarDiaExplorando")

## Descansar (botón del sol explorando): pasa el día sin andar
func descansar() -> void:
	if con_turnos():
		return
	pasos = 0
	_pm().call("PasarDiaExplorando")

# ==========================================
# COMBATE: los que están lejos
# ==========================================
## Héroes con PA a más de DIST_LEJOS casillas de cualquier enemigo despierto
func lejanos() -> Array:
	var out := []
	if not con_turnos():
		return out
	# En combate, lejos de los despiertos; en sigilo, lejos de los que vigilan
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

## Acaba el turno de todos los lejanos (botón del HUD, tecla L)
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

func _heroes() -> Array:
	var out := []
	for u in Tactica.unidades():
		if Tactica.es_heroe(u):
			out.append(u)
	return out

func _xp_total(h: Node) -> int:
	var total := int(h.get_meta("xp", 0))
	for n in range(1, int(h.get_meta("nivel", 1))):
		total += Progreso.xp_para(n)
	return total
