extends Control

var combat_manager = null
var token_to_follow = null
var escala_ui = 0.2

func _ready():
	visible = false
	
	var map = get_node_or_null("/root/Main/ContenedorMapa/SubViewport/DungeonManager")
	if map:
		map.set_meta("raton_en_ui", true)
		combat_manager = map.get_node_or_null("CombatManager")
		
		var pm = get_node_or_null("/root/Main/PartyManager")
		if pm:
			var act_char = pm.call("GetActiveCharacter")
			if act_char:
				token_to_follow = map.get_node_or_null(str(act_char.name))
	
	_actualizar_transformacion()
	visible = true

func _process(_delta):
	_actualizar_transformacion()

func _actualizar_transformacion():
	if not token_to_follow: return
	
	var mapa_viewport = token_to_follow.get_viewport()
	var contenedor = get_node_or_null("/root/Main/ContenedorMapa")
	
	if mapa_viewport and contenedor:
		var xform = mapa_viewport.canvas_transform
		var pos_en_viewport = xform * token_to_follow.global_position
		
		# Ratio de estiramiento del SubViewport
		var ratio_x = contenedor.size.x / mapa_viewport.size.x
		var ratio_y = contenedor.size.y / mapa_viewport.size.y
		
		# ¡NUEVO! Como somos hijos del contenedor, la posición es 100% LOCAL a él
		var pos_local_x = pos_en_viewport.x * ratio_x
		var pos_local_y = pos_en_viewport.y * ratio_y
		
		var zoom_camara = xform.get_scale()
		scale = zoom_camara * escala_ui
		
		var desvio_x = 40 * zoom_camara.x * ratio_x * escala_ui
		var desvio_y = -40 * zoom_camara.y * ratio_y * escala_ui
		
		position = Vector2(pos_local_x + desvio_x, pos_local_y + desvio_y)

# ==========================================
# DEJA AQUÍ TUS FUNCIONES _on_btn_atacar_pressed(), _input(), _notification(), etc...
# ==========================================


func _on_btn_atacar_pressed() -> void:
	print("[UI] El Menú Táctico: Ataque presionado.")
	if combat_manager:
		combat_manager.call("OnBotonAtacar")
	queue_free()


func _on_btn_habilidad_pressed() -> void:
	if combat_manager:
		combat_manager.call("OnBotonHabilidad")
	queue_free()


func _on_btn_pasar_pressed() -> void:
	if combat_manager:
		combat_manager.call("OnBotonPasar")
	queue_free()

func _input(event):
	# Solo clic izquierdo
	if event is InputEventMouseButton and event.pressed and event.button_index == MOUSE_BUTTON_LEFT:
		print("[UI DEBUG] El menú táctico ha detectado un clic en: ", event.position)
		
func _notification(what):
	if what == NOTIFICATION_PREDELETE:
		var map = get_node_or_null("/root/Main/ContenedorMapa/SubViewport/DungeonManager")
		if map:
			map.set_meta("raton_en_ui", false)

func _on_mouse_entered():
	var map = get_node_or_null("/root/Main/ContenedorMapa/SubViewport/DungeonManager")
	if map:
		map.set_meta("raton_en_ui", true)

func _on_mouse_exited():
	var map = get_node_or_null("/root/Main/ContenedorMapa/SubViewport/DungeonManager")
	if map:
		map.set_meta("raton_en_ui", false)
