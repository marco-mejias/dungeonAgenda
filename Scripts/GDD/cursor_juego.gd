extends Node

const Iconos := preload("res://Scripts/GDD/iconos_pixel.gd")

# Pixel art cursor that changes with whatever is under the mouse on the map:
#   arrow (normal), hand (interactable), sword (enemy), cross (can't go)
# It asks the DungeonManager with TipoCursor(x, y). Over the interface it's always the arrow.

const ESCALA := 3
const PUNTOS := {"flecha": Vector2(0, 0), "mano": Vector2(4, 0), "espada": Vector2(8, 0), "aspa": Vector2(5, 5)}
const POR_TIPO := ["flecha", "mano", "espada", "aspa"]

var texturas := {}
var actual := ""

func _ready() -> void:
	for n in PUNTOS.keys():
		texturas[n] = Iconos.textura(n, ESCALA)
	_poner("flecha")

func _exit_tree() -> void:
	# Back to the menu: system cursor
	Input.set_custom_mouse_cursor(null)

func _process(_delta: float) -> void:
	var nombre := "flecha"
	var dm := get_node_or_null("/root/Main/ContenedorMapa/SubViewport/DungeonManager")
	var contenedor := get_node_or_null("/root/Main/ContenedorMapa")
	var bajo_raton := get_viewport().gui_get_hovered_control()
	if dm and (bajo_raton == null or bajo_raton == contenedor):
		var celda: Vector2i = dm.get_meta("hover_celda", Vector2i(-1, -1))
		var tipo: int = dm.call("TipoCursor", celda.x, celda.y)
		nombre = POR_TIPO[clampi(tipo, 0, POR_TIPO.size() - 1)]
	_poner(nombre)

# Swaps the system cursor (only when it actually changes)
func _poner(nombre: String) -> void:
	if nombre == actual:
		return
	actual = nombre
	Input.set_custom_mouse_cursor(texturas[nombre], Input.CURSOR_ARROW, PUNTOS[nombre] * ESCALA)
