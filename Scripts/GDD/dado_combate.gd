extends CanvasLayer

# DadoCombate (autoload): el d20 de cada ataque (héroes y enemigos). Un dado pequeño sobre la
# cabeza de quien ataca que solo enseña el número y, si toca, CRÍTICO o FALLO. Sin cuentas.
#   tirar(d20, acierta, sello, _detalle, ficha)  -> el que ataca espera `espera` segundos y lanza el golpe
# 'ficha' es el Node3D de quien tira; sin ella se pone sobre el héroe activo.

const DADO := preload("res://Scripts/GDD/dado3d.gd")
const GIRO := 0.6            # lo que gira el dado antes de pararse
const LECTURA := 0.35        # tiempo para leer el resultado antes del golpe
const TAM := 92.0            # px del dado
const ALTURA := 1.15         # sobre los pies de la ficha (unidades de mundo)
var espera := GIRO + LECTURA # lo lee el CombatManager

var caja: Control
var dado: Control
var ficha: WeakRef = null
var etiqueta_ventaja: Label
var t_ocultar := 0.0

func _ready() -> void:
	layer = 15
	process_mode = Node.PROCESS_MODE_ALWAYS
	caja = Control.new()
	caja.mouse_filter = Control.MOUSE_FILTER_IGNORE
	caja.size = Vector2(TAM, TAM + 24)
	add_child(caja)
	dado = DADO.new()
	dado.size = Vector2(TAM, TAM)
	dado.solo_sellos_especiales = true
	caja.add_child(dado)
	# Más pequeño que el de los eventos: número y sello a escala, el sello debajo del dado
	dado.numero.add_theme_font_size_override("font_size", 30)
	dado.numero.add_theme_constant_override("outline_size", 7)
	dado.sello.add_theme_font_size_override("font_size", 16)
	dado.sello.offset_top = 0
	dado.sello.offset_bottom = 22
	# ▲ VENTAJA / ▼ DESVENTAJA encima del dado (se tiraron dos y cuenta el mejor/peor)
	etiqueta_ventaja = Label.new()
	etiqueta_ventaja.horizontal_alignment = HORIZONTAL_ALIGNMENT_CENTER
	etiqueta_ventaja.position = Vector2(-30, -16)
	etiqueta_ventaja.size = Vector2(TAM + 60, 18)
	etiqueta_ventaja.add_theme_font_size_override("font_size", 13)
	etiqueta_ventaja.add_theme_color_override("font_outline_color", Color(0.02, 0.02, 0.05))
	etiqueta_ventaja.add_theme_constant_override("outline_size", 6)
	etiqueta_ventaja.mouse_filter = Control.MOUSE_FILTER_IGNORE
	caja.add_child(etiqueta_ventaja)
	caja.visible = false

func tirar(d20: int, acierta: int, sello: String, _detalle: String = "", quien: Node3D = null, ventaja: int = 0) -> float:
	var hud := get_tree().get_first_node_in_group("hud")
	if hud:
		var nombre := str(quien.get_meta("nombre_display", quien.name)) if quien else "Tirada"
		var extra := " (ventaja)" if ventaja > 0 else (" (desventaja)" if ventaja < 0 else "")
		hud.registrar("%s: d20 %d%s · %s" % [nombre, d20, extra, sello if sello != "" else ("acierta" if acierta else "falla")], Color(0.85, 0.75, 0.45))
	etiqueta_ventaja.text = "▲ VENTAJA" if ventaja > 0 else ("▼ DESVENTAJA" if ventaja < 0 else "")
	etiqueta_ventaja.add_theme_color_override("font_color", Color(0.55, 1.0, 0.55) if ventaja > 0 else Color(1.0, 0.5, 0.45))
	if quien == null:
		quien = _ficha_activa()
	ficha = weakref(quien) if quien else null
	caja.visible = true
	_colocar()
	dado.tirar(d20, acierta != 0, GIRO, sello)
	t_ocultar = espera + 0.6
	return espera

func esta_visible() -> bool:
	return caja.visible

func _ficha_activa() -> Node3D:
	var pm := get_node_or_null("/root/Main/PartyManager")
	var dm := get_node_or_null("/root/Main/ContenedorMapa/SubViewport/DungeonManager")
	var h = pm.call("GetActiveCharacter") if pm else null
	return dm.get_node_or_null(NodePath(str(h.name))) as Node3D if dm and h else null

# Sobre la cabeza de quien tira (la sigue si se mueve o si la cámara se recoloca)
func _colocar() -> void:
	var f = ficha.get_ref() if ficha else null
	var cont: SubViewportContainer = get_node_or_null("/root/Main/ContenedorMapa")
	var cam: Camera3D = cont.get_child(0).get_camera_3d() if cont and cont.get_child_count() > 0 else null
	if not (f is Node3D) or not is_instance_valid(f) or cam == null:
		caja.position = Vector2(get_viewport().get_visible_rect().size.x * 0.5 - TAM * 0.5, 90)
		return
	var punto: Vector3 = f.global_position + Vector3(0, ALTURA, 0)
	if cam.is_position_behind(punto):
		return
	var p: Vector2 = cont.global_position + cam.unproject_position(punto) * cont.stretch_shrink
	# Siempre dentro de la pantalla
	var pantalla := get_viewport().get_visible_rect().size
	caja.position = (p - Vector2(TAM * 0.5, TAM)).clamp(Vector2(4, 4), pantalla - caja.size - Vector2(4, 4))

func _process(delta: float) -> void:
	if caja.visible:
		_colocar()
	if t_ocultar > 0.0:
		t_ocultar -= delta
		if t_ocultar <= 0.0:
			caja.visible = false
			dado.ocultar()
