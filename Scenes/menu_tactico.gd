extends Control

# Menú contextual anclado al héroe activo. Vive en la capa de HUD (CanvasLayer)
# y se recoloca cada frame proyectando la ficha 3D a la pantalla.

const ESCALA_BASE := 0.5
const OFFSET_PANTALLA := Vector2(26, -60) # a la derecha y por encima de la cabeza

var combat_manager: Node = null
var token_to_follow: Node3D = null
var cam: Camera3D = null
var contenedor: SubViewportContainer = null

func _ready():
	visible = false
	var map = get_node_or_null("/root/Main/ContenedorMapa/SubViewport/DungeonManager")
	contenedor = get_node_or_null("/root/Main/ContenedorMapa")
	if map:
		combat_manager = map.get_node_or_null("CombatManager")
		cam = map.get_node_or_null("Camera3D")
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
	if not token_to_follow or not cam or not contenedor:
		return
	var p_sub := cam.unproject_position(token_to_follow.global_position + Vector3(0, 0.9, 0))
	var zoom: float = cam.get_meta("zoom_factor", 2.0)
	scale = Vector2.ONE * ESCALA_BASE * zoom
	position = contenedor.global_position + p_sub * contenedor.stretch_shrink + OFFSET_PANTALLA * scale

# Primero nos quitamos de en medio (para que el DungeonManager no nos vea abiertos)
# y luego avisamos al CombatManager.
func _cerrar_y_llamar(metodo: String) -> void:
	name = "MenuTactico_Cerrando"
	queue_free()
	if combat_manager:
		combat_manager.call(metodo)

func _on_btn_atacar_pressed() -> void:
	_cerrar_y_llamar("OnBotonAtacar")

func _on_btn_habilidad_pressed() -> void:
	_cerrar_y_llamar("OnBotonHabilidad")

func _on_btn_pasar_pressed() -> void:
	_cerrar_y_llamar("OnBotonPasar")
