extends CanvasLayer

# Mochila: inventario y equipo (tecla I, botón MOCHILA del HUD o "Repartir objetos" en la fogata).
#   Izquierda: los héroes (su sprite, animado). Clic = ver su mochila; soltar un objeto encima = dárselo.
#   Centro: el héroe en grande con su animación de reposo y lo que lleva PUESTO dibujado encima
#     (EquipoData.sprite_puesto). Ratón encima = la pieza se resalta; clic = bocadillo con lo que hace.
#   Derecha: el saco. Los objetos están tirados dentro con algo de física: caen, se apilan y se
#     pueden coger y lanzar. Cada héroe recuerda dónde dejó cada cosa. La carga, en el borde.
#   Abajo: estado (vida, PA, cordura, hambre) y atributos (fuerza, defensa, magia, suerte, luz).
# Arrastrar del saco al héroe = equipar (o usar, si es un consumible); del héroe al saco = quitar;
# a otro héroe = dárselo; fuera de la ventana = tirarlo. Doble clic = equipar/usar/quitar.
# Clic derecho = menú. Nada de esto cuesta PA. Todo pasa por el PartyManager (Inventario.hpp).

const Textos := preload("res://Scripts/GDD/textos_objetos.gd")
const HOJA := preload("res://Assets/2D Pixel Dungeon Asset Pack/character and tileset/Dungeon_Character_2.png")
const SHADER_CONTORNO := preload("res://Shaders/contorno_ui.gdshader")

const ESCALA_HEROE := 16          # el sprite de 16x16 se ve a 256x256
const TAM_OBJETO := 60            # objetos del saco
const RADIO := 27.0               # su "radio" para chocar entre ellos
const GRAVEDAD := 1500.0
const T_IDLE := 0.5               # cada cuánto alterna el sprite (como en el mapa)
const UMBRAL_ARRASTRE := 6.0
# Orden de dibujo de lo puesto: la mochila detrás del héroe, el resto encima
const CAPAS_DETRAS := ["mochila"]
const CAPAS_DELANTE := ["cuerpo", "cabeza", "accesorio_1", "accesorio_2", "luz", "mano_secundaria", "mano_principal"]
const ARMAS_EN_MANO := ["mano_principal"]
const STATS_ESTADO := [["vida", "hp_actual", "hp_max", "Vida"], ["pa", "pa_actual", "pa_max", "PA (acciones de este turno)"],
	["cordura", "sanidad_actual", "sanidad_max", "Cordura"], ["hambre", "hambre_actual", "hambre_max", "Saciedad"]]
const STATS_ATRIBUTOS := [["fuerza", "Fuerza: daño de las armas de fuerza"], ["defensa", "Defensa: resta al daño que recibes"],
	["magia", "Magia: daño de bastones y conjuros"], ["suerte", "Suerte: probabilidad de crítico"], ["radio_luz", "Luz: casillas que ves alrededor"]]

var pm: Node
var cat: Node
var heroe := ""
var seleccion := {} # {"origen": "inv", "indice": i} o {"origen": "eq", "ranura": r}
var firma := ""
var t_idle := 0.0
var fase_idle := 0

# Nodos
var fondo: ColorRect
var marco: PanelContainer
var retratos: VBoxContainer
var muneco: PanelContainer
var figura: Control
var capas := {}          # ranura (o "heroe") -> TextureRect
var saco: PanelContainer
var interior: Control    # donde caen los objetos
var barra_carga: ProgressBar
var lbl_carga: Label
var fila_estado: HBoxContainer
var fila_atributos: HBoxContainer
var lbl_msg: Label
var bocadillo: PanelContainer
var det_texto: RichTextLabel # texto del bocadillo
var cola: Polygon2D
var menu: PopupMenu
var menu_acciones := {}
var boton_hud: Button
var boton_fogata: Button
var fantasma: TextureRect    # lo que se arrastra desde el héroe

# Física del saco: una entrada por objeto del inventario
var objetos: Array = []      # [{nodo, pos, vel, ang, giro, clave, indice}]
var cogido := -1             # índice en 'objetos' del que se arrastra
var desfase := Vector2.ZERO
var pos_anterior := Vector2.ZERO
var pulsado_en := Vector2.ZERO
var arrastrando_puesto := ""  # ranura que se arrastra desde el héroe
var bajo_raton := ""          # ranura resaltada en el héroe
var resaltado_saco := -1

var material_resalte: ShaderMaterial
var imagenes_cache := {}
var hoja_img: Image # la hoja de personajes, descomprimida (de una imagen comprimida no se recorta nada)

func _hoja() -> Image:
	if hoja_img == null:
		hoja_img = HOJA.get_image()
		if hoja_img.is_compressed():
			hoja_img.decompress()
	return hoja_img

func _ready() -> void:
	layer = 12
	pm = get_node_or_null("/root/Main/PartyManager")
	cat = get_node_or_null("/root/Catalogo")
	material_resalte = ShaderMaterial.new()
	material_resalte.shader = SHADER_CONTORNO
	_construir()
	_crear_botones_de_acceso()
	cerrar()

func esta_abierta() -> bool:
	return marco.visible

func puede_abrir() -> bool:
	var dm := get_node_or_null("/root/Main/ContenedorMapa/SubViewport/DungeonManager")
	if dm and (dm.get_meta("turno_enemigo", false) or dm.get_meta("bloqueo_evento", false)):
		return false
	return pm != null

func alternar() -> void:
	if marco.visible:
		cerrar()
	else:
		abrir()

func abrir(nombre: String = "") -> void:
	if not puede_abrir():
		return
	if nombre != "":
		heroe = nombre
	if heroe == "" or not _nodo(heroe) or _nodo(heroe).get_meta("esta_muerto", false):
		var act = pm.call("GetActiveCharacter")
		heroe = str(act.name) if act else (str(_heroes()[0].name) if not _heroes().is_empty() else "")
	seleccion = {}
	lbl_msg.text = ""
	fondo.visible = true
	marco.visible = true
	_cerrar_bocadillo()
	_refrescar(true)
	_sonido("open")

func cerrar() -> void:
	_guardar_posiciones()
	fondo.visible = false
	marco.visible = false
	if bocadillo:
		_cerrar_bocadillo()

# ==========================================
# CONSTRUCCIÓN
# ==========================================

func _boton(texto: String, accion: Callable) -> Button:
	var b := Button.new()
	b.text = texto
	b.add_theme_font_size_override("font_size", 14)
	b.focus_mode = Control.FOCUS_NONE
	b.pressed.connect(accion)
	return b

func _construir() -> void:
	fondo = ColorRect.new()
	fondo.color = Estilo.color("velo")
	fondo.set_anchors_preset(Control.PRESET_FULL_RECT)
	fondo.gui_input.connect(func(ev):
		if ev is InputEventMouseButton and ev.pressed and ev.button_index == MOUSE_BUTTON_LEFT and cogido < 0:
			cerrar())
	add_child(fondo)

	marco = PanelContainer.new()
	marco.name = "PantallaInventario"
	marco.anchor_left = 0.5
	marco.anchor_right = 0.5
	marco.anchor_top = 0.5
	marco.anchor_bottom = 0.5
	marco.offset_left = -470
	marco.offset_right = 470
	marco.offset_top = -310
	marco.offset_bottom = 310
	marco.add_theme_stylebox_override("panel", Estilo.caja("panel_oro"))
	add_child(marco)

	var v := VBoxContainer.new()
	v.add_theme_constant_override("separation", 10)
	marco.add_child(v)

	var cab := HBoxContainer.new()
	cab.add_child(Estilo.etiqueta("Equipo", "titulo", "oro"))
	lbl_msg = Estilo.etiqueta("", "pequeno", "oro")
	lbl_msg.size_flags_horizontal = Control.SIZE_EXPAND_FILL
	lbl_msg.horizontal_alignment = HORIZONTAL_ALIGNMENT_CENTER
	cab.add_child(lbl_msg)
	cab.add_child(_boton(" X ", cerrar))
	v.add_child(cab)

	var cuerpo := HBoxContainer.new()
	cuerpo.add_theme_constant_override("separation", 16)
	cuerpo.size_flags_vertical = Control.SIZE_EXPAND_FILL
	v.add_child(cuerpo)

	# 1. Héroes
	retratos = VBoxContainer.new()
	retratos.add_theme_constant_override("separation", 10)
	cuerpo.add_child(retratos)

	# 2. El héroe con lo puesto
	muneco = PanelContainer.new()
	muneco.custom_minimum_size = Vector2(300, 0)
	muneco.add_theme_stylebox_override("panel", Estilo.caja("hueco"))
	figura = Control.new()
	figura.custom_minimum_size = Vector2(16 * ESCALA_HEROE, 16 * ESCALA_HEROE)
	figura.size_flags_horizontal = Control.SIZE_SHRINK_CENTER
	figura.size_flags_vertical = Control.SIZE_SHRINK_CENTER
	figura.mouse_filter = Control.MOUSE_FILTER_STOP
	figura.gui_input.connect(_al_tocar_figura)
	figura.mouse_exited.connect(func(): _resaltar_puesto(""))
	muneco.add_child(figura)
	for ranura in CAPAS_DETRAS + ["heroe"] + CAPAS_DELANTE:
		var t := TextureRect.new()
		t.size = figura.custom_minimum_size
		t.expand_mode = TextureRect.EXPAND_IGNORE_SIZE
		t.texture_filter = CanvasItem.TEXTURE_FILTER_NEAREST
		t.mouse_filter = Control.MOUSE_FILTER_IGNORE
		figura.add_child(t)
		capas[ranura] = t
	cuerpo.add_child(muneco)

	# 3. El saco
	saco = PanelContainer.new()
	saco.size_flags_horizontal = Control.SIZE_EXPAND_FILL
	saco.add_theme_stylebox_override("panel", Estilo.caja("hueco"))
	var dentro := HBoxContainer.new()
	dentro.add_theme_constant_override("separation", 6)
	saco.add_child(dentro)
	interior = Control.new()
	interior.size_flags_horizontal = Control.SIZE_EXPAND_FILL
	interior.clip_contents = true
	interior.mouse_filter = Control.MOUSE_FILTER_PASS
	dentro.add_child(interior)
	var col_carga := VBoxContainer.new()
	barra_carga = ProgressBar.new()
	barra_carga.fill_mode = ProgressBar.FILL_BOTTOM_TO_TOP
	barra_carga.show_percentage = false
	barra_carga.custom_minimum_size = Vector2(12, 0)
	barra_carga.size_flags_vertical = Control.SIZE_EXPAND_FILL
	barra_carga.size_flags_horizontal = Control.SIZE_SHRINK_CENTER
	col_carga.add_child(barra_carga)
	lbl_carga = Estilo.etiqueta("", "pequeno", "texto_suave")
	lbl_carga.horizontal_alignment = HORIZONTAL_ALIGNMENT_CENTER
	col_carga.add_child(lbl_carga)
	dentro.add_child(col_carga)
	cuerpo.add_child(saco)

	# 4. Barra de stats
	var abajo := PanelContainer.new()
	abajo.add_theme_stylebox_override("panel", Estilo.caja("hueco"))
	var fila := HBoxContainer.new()
	fila_estado = HBoxContainer.new()
	fila_estado.add_theme_constant_override("separation", 18)
	fila.add_child(fila_estado)
	var hueco := Control.new()
	hueco.size_flags_horizontal = Control.SIZE_EXPAND_FILL
	fila.add_child(hueco)
	fila_atributos = HBoxContainer.new()
	fila_atributos.add_theme_constant_override("separation", 18)
	fila.add_child(fila_atributos)
	abajo.add_child(fila)
	v.add_child(abajo)

	# Bocadillo de cómic (papel hueso, como los del mapa) con la info del objeto
	bocadillo = PanelContainer.new()
	var estilo := StyleBoxFlat.new()
	estilo.bg_color = Color(0.85, 0.84, 0.8)
	estilo.border_color = Color(0.08, 0.09, 0.11)
	estilo.set_border_width_all(2)
	estilo.set_corner_radius_all(6)
	estilo.set_content_margin_all(10)
	bocadillo.add_theme_stylebox_override("panel", estilo)
	bocadillo.mouse_filter = Control.MOUSE_FILTER_IGNORE
	bocadillo.custom_minimum_size = Vector2(290, 0)
	det_texto = RichTextLabel.new()
	det_texto.bbcode_enabled = true
	det_texto.fit_content = true
	det_texto.scroll_active = false
	det_texto.custom_minimum_size = Vector2(270, 0)
	det_texto.mouse_filter = Control.MOUSE_FILTER_IGNORE
	det_texto.add_theme_color_override("default_color", Color(0.13, 0.14, 0.17))
	det_texto.add_theme_font_size_override("normal_font_size", 13)
	det_texto.add_theme_font_size_override("bold_font_size", 15)
	det_texto.add_theme_font_size_override("italics_font_size", 13)
	bocadillo.add_child(det_texto)
	cola = Polygon2D.new()
	cola.color = Color(0.85, 0.84, 0.8)
	add_child(cola)
	add_child(bocadillo)
	_cerrar_bocadillo()

	fantasma = TextureRect.new()
	fantasma.size = Vector2(TAM_OBJETO, TAM_OBJETO)
	fantasma.expand_mode = TextureRect.EXPAND_IGNORE_SIZE
	fantasma.texture_filter = CanvasItem.TEXTURE_FILTER_NEAREST
	fantasma.mouse_filter = Control.MOUSE_FILTER_IGNORE
	fantasma.modulate.a = 0.85
	fantasma.visible = false
	add_child(fantasma)

	menu = PopupMenu.new()
	menu.add_theme_font_size_override("font_size", 14)
	menu.id_pressed.connect(func(id):
		if menu_acciones.has(id):
			menu_acciones[id].call())
	add_child(menu)

func _crear_botones_de_acceso() -> void:
	boton_hud = _boton("MOCHILA (I)", alternar)
	boton_hud.add_theme_font_size_override("font_size", 12)
	boton_hud.position = Vector2(186, 8)
	boton_hud.size = Vector2(130, 30)
	add_child(boton_hud)
	move_child(boton_hud, 0)
	var fogata := get_node_or_null("/root/Main/CanvasLayer3/PantallaFogata")
	if fogata:
		boton_fogata = _boton("Repartir objetos (I)", func(): abrir())
		boton_fogata.name = "BtnRepartir"
		boton_fogata.anchor_left = 0.5
		boton_fogata.anchor_right = 0.5
		boton_fogata.anchor_top = 1.0
		boton_fogata.anchor_bottom = 1.0
		boton_fogata.offset_left = -154
		boton_fogata.offset_right = 154
		boton_fogata.offset_top = -148
		boton_fogata.offset_bottom = -108
		fogata.add_child(boton_fogata)

# ==========================================
# DATOS
# ==========================================

func _heroes() -> Array:
	var out: Array = []
	if pm:
		for h in pm.get_children():
			if h.name != "DatosEquipo" and not h.get_meta("esta_muerto", false):
				out.append(h)
	return out

func _nodo(nombre: String) -> Node:
	return pm.get_node_or_null(NodePath(nombre)) if pm and nombre != "" else null

func _def(id: String) -> ObjetoData:
	return cat.obtener(id) if cat and id != "" else null

func _inventario() -> Array:
	return pm.call("DescribirInventario", heroe) if heroe != "" else []

func _equipado(ranura: String) -> ObjetoData:
	var h := _nodo(heroe)
	return _def(h.get_meta("equipo", {}).get(ranura, "")) if h else null

func _equipado_para(o: ObjetoData) -> ObjetoData:
	if not (o is EquipoData):
		return null
	return _equipado(Textos.RANURA_DE[o.ranura])

func _objeto_de(clave: Dictionary) -> ObjetoData:
	match clave.get("origen", ""):
		"eq":
			return _equipado(clave["ranura"])
		"inv":
			var inv := _inventario()
			var i := int(clave["indice"])
			return _def(inv[i]["id"]) if i < inv.size() else null
	return null

# Fotograma del sprite del héroe en la hoja y su pareja de reposo (igual que en el mapa)
func _frame(h: Node) -> int:
	return {"Ladron": 0, "Mago": 2, "Caballero": 4, "Tanque": 6}.get(str(h.get_meta("clase", "")), 4)

static func pareja(f: int) -> int:
	return {0: 1, 1: 0, 2: 3, 3: 2, 4: 5, 5: 4, 6: 5, 9: 10, 10: 9, 11: 12, 12: 11, 13: 12}.get(f, f)

# Sprite del héroe; sin el arma que trae dibujada si lleva otra en la mano
func _textura_heroe(f: int, sin_arma: bool) -> Texture2D:
	var clave := "%d_%s" % [f, sin_arma]
	if imagenes_cache.has(clave):
		return imagenes_cache[clave]
	var im := _hoja().get_region(Rect2i((f % 7) * 16, (f / 7) * 16, 16, 16))
	if sin_arma:
		for x in range(13, 16):
			for y in range(0, 11):
				im.set_pixel(x, y, Color(0, 0, 0, 0))
	var tex := ImageTexture.create_from_image(im)
	imagenes_cache[clave] = tex
	return tex

# Cuántos píxeles baja el héroe en su segundo fotograma (para que lo puesto se mueva con él)
func _bajada(f: int) -> int:
	var clave := "bajada_%d" % f
	if imagenes_cache.has(clave):
		return imagenes_cache[clave]
	var hoja := _hoja()
	var arriba := func(fr: int) -> int:
		for y in 16:
			for x in 16:
				if hoja.get_pixel((fr % 7) * 16 + x, (fr / 7) * 16 + y).a > 0.1:
					return y
		return 0
	var d: int = arriba.call(pareja(f)) - arriba.call(f)
	imagenes_cache[clave] = d
	return d

func _imagen(tex: Texture2D) -> Image:
	if not imagenes_cache.has(tex):
		var im: Image = tex.get_image() if tex else null
		if im and im.is_compressed():
			im.decompress()
		imagenes_cache[tex] = im
	return imagenes_cache[tex]

func _sonido(nombre: String) -> void:
	var audio := get_node_or_null("/root/AudioManager")
	if audio:
		audio.call("play_ui", nombre)

func _resultado(accion: String, error: String) -> void:
	lbl_msg.text = accion + (": hecho" if error == "" else ": " + error)
	lbl_msg.add_theme_color_override("font_color", Estilo.color("oro") if error == "" else Estilo.color("mal"))
	_sonido("clic" if error == "" else "no")
	_refrescar(true)

# ==========================================
# BUCLE: animación, física y arrastres
# ==========================================

func _process(delta: float) -> void:
	if boton_hud:
		boton_hud.visible = not get_tree().paused and puede_abrir() and not marco.visible
	if not marco.visible:
		return
	_refrescar(false)
	t_idle += delta
	if t_idle >= T_IDLE:
		t_idle = 0.0
		fase_idle = 1 - fase_idle
		_pintar_heroe()
		_animar_retratos()
	_fisica(minf(delta, 1.0 / 30.0))
	if fantasma.visible:
		fantasma.position = fantasma.get_viewport().get_mouse_position() - fantasma.size * 0.5

func _input(event: InputEvent) -> void:
	if event is InputEventKey and event.pressed and not event.echo and event.physical_keycode == KEY_I:
		alternar()
		get_viewport().set_input_as_handled()
		return
	if not marco.visible:
		return
	# Soltar lo que se arrastra (del saco o desde el héroe), esté donde esté el ratón
	if event is InputEventMouseButton and not event.pressed and event.button_index == MOUSE_BUTTON_LEFT:
		if cogido >= 0:
			_soltar_del_saco(event.position)
		elif arrastrando_puesto != "":
			_soltar_puesto(event.position)
	elif event is InputEventMouseMotion and arrastrando_puesto == "" and bajo_raton != "" and event.button_mask & MOUSE_BUTTON_MASK_LEFT:
		# Empezar a arrastrar una pieza puesta en cuanto el ratón se mueve un poco
		if event.position.distance_to(pulsado_en) > UMBRAL_ARRASTRE and _equipado(bajo_raton):
			arrastrando_puesto = bajo_raton
			fantasma.texture = _equipado(bajo_raton).icono
			fantasma.visible = true
			_cerrar_bocadillo()

func _fisica(dt: float) -> void:
	var area := interior.size
	var raton := interior.get_local_mouse_position()
	for i in objetos.size():
		var o: Dictionary = objetos[i]
		if i == cogido:
			var nueva: Vector2 = raton - desfase
			o["vel"] = (nueva - o["pos"]) / maxf(dt, 0.001)
			o["pos"] = nueva
			o["giro"] = o["vel"].x * 0.002
		else:
			o["vel"].y += GRAVEDAD * dt
			o["pos"] += o["vel"] * dt
			o["ang"] += o["giro"] * dt
			o["giro"] *= 0.9
			# Paredes y suelo del saco
			if o["pos"].y > area.y - RADIO:
				o["pos"].y = area.y - RADIO
				o["vel"].y *= -0.25
				o["vel"].x *= 0.82
				o["giro"] = o["vel"].x / RADIO
				o["ang"] = lerpf(o["ang"], 0.0, 0.08)
			if o["pos"].x < RADIO:
				o["pos"].x = RADIO
				o["vel"].x *= -0.4
			elif o["pos"].x > area.x - RADIO:
				o["pos"].x = area.x - RADIO
				o["vel"].x *= -0.4
			if o["pos"].y < RADIO:
				o["pos"].y = RADIO
				o["vel"].y *= -0.3
	# Choques entre objetos (se empujan para no solaparse)
	for _iter in 2:
		for a in objetos.size():
			for b in range(a + 1, objetos.size()):
				var pa: Dictionary = objetos[a]
				var pb: Dictionary = objetos[b]
				var d: Vector2 = pb["pos"] - pa["pos"]
				var l := d.length()
				if l >= RADIO * 2.0 or l < 0.001:
					continue
				var n := d / l
				var empuje := (RADIO * 2.0 - l) * 0.5
				if a != cogido:
					pa["pos"] -= n * empuje * (2.0 if b == cogido else 1.0)
				if b != cogido:
					pb["pos"] += n * empuje * (2.0 if a == cogido else 1.0)
				var rel: float = (pb["vel"] - pa["vel"]).dot(n)
				if rel < 0:
					var imp := -rel * 0.6
					if a != cogido:
						pa["vel"] -= n * imp * 0.5
					if b != cogido:
						pb["vel"] += n * imp * 0.5
	for o in objetos:
		var nodo: Control = o["nodo"]
		nodo.position = o["pos"] - nodo.size * 0.5
		nodo.rotation = o["ang"]

# ==========================================
# PINTAR
# ==========================================

func _refrescar(forzar: bool) -> void:
	var h := _nodo(heroe)
	if not h:
		return
	var nueva := var_to_str([heroe, h.get_meta("inventario", []), h.get_meta("equipo", {}), h.get_meta("hp_actual", 0), h.get_meta("pa_actual", 0), _heroes().size()])
	if not forzar and nueva == firma:
		return
	firma = nueva
	_pintar_retratos()
	_pintar_heroe()
	_pintar_saco()
	_pintar_stats(h)

func _pintar_retratos() -> void:
	for c in retratos.get_children():
		c.queue_free()
	for h in _heroes():
		var nombre := str(h.name)
		var junto: bool = nombre == heroe or bool(pm.call("PuedenIntercambiar", heroe, nombre))
		var r := PanelContainer.new()
		r.name = nombre
		r.custom_minimum_size = Vector2(72, 72)
		r.add_theme_stylebox_override("panel", Estilo.caja("hueco_oro" if nombre == heroe else "hueco"))
		r.tooltip_text = str(h.get_meta("nombre_display", nombre)) + ("" if junto else "\nEstá lejos: solo podéis intercambiar pegados o en la fogata")
		r.mouse_filter = Control.MOUSE_FILTER_STOP
		var t := TextureRect.new()
		t.texture = _textura_heroe(_frame(h), false)
		t.set_meta("frame", _frame(h))
		t.expand_mode = TextureRect.EXPAND_IGNORE_SIZE
		t.stretch_mode = TextureRect.STRETCH_KEEP_ASPECT_CENTERED
		t.texture_filter = CanvasItem.TEXTURE_FILTER_NEAREST
		t.mouse_filter = Control.MOUSE_FILTER_IGNORE
		r.add_child(t)
		if not junto:
			r.modulate = Color(0.55, 0.55, 0.6)
		r.gui_input.connect(func(ev):
			if ev is InputEventMouseButton and ev.pressed and ev.button_index == MOUSE_BUTTON_LEFT and cogido < 0:
				_cambiar_heroe(nombre))
		retratos.add_child(r)

func _animar_retratos() -> void:
	for r in retratos.get_children():
		if r.get_child_count() == 0:
			continue
		var t: TextureRect = r.get_child(0)
		var f: int = t.get_meta("frame", 4)
		t.texture = _textura_heroe(f if fase_idle == 0 else pareja(f), false)

func _cambiar_heroe(nombre: String) -> void:
	if nombre == heroe:
		return
	_guardar_posiciones()
	heroe = nombre
	seleccion = {}
	lbl_msg.text = ""
	_cerrar_bocadillo()
	for o in objetos:
		o["nodo"].queue_free()
	objetos.clear()
	_refrescar(true)

func _pintar_heroe() -> void:
	var h := _nodo(heroe)
	if not h:
		return
	var f := _frame(h)
	var fr := f if fase_idle == 0 else pareja(f)
	var con_arma := _equipado("mano_principal") != null
	capas["heroe"].texture = _textura_heroe(fr, con_arma)
	var bajada := (_bajada(f) if fase_idle == 1 else 0) * ESCALA_HEROE
	for ranura in CAPAS_DETRAS + CAPAS_DELANTE:
		var o := _equipado(ranura)
		var t: TextureRect = capas[ranura]
		t.texture = o.sprite_puesto if o is EquipoData and o.sprite_puesto else null
		t.position.y = bajada
		t.material = material_resalte if ranura == bajo_raton else null

func _pintar_stats(h: Node) -> void:
	for c in fila_estado.get_children() + fila_atributos.get_children():
		c.queue_free()
	for s in STATS_ESTADO:
		fila_estado.add_child(_stat(s[0], "%d/%d" % [h.get_meta(s[1], 0), h.get_meta(s[2], 0)], s[0] if s[0] != "pa" else "oro", s[3], 0))
	var base: Dictionary = h.get_meta("stats_base", {})
	for s in STATS_ATRIBUTOS:
		var f := int(h.get_meta(s[0], 0))
		fila_atributos.add_child(_stat(s[0], str(f), "hueso", s[1], f - int(base.get(s[0], f))))

func _stat(icono: String, valor: String, color: String, ayuda: String, bono: int) -> Control:
	var h := HBoxContainer.new()
	h.add_theme_constant_override("separation", 5)
	h.tooltip_text = ayuda
	h.mouse_filter = Control.MOUSE_FILTER_STOP
	var t := TextureRect.new()
	t.texture = load("res://Assets/UI/Iconos/%s.png" % icono)
	t.custom_minimum_size = Vector2(24, 24)
	t.texture_filter = CanvasItem.TEXTURE_FILTER_NEAREST
	t.mouse_filter = Control.MOUSE_FILTER_IGNORE
	h.add_child(t)
	var l := Estilo.etiqueta(valor, "normal", color)
	l.mouse_filter = Control.MOUSE_FILTER_IGNORE
	h.add_child(l)
	if bono != 0:
		var b := Estilo.etiqueta("%+d" % bono, "pequeno", "bien" if bono > 0 else "mal")
		b.mouse_filter = Control.MOUSE_FILTER_IGNORE
		h.add_child(b)
	return h

# ==========================================
# EL SACO
# ==========================================

# Una clave estable por unidad de inventario: "pocion_vida#0", "pocion_vida#1"...
func _claves(inv: Array) -> Array:
	var vistos := {}
	var out: Array = []
	for e in inv:
		var n: int = vistos.get(e["id"], 0)
		vistos[e["id"]] = n + 1
		out.append("%s#%d" % [e["id"], n])
	return out

func _guardar_posiciones() -> void:
	var h := _nodo(heroe)
	if not h or objetos.is_empty():
		return
	var pos := {}
	for o in objetos:
		pos[o["clave"]] = o["pos"]
	h.set_meta("pos_mochila", pos)

func _pintar_saco() -> void:
	var h := _nodo(heroe)
	var inv := _inventario()
	var claves := _claves(inv)
	var guardadas: Dictionary = h.get_meta("pos_mochila", {}) if h else {}
	# Lo que ya está en el saco conserva su sitio (y su velocidad)
	var previas := {}
	for o in objetos:
		previas[o["clave"]] = o
	var nuevos: Array = []
	for i in inv.size():
		var clave: String = claves[i]
		var o: Dictionary
		if previas.has(clave):
			o = previas[clave]
			previas.erase(clave)
		else:
			var nodo := _nodo_objeto()
			interior.add_child(nodo)
			var inicial: Vector2 = guardadas.get(clave, Vector2(randf_range(RADIO, maxf(RADIO + 1.0, interior.size.x - RADIO)), RADIO))
			o = {"nodo": nodo, "pos": inicial, "vel": Vector2(randf_range(-60, 60), 0), "ang": randf_range(-0.4, 0.4), "giro": 0.0, "clave": clave}
		o["indice"] = i
		var tex: TextureRect = o["nodo"].get_node("Icono")
		tex.texture = inv[i]["icono"]
		(o["nodo"].get_node("Cantidad") as Label).text = str(inv[i]["cantidad"]) if int(inv[i]["cantidad"]) > 1 else ""
		o["nodo"].set_meta("indice", i)
		nuevos.append(o)
	for o in previas.values():
		o["nodo"].queue_free()
	objetos = nuevos
	cogido = -1
	resaltado_saco = -1
	# Carga en el borde del saco
	var peso: float = pm.call("PesoInventario", heroe)
	var cap: float = pm.call("CapacidadInventario", heroe)
	barra_carga.max_value = maxf(cap, 0.1)
	barra_carga.value = peso
	barra_carga.add_theme_stylebox_override("fill", Estilo.caja("barra_vida" if peso >= cap * 0.9 else "barra_pa"))
	barra_carga.tooltip_text = "Carga: %.1f / %.1f kg (lo equipado no pesa)" % [peso, cap]
	lbl_carga.text = "%.0f/%.0f\nkg" % [peso, cap]

func _nodo_objeto() -> Control:
	var c := Control.new()
	c.size = Vector2(TAM_OBJETO, TAM_OBJETO)
	c.pivot_offset = c.size * 0.5
	c.mouse_filter = Control.MOUSE_FILTER_STOP
	var t := TextureRect.new()
	t.name = "Icono"
	t.set_anchors_preset(Control.PRESET_FULL_RECT)
	t.expand_mode = TextureRect.EXPAND_IGNORE_SIZE
	t.stretch_mode = TextureRect.STRETCH_KEEP_ASPECT_CENTERED
	t.texture_filter = CanvasItem.TEXTURE_FILTER_NEAREST
	t.mouse_filter = Control.MOUSE_FILTER_IGNORE
	c.add_child(t)
	var l := Estilo.etiqueta("", "pequeno", "hueso")
	l.name = "Cantidad"
	l.position = Vector2(TAM_OBJETO - 16, TAM_OBJETO - 18)
	l.add_theme_constant_override("outline_size", 4)
	l.mouse_filter = Control.MOUSE_FILTER_IGNORE
	c.add_child(l)
	c.mouse_entered.connect(func(): _resaltar_saco(c, true))
	c.mouse_exited.connect(func(): _resaltar_saco(c, false))
	c.gui_input.connect(func(ev): _al_tocar_objeto(c, ev))
	return c

func _resaltar_saco(c: Control, si: bool) -> void:
	(c.get_node("Icono") as TextureRect).material = material_resalte if si else null

func _entrada_de(c: Control) -> int:
	for i in objetos.size():
		if objetos[i]["nodo"] == c:
			return i
	return -1

func _al_tocar_objeto(c: Control, ev: InputEvent) -> void:
	if not (ev is InputEventMouseButton and ev.pressed):
		return
	var i := _entrada_de(c)
	if i < 0:
		return
	var indice: int = objetos[i]["indice"]
	var clave := {"origen": "inv", "indice": indice}
	var o := _objeto_de(clave)
	if ev.button_index == MOUSE_BUTTON_LEFT:
		if ev.double_click:
			cogido = -1
			if o is EquipoData:
				_equipar(indice)
			elif o and not o.efectos_al_usar.is_empty():
				_usar(indice)
			return
		cogido = i
		desfase = interior.get_local_mouse_position() - objetos[i]["pos"]
		pulsado_en = ev.global_position
		c.move_to_front()
		_cerrar_bocadillo()
	elif ev.button_index == MOUSE_BUTTON_RIGHT:
		seleccion = clave
		_abrir_menu(clave, o)

# Al soltar un objeto del saco: ¿dónde ha caído?
func _soltar_del_saco(pos: Vector2) -> void:
	var i := cogido
	cogido = -1
	if i < 0 or i >= objetos.size():
		return
	var indice: int = objetos[i]["indice"]
	var o := _objeto_de({"origen": "inv", "indice": indice})
	# Un clic sin mover = ver qué es
	if pos.distance_to(pulsado_en) <= UMBRAL_ARRASTRE:
		objetos[i]["vel"] = Vector2.ZERO
		_mostrar_bocadillo({"origen": "inv", "indice": indice}, objetos[i]["nodo"].get_global_rect().get_center())
		return
	if muneco.get_global_rect().has_point(pos):
		objetos[i]["vel"] = Vector2.ZERO
		if o is EquipoData:
			_equipar(indice)
		elif o and not o.efectos_al_usar.is_empty():
			_usar(indice)
		else:
			_resultado(o.nombre if o else "?", "no se puede poner ni usar")
		return
	for r in retratos.get_children():
		if r.get_global_rect().has_point(pos) and str(r.name) != heroe:
			_dar(indice, str(r.name))
			return
	if not marco.get_global_rect().has_point(pos):
		pm.call("QuitarObjeto", heroe, indice, 1)
		_resultado("Tirar " + (o.nombre if o else "?"), "")
		return
	# Dentro del saco: se queda donde cae, con la inercia del lanzamiento
	objetos[i]["vel"] = objetos[i]["vel"].limit_length(900.0)

# ==========================================
# EL HÉROE (lo puesto)
# ==========================================

# Qué pieza puesta hay bajo el ratón (la de más arriba que tenga un píxel opaco ahí)
func _puesto_en(local: Vector2) -> String:
	var px := Vector2i(int(local.x / ESCALA_HEROE), int(local.y / ESCALA_HEROE) - (_bajada(_frame(_nodo(heroe))) if fase_idle == 1 else 0))
	if px.x < 0 or px.y < 0 or px.x >= 16 or px.y >= 16:
		return ""
	var orden: Array = CAPAS_DELANTE.duplicate()
	orden.reverse()
	orden.append_array(CAPAS_DETRAS)
	for ranura in orden:
		var o := _equipado(ranura)
		if o is EquipoData and o.sprite_puesto:
			var im := _imagen(o.sprite_puesto)
			if im and im.get_pixelv(px).a > 0.1:
				return ranura
	return ""

func _resaltar_puesto(ranura: String) -> void:
	if ranura == bajo_raton:
		return
	bajo_raton = ranura
	figura.tooltip_text = (_equipado(ranura).nombre_completo() if ranura != "" else "")
	_pintar_heroe()

func _al_tocar_figura(ev: InputEvent) -> void:
	if ev is InputEventMouseMotion:
		if arrastrando_puesto == "":
			_resaltar_puesto(_puesto_en(ev.position))
		return
	if not (ev is InputEventMouseButton and ev.pressed):
		return
	var ranura := _puesto_en(ev.position)
	if ranura == "":
		_cerrar_bocadillo()
		return
	var clave := {"origen": "eq", "ranura": ranura}
	if ev.button_index == MOUSE_BUTTON_LEFT:
		if ev.double_click:
			_quitar(ranura)
			return
		pulsado_en = ev.global_position
		_mostrar_bocadillo(clave, ev.global_position)
	elif ev.button_index == MOUSE_BUTTON_RIGHT:
		_abrir_menu(clave, _equipado(ranura))

func _soltar_puesto(pos: Vector2) -> void:
	var ranura := arrastrando_puesto
	arrastrando_puesto = ""
	fantasma.visible = false
	if saco.get_global_rect().has_point(pos):
		_quitar(ranura)

# ==========================================
# BOCADILLO Y MENÚ
# ==========================================

func _mostrar_bocadillo(clave: Dictionary, apunta: Vector2) -> void:
	var o := _objeto_de(clave)
	if o == null:
		_cerrar_bocadillo()
		return
	seleccion = clave
	var t := Textos.detalle(o)
	if clave.get("origen", "") == "inv":
		var comp := Textos.comparar(o, _equipado_para(o))
		if comp != "":
			t += "\n" + comp
	t += "\n[color=#5a5c6b][i]%s[/i][/color]" % ("Doble clic o arrástralo al saco para quitártelo" if clave["origen"] == "eq" else "Arrástralo al héroe para ponértelo o usarlo")
	# Sobre el papel claro del bocadillo, los colores claros de la interfaz no se leen: tinta oscura
	for par in [[Textos.GRIS, "4a4c5a"], [Textos.VERDE, "3a7322"], [Textos.ROJO, "a3322a"]]:
		t = t.replace(par[0], par[1])
	det_texto.text = t
	bocadillo.reset_size()
	bocadillo.visible = true
	# Encima del objeto y un poco a la derecha, sin salirse de la pantalla
	var pantalla := bocadillo.get_viewport().get_visible_rect().size
	var pos := apunta + Vector2(30, -bocadillo.size.y - 34)
	pos.x = clampf(pos.x, 8, pantalla.x - bocadillo.size.x - 8)
	pos.y = clampf(pos.y, 8, pantalla.y - bocadillo.size.y - 8)
	bocadillo.position = pos
	var base := Vector2(clampf(apunta.x + 6, pos.x + 16, pos.x + bocadillo.size.x - 30), pos.y + bocadillo.size.y - 2)
	if pos.y > apunta.y:
		base.y = pos.y + 2
	cola.polygon = PackedVector2Array([base, base + Vector2(22, 0), apunta + Vector2(4, -8)])
	cola.visible = true

func _cerrar_bocadillo() -> void:
	bocadillo.visible = false
	cola.visible = false
	seleccion = {}

func _abrir_menu(clave: Dictionary, o: ObjetoData) -> void:
	if o == null:
		return
	menu.clear()
	menu_acciones.clear()
	var add := func(texto: String, accion: Callable):
		var id := menu.item_count
		menu.add_item(texto, id)
		menu_acciones[id] = accion
	if clave["origen"] == "eq":
		add.call("Quitar", func(): _quitar(clave["ranura"]))
	else:
		var indice := int(clave["indice"])
		if not o.efectos_al_usar.is_empty():
			add.call("Usar", func(): _usar(indice))
		if o is EquipoData:
			add.call("Equipar", func(): _equipar(indice))
		for h in _heroes():
			var n := str(h.name)
			if n != heroe and bool(pm.call("PuedenIntercambiar", heroe, n)):
				add.call("Dar a %s" % h.get_meta("nombre_display", n), func(): _dar(indice, n))
		menu.add_separator()
		add.call("Tirar", func():
			pm.call("QuitarObjeto", heroe, indice, 1)
			_resultado("Tirar " + o.nombre, ""))
	menu.position = Vector2i(menu.get_viewport().get_mouse_position())
	menu.reset_size()
	menu.popup()

# ==========================================
# ACCIONES
# ==========================================

func _equipar(indice: int) -> void:
	var inv := _inventario()
	var nombre: String = inv[indice]["nombre"] if indice < inv.size() else "?"
	_cerrar_bocadillo()
	_resultado("Equipar " + nombre, pm.call("Equipar", heroe, indice))

func _quitar(ranura: String) -> void:
	var o := _equipado(ranura)
	_cerrar_bocadillo()
	bajo_raton = ""
	_resultado("Quitar " + (o.nombre if o else ""), pm.call("Desequipar", heroe, ranura))

func _usar(indice: int) -> void:
	var inv := _inventario()
	var nombre: String = inv[indice]["nombre"] if indice < inv.size() else "?"
	_cerrar_bocadillo()
	_resultado("Usar " + nombre, pm.call("UsarObjeto", heroe, indice))

func _dar(indice: int, destino: String) -> void:
	var inv := _inventario()
	var nombre: String = inv[indice]["nombre"] if indice < inv.size() else "?"
	_cerrar_bocadillo()
	_resultado("Dar %s a %s" % [nombre, destino], pm.call("DarObjeto", heroe, indice, destino, 1))

# Para los tests: el nodo del saco que representa la entrada 'indice' del inventario
func nodo_de_objeto(indice: int) -> Control:
	for o in objetos:
		if o["indice"] == indice:
			return o["nodo"]
	return null
