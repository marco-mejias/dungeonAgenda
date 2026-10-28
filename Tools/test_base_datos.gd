extends SceneTree
# Test del panel "Base de datos" (fuera del editor):
#   godot --headless --path . -s res://Tools/test_base_datos.gd

var fallos: Array[String] = []

func check(cond: bool, msg: String) -> void:
	print("[test] ", "OK   " if cond else "FAIL ", msg)
	if not cond:
		fallos.append(msg)

func filas(panel) -> int:
	var n := 0
	var item: TreeItem = panel.arbol.get_root().get_first_child()
	while item:
		n += 1
		item = item.get_next()
	return n

func _init() -> void:
	var panel = load("res://addons/base_datos/panel_base_datos.gd").new()
	root.add_child(panel)
	await process_frame

	var datos: Dictionary = panel.datos
	check(datos["objetos"].size() >= 16 and datos["enemigos"].size() >= 6 and datos["tablas"].size() >= 2, "lee objetos, enemigos y tablas (%d, %d, %d)" % [datos["objetos"].size(), datos["enemigos"].size(), datos["tablas"].size()])
	check(datos["avisos"].is_empty(), "los datos no tienen avisos %s" % [datos["avisos"]])
	var total := filas(panel)
	check(total == datos["objetos"].size() + datos["enemigos"].size() + datos["tablas"].size(), "la tabla tiene una fila por recurso (%d)" % total)

	panel.opt_filtro.select(1)
	panel._pintar()
	check(filas(panel) == 6, "el filtro Armas deja las 6 armas (%d)" % filas(panel))
	panel.opt_filtro.select(5)
	panel._pintar()
	check(filas(panel) == datos["enemigos"].size(), "el filtro Enemigos deja los enemigos (%d)" % filas(panel))
	panel.opt_filtro.select(0)
	panel.buscar.text = "pocion"
	panel._pintar()
	check(filas(panel) == 1, "buscar 'pocion' encuentra la poción (%d)" % filas(panel))
	panel.buscar.text = ""

	# Crear y duplicar un consumible de prueba (y borrarlos después)
	var ruta: String = panel.crear(3, "prueba temporal")
	check(ruta == "res://Data/Objetos/Consumibles/prueba_temporal.tres", "crea el consumible en su carpeta (%s)" % ruta)
	var nuevo: ObjetoData = load(ruta) if ruta != "" else null
	check(nuevo != null and nuevo.apilable and nuevo.usos == 1 and nuevo.efectos_al_usar.size() == 1, "el consumible nuevo nace apilable, de 1 uso y con un efecto")
	check(panel.datos["objetos"].has("prueba_temporal"), "y aparece en la base de datos")
	var item: TreeItem = panel.arbol.get_selected()
	check(item != null and item.get_metadata(0) == ruta, "y queda seleccionado")
	panel._duplicar()
	var copia := "res://Data/Objetos/Consumibles/prueba_temporal_copia.tres"
	check(FileAccess.file_exists(copia) and panel.datos["objetos"].has("prueba_temporal_copia"), "duplicar crea la copia con otro id")
	var repetidos := Array(panel.datos["avisos"]).filter(func(t): return "repetido" in t)
	check(repetidos.is_empty(), "la copia no repite id %s" % [repetidos])
	check(Array(panel.datos["avisos"]).any(func(t): return "prueba_temporal" in t and "icono" in t), "el panel avisa de que el objeto nuevo no tiene icono")
	for r in [ruta, copia]:
		DirAccess.remove_absolute(ProjectSettings.globalize_path(r))
		DirAccess.remove_absolute(ProjectSettings.globalize_path(r + ".uid"))

	print("[test] RESULTADO: ", "TODO OK" if fallos.is_empty() else "%d FALLOS: %s" % [fallos.size(), fallos])
	quit(0 if fallos.is_empty() else 1)
