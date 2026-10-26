extends Node

# Game feel centralizado (autoload "Feel"): pausa al golpear, sacudida de cámara,
# golpe de zoom, destellos de pantalla y botín que vuela hasta la ficha del héroe.
# Desde C++ se usa con get_node("/root/Feel")->call("sacudir", 0.4), etc.

const HOJA_TILES := preload("res://Assets/2D Pixel Dungeon Asset Pack/character and tileset/Dungeon_Tileset.png")
const RUTA_DUNGEON := "/root/Main/ContenedorMapa/SubViewport/DungeonManager"

var nivel := 1 # 0 = sin efectos, 1 = sutil, 2 = fuerte (opción del menú de pausa)
var trauma := 0.0
var capa: CanvasLayer
var destello: ColorRect

func _ready() -> void:
	process_mode = Node.PROCESS_MODE_ALWAYS
	capa = CanvasLayer.new()
	capa.layer = 15
	add_child(capa)
	destello = ColorRect.new()
	destello.set_anchors_preset(Control.PRESET_FULL_RECT)
	destello.mouse_filter = Control.MOUSE_FILTER_IGNORE
	destello.color = Color(1, 1, 1, 0)
	capa.add_child(destello)

func _process(delta: float) -> void:
	trauma = maxf(0.0, trauma - delta * 1.8)
	var cam := get_node_or_null(RUTA_DUNGEON + "/Camera3D")
	if cam:
		var fuerza := trauma * trauma * (0.22 if nivel == 1 else 0.45)
		cam.set_meta("sacudida", Vector2(randf_range(-1, 1), randf_range(-1, 1)) * fuerza)

# --- API ---

func sacudir(cantidad: float) -> void:
	if nivel == 0:
		return
	trauma = minf(1.0, trauma + cantidad)

func pausa_impacto(segundos: float) -> void:
	if nivel == 0 or Engine.time_scale < 0.5:
		return
	Engine.time_scale = 0.05
	await get_tree().create_timer(segundos * (1.0 if nivel == 1 else 1.5), true, false, true).timeout
	Engine.time_scale = 1.0

# Acercamiento brusco de la cámara que vuelve solo (críticos)
func golpe_camara(fuerza: float) -> void:
	if nivel == 0:
		return
	var cam := get_node_or_null(RUTA_DUNGEON + "/Camera3D")
	if cam:
		cam.set_meta("punch", fuerza * (1.0 if nivel == 1 else 1.6))

func destello_pantalla(color: Color, duracion: float = 0.3) -> void:
	destello.color = color
	var tw := create_tween()
	tw.tween_property(destello, "color:a", 0.0, duracion)

# Un icono del tileset (región) sale del punto 3D y vuela hasta la ficha del héroe
func volar_a_ficha(desde: Vector3, heroe: String, region: Rect2, texto: String) -> void:
	var cam: Camera3D = get_node_or_null(RUTA_DUNGEON + "/Camera3D")
	var cont: SubViewportContainer = get_node_or_null("/root/Main/ContenedorMapa")
	if not cam or not cont or cam.is_position_behind(desde):
		return
	var origen := cont.global_position + cam.unproject_position(desde) * cont.stretch_shrink

	var icono := TextureRect.new()
	var at := AtlasTexture.new()
	at.atlas = HOJA_TILES
	at.region = region
	icono.texture = at
	icono.texture_filter = CanvasItem.TEXTURE_FILTER_NEAREST
	icono.expand_mode = TextureRect.EXPAND_IGNORE_SIZE
	icono.size = Vector2(28, 28)
	icono.pivot_offset = icono.size * 0.5
	icono.position = origen - icono.size * 0.5
	icono.mouse_filter = Control.MOUSE_FILTER_IGNORE
	capa.add_child(icono)

	var etiqueta := Label.new()
	etiqueta.text = texto
	etiqueta.add_theme_font_size_override("font_size", 16)
	etiqueta.add_theme_color_override("font_color", Color(1, 0.85, 0.3))
	etiqueta.add_theme_color_override("font_outline_color", Color(0.05, 0.02, 0.0))
	etiqueta.add_theme_constant_override("outline_size", 5)
	etiqueta.position = origen + Vector2(-20, -40)
	etiqueta.mouse_filter = Control.MOUSE_FILTER_IGNORE
	capa.add_child(etiqueta)

	var ficha: Control = get_node_or_null("/root/Main/CanvasLayer/PanelFichas/" + heroe)
	var destino := Vector2(40, 70)
	if ficha and ficha.is_visible_in_tree():
		destino = ficha.global_position + Vector2(22, 22)

	var tw := create_tween()
	# Salta hacia arriba y luego vuela a la ficha
	tw.tween_property(icono, "position:y", icono.position.y - 36, 0.22).set_trans(Tween.TRANS_QUAD).set_ease(Tween.EASE_OUT)
	tw.parallel().tween_property(icono, "scale", Vector2(1.4, 1.4), 0.22)
	tw.tween_property(icono, "position", destino - icono.size * 0.5, 0.55).set_trans(Tween.TRANS_QUAD).set_ease(Tween.EASE_IN)
	tw.parallel().tween_property(icono, "scale", Vector2(0.7, 0.7), 0.55)
	tw.tween_callback(icono.queue_free)
	tw.tween_callback(_latido_ficha.bind(heroe))

	var tw2 := create_tween()
	tw2.tween_property(etiqueta, "position:y", etiqueta.position.y - 24, 0.9).set_trans(Tween.TRANS_QUAD).set_ease(Tween.EASE_OUT)
	tw2.parallel().tween_property(etiqueta, "modulate:a", 0.0, 0.5).set_delay(0.5)
	tw2.tween_callback(etiqueta.queue_free)

	var audio := get_node_or_null("/root/AudioManager")
	if audio:
		audio.play_ui("open")

func _latido_ficha(heroe: String) -> void:
	var ficha: Control = get_node_or_null("/root/Main/CanvasLayer/PanelFichas/" + heroe)
	if not ficha:
		return
	ficha.pivot_offset = ficha.size * 0.5
	var tw := create_tween()
	tw.tween_property(ficha, "scale", Vector2(1.06, 1.06), 0.08)
	tw.tween_property(ficha, "scale", Vector2.ONE, 0.15)

# La ficha tiembla (p.ej. al intentar algo sin PA)
func sacudir_ficha(heroe: String) -> void:
	var ficha: Control = get_node_or_null("/root/Main/CanvasLayer/PanelFichas/" + heroe)
	if not ficha or ficha.has_meta("temblando"):
		return
	ficha.set_meta("temblando", true)
	var x0 := ficha.position.x
	var tw := create_tween()
	for d in [6.0, -5.0, 4.0, -3.0, 0.0]:
		tw.tween_property(ficha, "position:x", x0 + d, 0.04)
	tw.tween_callback(ficha.remove_meta.bind("temblando"))
