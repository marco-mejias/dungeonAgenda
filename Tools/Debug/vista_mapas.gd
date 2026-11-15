extends SceneTree
# Generates several floors with the real generator (Mazmorra.hpp) and saves them seen from above
# in a PNG, to review the design without playing:
#   godot --headless --path . -s res://Tools/Debug/vista_mapas.gd -- <output.png> [num_maps] [floor]
# Colours: black wall, stone floor, light stone high room, blue step, light grey column,
# orange closed door, yellow open door.

const COLORES := {0: Color("120c14"), 1: Color("6b5a5e"), 2: Color("d07a30"), 3: Color("e8c060"), 4: Color("c8c0b8"), 5: Color("9a8a90"), 6: Color("5a8ab0")}
const ESC := 8
const HUECO := 2

var salida := "user://mapas.png"
var num := 6
var piso := 1
var frames := 0

func _initialize() -> void:
	var args := OS.get_cmdline_user_args()
	if args.size() > 0:
		salida = args[0]
	if args.size() > 1:
		num = int(args[1])
	if args.size() > 2:
		piso = int(args[2])
	change_scene_to_file("res://Scenes/main.tscn")

func _process(_dt: float) -> bool:
	frames += 1
	if frames < 30:
		return false
	var dm := root.get_node_or_null("Main/ContenedorMapa/SubViewport/DungeonManager")
	var pm := root.get_node_or_null("Main/PartyManager")
	if not dm:
		push_error("no encuentro el DungeonManager")
		quit(1)
		return true
	if pm:
		pm.set_meta("piso_actual", piso)
	var lado := 0
	var mapas: Array = []
	for i in num:
		dm.call("ConstruirLogica")
		var tipos: PackedByteArray = dm.call("ObtenerTiposMapa")
		lado = int(sqrt(tipos.size()))
		mapas.append(tipos)
	var cols := mini(3, num)
	var filas := ceili(num / float(cols))
	var celda := (lado + HUECO) * ESC
	var img := Image.create(cols * celda + HUECO * ESC, filas * celda + HUECO * ESC, false, Image.FORMAT_RGBA8)
	img.fill(Color("2a2030"))
	for m in mapas.size():
		var ox := HUECO * ESC + (m % cols) * celda
		var oy := HUECO * ESC + (m / cols) * celda
		var tipos: PackedByteArray = mapas[m]
		for x in lado:
			for y in lado:
				img.fill_rect(Rect2i(ox + x * ESC, oy + y * ESC, ESC, ESC), COLORES.get(int(tipos[x * lado + y]), Color.MAGENTA))
	img.save_png(salida)
	print("[vista_mapas] ", num, " mapas -> ", salida)
	quit(0)
	return true
