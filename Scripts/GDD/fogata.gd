extends Control

# Pantalla de descanso entre pisos: hoguera animada (partículas pixeladas) y el grupo
# sentado alrededor con su animación idle. Se monta sola al hacerse visible.

const HOJA := preload("res://Assets/2D Pixel Dungeon Asset Pack/character and tileset/Dungeon_Character_2.png")
const PAREJAS := {0: 1, 1: 0, 2: 3, 3: 2, 4: 5, 5: 4, 6: 5}
const CENTRO := Vector2(576, 372)

var resplandor: TextureRect
var sentados: Array[TextureRect] = []
var t := 0.0

@onready var boton: Button = $BtnDescansar
@onready var fondo: ColorRect = $Fondo
@onready var titulo: Label = $Label

func _ready() -> void:
	texture_filter = CanvasItem.TEXTURE_FILTER_NEAREST
	move_child(fondo, 0)
	fondo.color = Color(0.025, 0.018, 0.04, 0.97)

	titulo.text = "LA HOGUERA"
	titulo.add_theme_font_size_override("font_size", 30)
	titulo.add_theme_color_override("font_color", Color(1.0, 0.8, 0.35))
	titulo.offset_left = -300
	titulo.offset_right = 300
	titulo.offset_top = 40
	titulo.offset_bottom = 84
	titulo.horizontal_alignment = HORIZONTAL_ALIGNMENT_CENTER

	var sub := Label.new()
	sub.name = "Recuperacion"
	sub.text = "El grupo recupera fuerzas:  +10 PV   +3 hambre   +50 cordura"
	sub.add_theme_font_size_override("font_size", 13)
	sub.add_theme_color_override("font_color", Color(0.85, 0.8, 0.95))
	sub.horizontal_alignment = HORIZONTAL_ALIGNMENT_CENTER
	sub.set_anchors_and_offsets_preset(Control.PRESET_CENTER_TOP)
	sub.offset_left = -300
	sub.offset_right = 300
	sub.offset_top = 90
	sub.offset_bottom = 112
	add_child(sub)

	_crear_resplandor()
	_crear_hoguera()

	# Botón abajo, con estilo de ventana PS1
	boton.anchor_top = 1.0
	boton.anchor_bottom = 1.0
	boton.offset_top = -96
	boton.offset_bottom = -52
	boton.focus_mode = Control.FOCUS_NONE
	for estado in ["normal", "hover", "pressed"]:
		var s := StyleBoxFlat.new()
		s.bg_color = Color(0.07, 0.08, 0.22) if estado == "normal" else Color(0.14, 0.12, 0.3)
		s.set_border_width_all(2)
		s.border_color = Color(0.4, 0.45, 0.7) if estado == "normal" else Color(1, 0.8, 0.3)
		s.corner_detail = 1
		boton.add_theme_stylebox_override(estado, s)
	boton.add_theme_color_override("font_hover_color", Color(1, 0.85, 0.35))
	move_child(boton, get_child_count() - 1)

	var capa := get_parent() as CanvasLayer
	if capa:
		capa.visibility_changed.connect(_al_mostrarse)

func _crear_resplandor() -> void:
	var g := Gradient.new()
	g.set_color(0, Color(1.0, 0.55, 0.2, 0.55))
	g.set_color(1, Color(1.0, 0.3, 0.1, 0.0))
	var tex := GradientTexture2D.new()
	tex.gradient = g
	tex.fill = GradientTexture2D.FILL_RADIAL
	tex.fill_from = Vector2(0.5, 0.5)
	tex.fill_to = Vector2(0.5, 0.0)
	tex.width = 64
	tex.height = 64
	resplandor = TextureRect.new()
	resplandor.name = "Resplandor"
	resplandor.texture = tex
	resplandor.size = Vector2(520, 520)
	resplandor.pivot_offset = resplandor.size * 0.5
	resplandor.position = CENTRO - resplandor.size * 0.5
	resplandor.mouse_filter = Control.MOUSE_FILTER_IGNORE
	add_child(resplandor)

func _crear_hoguera() -> void:
	# Troncos cruzados
	for ang in [-0.35, 0.35]:
		var tronco := ColorRect.new()
		tronco.color = Color(0.32, 0.18, 0.1)
		tronco.size = Vector2(70, 12)
		tronco.pivot_offset = tronco.size * 0.5
		tronco.position = CENTRO + Vector2(-35, 18)
		tronco.rotation = ang
		tronco.mouse_filter = Control.MOUSE_FILTER_IGNORE
		add_child(tronco)

	var rampa := Gradient.new()
	rampa.offsets = PackedFloat32Array([0.0, 0.3, 0.7, 1.0])
	rampa.colors = PackedColorArray([Color(1, 0.95, 0.6), Color(1, 0.65, 0.15), Color(0.85, 0.2, 0.05), Color(0.3, 0.05, 0.05, 0.0)])

	var fuego := CPUParticles2D.new()
	fuego.name = "Fuego"
	fuego.position = CENTRO + Vector2(0, 14)
	fuego.amount = 70
	fuego.lifetime = 0.8
	fuego.emission_shape = CPUParticles2D.EMISSION_SHAPE_RECTANGLE
	fuego.emission_rect_extents = Vector2(18, 3)
	fuego.direction = Vector2(0, -1)
	fuego.spread = 12.0
	fuego.gravity = Vector2(0, -90)
	fuego.initial_velocity_min = 25.0
	fuego.initial_velocity_max = 60.0
	fuego.scale_amount_min = 5.0
	fuego.scale_amount_max = 9.0
	fuego.color_ramp = rampa
	add_child(fuego)

	var chispas := CPUParticles2D.new()
	chispas.name = "Chispas"
	chispas.position = CENTRO
	chispas.amount = 18
	chispas.lifetime = 2.2
	chispas.emission_shape = CPUParticles2D.EMISSION_SHAPE_RECTANGLE
	chispas.emission_rect_extents = Vector2(14, 2)
	chispas.direction = Vector2(0, -1)
	chispas.spread = 35.0
	chispas.gravity = Vector2(0, -30)
	chispas.initial_velocity_min = 40.0
	chispas.initial_velocity_max = 90.0
	chispas.scale_amount_min = 2.0
	chispas.scale_amount_max = 3.0
	chispas.color = Color(1.0, 0.75, 0.3)
	add_child(chispas)

func _al_mostrarse() -> void:
	if not get_parent().visible:
		return
	for s in sentados:
		s.queue_free()
	sentados.clear()

	var pm := get_node_or_null("/root/Main/PartyManager")
	var dm := get_node_or_null("/root/Main/ContenedorMapa/SubViewport/DungeonManager")
	if not pm:
		return
	var vivos := []
	for h in pm.get_children():
		if h.name != "DatosEquipo" and not h.get_meta("esta_muerto", false):
			vivos.append(h)

	# Sentados en semicírculo alrededor del fuego
	for i in vivos.size():
		var h: Node = vivos[i]
		var frame := 0
		if dm:
			var cuerpo := dm.get_node_or_null(NodePath(str(h.name) + "/Cuerpo"))
			if cuerpo:
				frame = cuerpo.get_meta("frame_a", cuerpo.frame)
		var ang: float = PI * (0.15 + 0.7 * (float(i) / maxf(1.0, vivos.size() - 1))) if vivos.size() > 1 else PI * 0.5
		var pos := CENTRO + Vector2(cos(ang + PI) * 150.0, -sin(ang + PI) * 40.0 + 10.0)

		var at := AtlasTexture.new()
		at.atlas = HOJA
		at.region = Rect2((frame % 7) * 16, (frame / 7) * 16, 16, 16)
		var r := TextureRect.new()
		r.texture = at
		r.size = Vector2(64, 64)
		r.position = pos - Vector2(32, 52)
		r.flip_h = pos.x > CENTRO.x
		r.mouse_filter = Control.MOUSE_FILTER_IGNORE
		r.set_meta("frame", frame)
		add_child(r)
		move_child(r, boton.get_index())
		sentados.append(r)

func _process(delta: float) -> void:
	if not get_parent().visible:
		return
	t += delta
	if resplandor:
		var f := 1.0 + 0.06 * sin(t * 9.0) + 0.04 * sin(t * 23.0)
		resplandor.scale = Vector2(f, f)
		resplandor.modulate.a = 0.85 + 0.15 * sin(t * 13.0)
	# Idle de los sentados (alternan sus dos frames)
	var fase := int(t * 1.8) % 2
	for i in sentados.size():
		var r := sentados[i]
		var base: int = r.get_meta("frame")
		var frame: int = base if (fase + i) % 2 == 0 else PAREJAS.get(base, base)
		(r.texture as AtlasTexture).region = Rect2((frame % 7) * 16, (frame / 7) * 16, 16, 16)
