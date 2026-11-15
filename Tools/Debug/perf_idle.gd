extends SceneTree
# Idle: does anything grow over time? (process, nodes, objects)
var t := 0.0
var sig := 2.0
func _initialize():
	change_scene_to_file("res://Scenes/main.tscn")
func _process(d: float) -> bool:
	t += d
	if t >= sig:
		sig += 3.0
		print("[perf] t=%.0f process %.1f ms | nodos %d | objetos %d | huérfanos %d | fps %d" % [t, Performance.get_monitor(Performance.TIME_PROCESS) * 1000.0, Performance.get_monitor(Performance.OBJECT_NODE_COUNT), Performance.get_monitor(Performance.OBJECT_COUNT), Performance.get_monitor(Performance.OBJECT_ORPHAN_NODE_COUNT), Performance.get_monitor(Performance.TIME_FPS)])
	if t > 20.0:
		quit()
	return false
