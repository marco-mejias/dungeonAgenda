extends Control

# Minimapa de lo explorado (esquina superior derecha). Gira con la cámara para que
# "arriba" sea siempre hacia donde mira. M lo oculta/muestra. Clic = llevar la cámara allí.

const GRID := 35
const LADO := 132.0
const ZOOM := 2.2 # el mapa entero no cabe a buen tamaño: vemos la zona del héroe activo
# Códigos que devuelve DungeonManager.ObtenerMinimapa()
const COLORES := {
	0: Color(0, 0, 0, 0),             # sin explorar
	1: Color(0.22, 0.18, 0.28, 0.95), # suelo explorado
	2: Color(0.42, 0.36, 0.5, 1.0),   # suelo a la vista
	3: Color(0.62, 0.42, 0.32, 1.0),  # muro
	4: Color(0.75, 0.5, 0.2, 1.0),    # puerta cerrada
	5: Color(1.0, 0.82, 0.25, 1.0),   # cofre
	6: Color(0.3, 0.6, 1.0, 1.0),     # escalera
	7: Color(1.0, 0.25, 0.2, 1.0),    # enemigo a la vista
	8: Color(0.55, 0.85, 1.0, 1.0),   # héroe
	9: Color(1.0, 1.0, 1.0, 1.0),     # héroe activo
	10: Color(1.0, 0.55, 0.15, 1.0),  # brasero
	11: Color(1.0, 0.3, 0.9, 1.0),    # evento
}

var imagen: Image
var textura: ImageTexture
var mapa: TextureRect
var t_refresco := 0.0
var m_prev := false
var yaw_actual := 45.0

func _ready() -> void:
	mouse_filter = Control.MOUSE_FILTER_IGNORE
	clip_contents = true
	custom_minimum_size = Vector2(LADO + 24, LADO + 24)
	size = custom_minimum_size
	imagen = Image.create_empty(GRID, GRID, false, Image.FORMAT_RGBA8)
	textura = ImageTexture.create_from_image(imagen)

	var marco := Panel.new()
	marco.name = "Marco"
	var s := StyleBoxFlat.new()
	s.bg_color = Color(0.02, 0.02, 0.06, 0.75)
	s.set_border_width_all(2)
	s.border_color = Color(0.55, 0.6, 0.85)
	s.set_corner_radius_all(int(size.x * 0.5))
	s.corner_detail = 6
	marco.add_theme_stylebox_override("panel", s)
	marco.size = size
	marco.mouse_filter = Control.MOUSE_FILTER_IGNORE
	add_child(marco)

	# Máscara circular: los hijos solo se dibujan donde se dibuja este panel
	var mascara := Panel.new()
	mascara.name = "Mascara"
	var sm := StyleBoxFlat.new()
	sm.bg_color = Color(1, 1, 1, 1)
	sm.set_corner_radius_all(int(size.x * 0.5) - 3)
	sm.corner_detail = 8
	mascara.add_theme_stylebox_override("panel", sm)
	mascara.position = Vector2(3, 3)
	mascara.size = size - Vector2(6, 6)
	mascara.clip_children = CanvasItem.CLIP_CHILDREN_ONLY
	mascara.mouse_filter = Control.MOUSE_FILTER_IGNORE
	add_child(mascara)

	mapa = TextureRect.new()
	mapa.name = "Mapa"
	mapa.texture = textura
	mapa.texture_filter = CanvasItem.TEXTURE_FILTER_NEAREST
	mapa.expand_mode = TextureRect.EXPAND_IGNORE_SIZE
	mapa.size = Vector2(LADO, LADO) * ZOOM
	mapa.mouse_filter = Control.MOUSE_FILTER_STOP
	mapa.gui_input.connect(_on_mapa_input)
	mascara.add_child(mapa)

func _process(delta: float) -> void:
	var m := Input.is_physical_key_pressed(KEY_M)
	if m and not m_prev:
		visible = not visible
	m_prev = m
	if not visible:
		return

	var dm := get_node_or_null("/root/Main/ContenedorMapa/SubViewport/DungeonManager")
	if not dm:
		return

	# Centrado en lo que mira la cámara y girado para que su dirección quede hacia arriba
	var cam := dm.get_node_or_null("Camera3D")
	if cam:
		var yaw: float = cam.get_meta("yaw", 45.0)
		yaw_actual = lerp(yaw_actual, yaw, min(1.0, 10.0 * delta))
		var y := deg_to_rad(yaw_actual)
		mapa.rotation = -PI * 0.5 - atan2(-cos(y), -sin(y))
		var pivote: Vector3 = cam.get_meta("pivote", Vector3(GRID * 0.5, 0, GRID * 0.5))
		mapa.pivot_offset = Vector2(pivote.x, pivote.z) / GRID * mapa.size
		mapa.position = mapa.get_parent().size * 0.5 - mapa.pivot_offset

	t_refresco -= delta
	if t_refresco > 0.0:
		return
	t_refresco = 0.2
	var datos: PackedByteArray = dm.call("ObtenerMinimapa")
	if datos.size() < GRID * GRID:
		return
	for x in GRID:
		for yy in GRID:
			imagen.set_pixel(x, yy, COLORES.get(datos[x * GRID + yy], Color(0, 0, 0, 0)))
	textura.update(imagen)

func _on_mapa_input(ev: InputEvent) -> void:
	if ev is InputEventMouseButton and ev.pressed and ev.button_index == MOUSE_BUTTON_LEFT:
		var local := mapa.get_local_mouse_position()
		var celda := local / mapa.size * GRID
		var dm := get_node_or_null("/root/Main/ContenedorMapa/SubViewport/DungeonManager")
		var cam: Node = dm.get_node_or_null("Camera3D") if dm else null
		if cam and celda.x >= 0 and celda.y >= 0 and celda.x < GRID and celda.y < GRID:
			cam.set_meta("ir_a", Vector3(celda.x, 0, celda.y))
		accept_event()
