extends Node

# Indicadores (autoload): cuando un héroe o un enemigo gana o pierde algo (daño, curación, buffs,
# debuffs), sale sobre su cabeza el icono de la stat con el número durante ~2 s: rojo si pierde,
# verde si gana. Varios cambios a la vez se apilan hacia arriba.
#
# No hace falta avisarle: cada frame compara las stats de todos los personajes con las del frame
# anterior. Solo hay que llamar a revisar(nodo) justo antes de quitar a alguien del mapa (un enemigo
# que muere), para que su último golpe también se vea.
# Los iconos son placeholders en Assets/UI/Indicadores (ver LEEME.md).

const STATS := {
	"hp_actual": "vida", "velocidad": "velocidad", "fuerza": "fuerza", "defensa": "defensa",
	"magia": "magia", "suerte": "suerte", "sanidad_actual": "cordura",
}
const NOMBRES := {
	"hp_actual": "Vida", "velocidad": "Velocidad", "fuerza": "Fuerza", "defensa": "Defensa",
	"magia": "Magia", "suerte": "Suerte", "sanidad_actual": "Cordura",
}
const CARPETA := "res://Assets/UI/Indicadores/"
const DURACION := 2.0
const ALTO_ICONO := 0.17  # unidades de mundo
const SEPARACION := 0.2   # entre indicadores apilados
const SUBIDA := 0.15      # lo que suben mientras duran
const ALTURA := 1.05      # sobre los pies de la ficha

var memoria := {}   # instance_id -> {stat: valor}
var activos: Array = [] # {nodo, ficha (WeakRef o null), base: Vector3, fila, t}
var texturas := {}
var mostrados := 0 # cuántos se han enseñado (lo mira el test)

func icono(stat: String) -> Texture2D:
	if not texturas.has(stat):
		var ruta: String = CARPETA + str(STATS.get(stat, stat)) + ".png"
		# Las stats que no salen flotando (luz, hambre) usan el icono del HUD
		var extra: String = "res://Assets/UI/Iconos/" + str(EXTRA.get(stat, stat)) + ".png"
		texturas[stat] = load(ruta) if ResourceLoader.exists(ruta) else (load(extra) if ResourceLoader.exists(extra) else null)
	return texturas[stat]

const EXTRA := {"radio_luz": "radio_luz", "hambre_actual": "hambre", "pa_actual": "pa"}
const NOMBRES_EXTRA := {"radio_luz": "Luz", "hambre_actual": "Hambre", "pa_actual": "PA"}

func nombre(stat: String) -> String:
	return NOMBRES.get(stat, NOMBRES_EXTRA.get(stat, stat.capitalize()))

func _dm() -> Node:
	return get_node_or_null("/root/Main/ContenedorMapa/SubViewport/DungeonManager")

# Mientras algo tapa el mapa (evento, menú en pausa, mochila) no se compara: lo que cambie se
# enseña al volver. Los cambios de equipo en la mochila no son buffs: se apuntan sin enseñarse.
func _en_pausa(dm: Node) -> bool:
	return get_tree().paused or dm.get_meta("bloqueo_evento", false)

func _mochila_abierta() -> bool:
	for n in get_tree().get_nodes_in_group("pantalla_inventario"):
		if n.has_method("esta_abierta") and n.esta_abierta():
			return true
	return false

func _process(delta: float) -> void:
	_animar(delta)
	var dm := _dm()
	if dm == null:
		memoria.clear()
		return
	if _en_pausa(dm):
		return
	var mostrar := not _mochila_abierta()
	var vistos := {}
	for par in _personajes(dm):
		vistos[par[0].get_instance_id()] = true
		_revisar(par[0], par[1], mostrar)
	for id in memoria.keys():
		if not vistos.has(id):
			memoria.erase(id)

# [datos, ficha]: los héroes guardan sus stats en el PartyManager y su ficha está en el mapa;
# los enemigos son su propia ficha
func _personajes(dm: Node) -> Array:
	var out := []
	var pm := get_node_or_null("/root/Main/PartyManager")
	if pm:
		for h in pm.get_children():
			if h.has_meta("grid_pos") and h.has_meta("hp_actual"):
				out.append([h, dm.get_node_or_null(NodePath(str(h.name)))])
	for c in dm.get_children():
		var n := str(c.name)
		if n.begins_with("Enemy_") and not n.ends_with("_muerto") and c.has_meta("hp_actual"):
			out.append([c, c])
	return out

## Compara ya las stats de 'datos' (p. ej. un enemigo que va a desaparecer) y enseña lo que cambió
func revisar(datos: Node) -> void:
	var dm := _dm()
	if datos == null or dm == null:
		return
	var ficha: Node = datos if datos is Node3D else dm.get_node_or_null(NodePath(str(datos.name)))
	_revisar(datos, ficha, true)

func _revisar(datos: Node, ficha: Node, mostrar: bool) -> void:
	var id := datos.get_instance_id()
	var ahora := {}
	for s in STATS:
		if datos.has_meta(s):
			ahora[s] = int(datos.get_meta(s))
	var antes = memoria.get(id)
	memoria[id] = ahora
	if antes == null or not mostrar or not (ficha is Node3D) or not ficha.visible:
		return
	for s in ahora:
		if antes.has(s) and int(ahora[s]) != int(antes[s]):
			mostrar_en(ficha, s, int(ahora[s]) - int(antes[s]))

## Enseña un indicador sobre la cabeza de 'ficha' (lo sigue si se mueve)
func mostrar_en(ficha: Node3D, stat: String, cambio: int) -> void:
	var dm := _dm()
	if dm == null or cambio == 0:
		return
	# Al registro del HUD: "Zombi: -6 Vida"
	var hud := get_tree().get_first_node_in_group("hud")
	if hud and stat != "xp":
		var quien := str(ficha.get_meta("nombre_display", ficha.name))
		hud.registrar("%s: %+d %s" % [quien, cambio, nombre(stat)], Color(0.55, 0.9, 0.55) if cambio > 0 else Color(0.95, 0.5, 0.45))
	var fila := 0
	for a in activos:
		if a["ficha"] != null and a["ficha"].get_ref() == ficha and a["t"] < DURACION * 0.6:
			fila = maxi(fila, int(a["fila"]) + 1)
	var raiz := Node3D.new()
	raiz.name = "Indicador"
	raiz.add_to_group("indicadores")
	var color := Color(0.45, 1.0, 0.5) if cambio > 0 else Color(1.0, 0.32, 0.27)
	var tex := icono(stat)
	var icono3d := Sprite3D.new()
	icono3d.texture = tex
	icono3d.texture_filter = BaseMaterial3D.TEXTURE_FILTER_NEAREST
	icono3d.no_depth_test = true
	icono3d.render_priority = 10
	icono3d.shaded = false
	icono3d.pixel_size = ALTO_ICONO / float(tex.get_height()) if tex else 0.0125
	icono3d.position = Vector3(-0.1, 0, 0)
	raiz.add_child(icono3d)
	var numero := Label3D.new()
	numero.text = ("+%d" % cambio) if cambio > 0 else str(cambio)
	numero.modulate = color
	numero.outline_modulate = Color(0, 0, 0)
	numero.font_size = 32
	numero.outline_size = 8
	numero.pixel_size = 0.006
	numero.no_depth_test = true
	numero.render_priority = 11
	numero.outline_render_priority = 10
	numero.texture_filter = BaseMaterial3D.TEXTURE_FILTER_NEAREST
	numero.horizontal_alignment = HORIZONTAL_ALIGNMENT_LEFT
	numero.position = Vector3(0.0, 0, 0)
	raiz.add_child(numero)
	dm.add_child(raiz, true)
	var a := {"nodo": raiz, "ficha": weakref(ficha), "base": ficha.global_position, "fila": fila, "t": 0.0}
	activos.append(a)
	mostrados += 1
	_colocar(a)

func _colocar(a: Dictionary) -> void:
	var raiz: Node3D = a["nodo"]
	var f = a["ficha"].get_ref() if a["ficha"] != null else null
	if f is Node3D and is_instance_valid(f):
		a["base"] = f.global_position
	var t: float = a["t"]
	var y := ALTURA + float(a["fila"]) * SEPARACION + SUBIDA * clampf(t / DURACION, 0.0, 1.0)
	raiz.global_position = a["base"] + Vector3(0, y, 0)
	# De cara a la cámara (así el número queda a la derecha del icono en pantalla)
	var cam := raiz.get_viewport().get_camera_3d()
	if cam:
		raiz.global_basis = cam.global_basis
	# Aparece con un pequeño rebote y se desvanece al final
	var s := 1.0
	if t < 0.12:
		s = lerpf(0.4, 1.25, t / 0.12)
	elif t < 0.22:
		s = lerpf(1.25, 1.0, (t - 0.12) / 0.1)
	raiz.scale = Vector3.ONE * s
	var alfa := clampf((DURACION - t) / 0.4, 0.0, 1.0)
	for c in raiz.get_children():
		c.modulate.a = alfa
		if c is Label3D:
			c.outline_modulate.a = alfa

func _animar(delta: float) -> void:
	for i in range(activos.size() - 1, -1, -1):
		var a: Dictionary = activos[i]
		if not is_instance_valid(a["nodo"]):
			activos.remove_at(i)
			continue
		a["t"] += delta
		if a["t"] >= DURACION:
			a["nodo"].queue_free()
			activos.remove_at(i)
			continue
		_colocar(a)
