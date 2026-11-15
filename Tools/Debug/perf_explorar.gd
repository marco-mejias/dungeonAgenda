extends SceneTree
# Measures performance while exploring: frame times while the party walks and the cost of the
# functions called on every step.
#   godot --path . -s res://Tools/Debug/perf_explorar.gd

var dm: Node
var pm: Node
var t0 := 0.0
var fase := 0
var frames: Array[float] = []
var ultimo := 0
var medir := false
var vueltas := 0

func _initialize():
	change_scene_to_file("res://Scenes/main.tscn")
	ultimo = Time.get_ticks_usec()

func _ms(f: Callable, n: int) -> float:
	var a := Time.get_ticks_usec()
	for i in n:
		f.call()
	return (Time.get_ticks_usec() - a) / 1000.0 / n

func _process(_d: float) -> bool:
	var ahora := Time.get_ticks_usec()
	if medir:
		frames.append((ahora - ultimo) / 1000.0)
	ultimo = ahora
	t0 += _d
	match fase:
		0:
			if t0 > 3.0:
				dm = root.get_node_or_null("Main/ContenedorMapa/SubViewport/DungeonManager")
				pm = root.get_node_or_null("Main/PartyManager")
				if dm == null:
					return false
				if vueltas == 0:
					print("[perf] RefrescarVision: %.2f ms" % _ms(func(): dm.call("ActualizarVisibilidadEntidades"), 20))
				print("[perf] ObtenerMinimapa: %.2f ms" % _ms(func(): dm.call("ObtenerMinimapa"), 20))
				print("[perf] UpdateHighlight: %.2f ms" % _ms(func(): dm.call("RefrescarLuces"), 20))
				print("[perf] Tactica.unidades: %.3f ms" % _ms(func(): root.get_node("Tactica").call("unidades"), 200))
				print("[perf] ObtenerObstaculosDinamicos: %.2f ms" % _ms(func(): dm.call("ObtenerObstaculosDinamicos"), 20))
				# Walk as far as possible through the explored area
				var alc: PackedInt32Array = dm.call("DebugAlcance")
				var h = pm.call("GetActiveCharacter")
				var p: Vector2i = h.get_meta("grid_pos")
				var mejor := p
				for i in range(0, alc.size(), 2):
					var c := Vector2i(alc[i], alc[i + 1])
					if absi(c.x - p.x) + absi(c.y - p.y) > absi(mejor.x - p.x) + absi(mejor.y - p.y):
						mejor = c
				print("[perf] andando de ", p, " a ", mejor)
				var t_clic := Time.get_ticks_usec()
				dm.call("ProcesarClicEnCasilla", mejor.x, mejor.y)
				print("[perf] el clic tarda %.1f ms" % ((Time.get_ticks_usec() - t_clic) / 1000.0))
				var pmg = root.get_node("Main/ContenedorMapa/SubViewport/DungeonManager/PathManager")
				var t_ruta := Time.get_ticks_usec()
				pmg.call("CalcularRuta", p.x, p.y, mejor.x, mejor.y)
				print("[perf] CalcularRuta %.1f ms" % ((Time.get_ticks_usec() - t_ruta) / 1000.0))
				medir = true
				fase = 1
				t0 = 0.0
		1:
			if t0 > 0.5 and not dm.get_meta("is_moving", false) or t0 > 8.0:
				medir = false
				frames.sort()
				var suma := 0.0
				for f in frames:
					suma += f
				var n := frames.size()
				if n > 0:
					print("[perf] andando: %d frames, media %.1f ms, p50 %.1f, p95 %.1f, max %.1f" % [n, suma / n, frames[n / 2], frames[int(n * 0.95)], frames[n - 1]])
				frames.clear()
				vueltas += 1
				if vueltas < 3:
					fase = 0
					t0 = 2.5
					return false
				print("[perf] fps monitor: ", Performance.get_monitor(Performance.TIME_FPS), " | process ", Performance.get_monitor(Performance.TIME_PROCESS) * 1000.0, " ms | draw calls ", Performance.get_monitor(Performance.RENDER_TOTAL_DRAW_CALLS_IN_FRAME), " | objetos ", Performance.get_monitor(Performance.RENDER_TOTAL_OBJECTS_IN_FRAME), " | nodos ", Performance.get_monitor(Performance.OBJECT_NODE_COUNT))
				quit()
	return false
