extends SceneTree
# Cuánto cuesta el _process de cada script GDScript (llamándolo a mano) y, para el resto,
# cuánto baja TIME_PROCESS al apagarlo.
#   godot --path . -s res://Tools/perf_procesos.gd
var t := 0.0
var hecho := false

func _initialize():
	change_scene_to_file("res://Scenes/main.tscn")

func _process(d: float) -> bool:
	t += d
	if t < 4.0 or hecho:
		return false
	hecho = true
	var nodos: Array = []
	for n in root.find_children("*", "", true, false):
		if n.is_processing() and n.get_script() != null:
			nodos.append(n)
	var res := []
	for n in nodos:
		var s: Script = n.get_script()
		if not (s is GDScript) or not n.has_method("_process"):
			res.append([-1.0, str(n.get_path()) + " (" + str(s) + ")"])
			continue
		var a := Time.get_ticks_usec()
		for i in 20:
			n._process(0.016)
		res.append([(Time.get_ticks_usec() - a) / 20000.0, str(n.get_path())])
	res.sort_custom(func(x, y): return x[0] > y[0])
	for r in res:
		print("[perf] %7.3f ms  %s" % r)
	quit()
	return false
