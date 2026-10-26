extends Control

# Dado 3D low poly (icosaedro de caras planas) para las tiradas de los eventos.
# Se dibuja en un SubViewport pequeño y pixelado dentro de la pantalla de evento.
#   tirar(resultado, exito, duracion)  /  ocultar()

const RESOLUCION := 72
const TONOS := [Color(0.93, 0.89, 0.78), Color(0.82, 0.76, 0.64), Color(0.7, 0.63, 0.52)]

var viewport: SubViewport
var dado: MeshInstance3D
var numero: Label
var sello: Label
var girando := false
var velocidad := 0.0
var eje := Vector3(1, 1, 0).normalized()

func _ready() -> void:
	mouse_filter = Control.MOUSE_FILTER_IGNORE
	visible = false

	var contenedor := SubViewportContainer.new()
	contenedor.stretch = true
	contenedor.stretch_shrink = 3
	contenedor.texture_filter = CanvasItem.TEXTURE_FILTER_NEAREST
	contenedor.set_anchors_preset(Control.PRESET_FULL_RECT)
	contenedor.mouse_filter = Control.MOUSE_FILTER_IGNORE
	add_child(contenedor)

	viewport = SubViewport.new()
	viewport.own_world_3d = true
	viewport.transparent_bg = true
	viewport.size = Vector2i(RESOLUCION, RESOLUCION)
	contenedor.add_child(viewport)

	var cam := Camera3D.new()
	cam.fov = 30
	cam.position = Vector3(0, 0, 5.2)
	viewport.add_child(cam)

	var luz := DirectionalLight3D.new()
	luz.rotation = Vector3(deg_to_rad(-40), deg_to_rad(30), 0)
	luz.light_energy = 1.3
	viewport.add_child(luz)
	var env := WorldEnvironment.new()
	env.environment = Environment.new()
	env.environment.background_mode = Environment.BG_CLEAR_COLOR
	env.environment.ambient_light_source = Environment.AMBIENT_SOURCE_COLOR
	env.environment.ambient_light_color = Color(0.5, 0.45, 0.6)
	viewport.add_child(env)

	dado = MeshInstance3D.new()
	dado.mesh = _icosaedro()
	var mat := StandardMaterial3D.new()
	mat.vertex_color_use_as_albedo = true
	mat.cull_mode = BaseMaterial3D.CULL_DISABLED
	mat.diffuse_mode = BaseMaterial3D.DIFFUSE_LAMBERT
	mat.specular_mode = BaseMaterial3D.SPECULAR_DISABLED
	dado.material_override = mat
	viewport.add_child(dado)

	numero = _etiqueta(40)
	numero.set_anchors_and_offsets_preset(Control.PRESET_FULL_RECT)
	sello = _etiqueta(18)
	sello.set_anchors_and_offsets_preset(Control.PRESET_BOTTOM_WIDE)
	sello.offset_top = -26
	sello.offset_bottom = -4

func _etiqueta(tam: int) -> Label:
	var l := Label.new()
	l.horizontal_alignment = HORIZONTAL_ALIGNMENT_CENTER
	l.vertical_alignment = VERTICAL_ALIGNMENT_CENTER
	l.add_theme_font_size_override("font_size", tam)
	l.add_theme_color_override("font_outline_color", Color(0.02, 0.02, 0.05))
	l.add_theme_constant_override("outline_size", 8)
	l.mouse_filter = Control.MOUSE_FILTER_IGNORE
	l.visible = false
	add_child(l)
	return l

# Icosaedro con caras planas (cada cara su normal y un tono de marfil)
func _icosaedro() -> ArrayMesh:
	var t := (1.0 + sqrt(5.0)) / 2.0
	var v := [
		Vector3(-1, t, 0), Vector3(1, t, 0), Vector3(-1, -t, 0), Vector3(1, -t, 0),
		Vector3(0, -1, t), Vector3(0, 1, t), Vector3(0, -1, -t), Vector3(0, 1, -t),
		Vector3(t, 0, -1), Vector3(t, 0, 1), Vector3(-t, 0, -1), Vector3(-t, 0, 1),
	]
	var caras := [
		[0, 11, 5], [0, 5, 1], [0, 1, 7], [0, 7, 10], [0, 10, 11],
		[1, 5, 9], [5, 11, 4], [11, 10, 2], [10, 7, 6], [7, 1, 8],
		[3, 9, 4], [3, 4, 2], [3, 2, 6], [3, 6, 8], [3, 8, 9],
		[4, 9, 5], [2, 4, 11], [6, 2, 10], [8, 6, 7], [9, 8, 1],
	]
	var st := SurfaceTool.new()
	st.begin(Mesh.PRIMITIVE_TRIANGLES)
	for i in caras.size():
		var c: Array = caras[i]
		var a: Vector3 = v[c[0]].normalized()
		var b: Vector3 = v[c[1]].normalized()
		var d: Vector3 = v[c[2]].normalized()
		var n := (b - a).cross(d - a).normalized()
		if n.dot(a) < 0:
			n = -n
		st.set_color(TONOS[i % TONOS.size()])
		st.set_normal(n)
		for p in [a, b, d]:
			st.add_vertex(p)
	return st.commit()

func tirar(resultado: int, exito: bool, duracion: float = 1.5) -> void:
	visible = true
	numero.visible = false
	sello.visible = false
	girando = true
	eje = Vector3(randf_range(-1, 1), randf_range(-1, 1), randf_range(-0.3, 0.3)).normalized()
	velocidad = 22.0
	dado.scale = Vector3.ONE * 0.6
	var tw := create_tween()
	tw.tween_property(dado, "scale", Vector3.ONE, 0.25).set_trans(Tween.TRANS_BACK).set_ease(Tween.EASE_OUT)
	tw.parallel().tween_property(self, "velocidad", 0.0, duracion).set_trans(Tween.TRANS_QUAD).set_ease(Tween.EASE_OUT)
	tw.tween_callback(_mostrar_resultado.bind(resultado, exito))
	var audio := get_node_or_null("/root/AudioManager")
	if audio:
		audio.play_sfx("dice")

func _mostrar_resultado(resultado: int, exito: bool) -> void:
	girando = false
	var color := Color(0.45, 1.0, 0.5) if exito else Color(1.0, 0.4, 0.35)
	numero.text = str(resultado)
	numero.add_theme_color_override("font_color", color)
	numero.visible = true
	numero.pivot_offset = numero.size * 0.5
	numero.scale = Vector2(1.8, 1.8)
	sello.text = "ÉXITO" if exito else "FALLO"
	sello.add_theme_color_override("font_color", color)
	sello.visible = true
	var tw := create_tween()
	tw.tween_property(numero, "scale", Vector2.ONE, 0.2).set_trans(Tween.TRANS_BACK).set_ease(Tween.EASE_OUT)
	var feel := get_node_or_null("/root/Feel")
	if feel:
		feel.sacudir(0.2)

func ocultar() -> void:
	visible = false
	girando = false

func _process(delta: float) -> void:
	if visible and girando and dado:
		dado.rotate(eje, velocidad * delta)
	elif visible and dado:
		# En reposo flota un poco
		dado.rotate_y(delta * 0.4)
