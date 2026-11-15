extends SceneTree
# Coste de las piezas del sigilo: solido(), los conos, la detección, acechados()
#   godot --path . -s res://Tools/perf_sigilo.gd
var t := 0.0
var hecho := false
func _initialize():
	change_scene_to_file("res://Scenes/main.tscn")
func _ms(f: Callable, n: int) -> float:
	var a := Time.get_ticks_usec()
	for i in n:
		f.call()
	return (Time.get_ticks_usec() - a) / 1000.0 / n
func _process(d: float) -> bool:
	t += d
	if t < 4.0 or hecho:
		return false
	hecho = true
	var dm = root.get_node("Main/ContenedorMapa/SubViewport/DungeonManager")
	var tac = root.get_node("Tactica")
	var sig = root.get_node("Sigilo")
	var enemigos := []
	for u in tac.unidades():
		if not tac.es_heroe(u):
			enemigos.append(u)
	print("[perf] enemigos: ", enemigos.size())
	var e0: Node = enemigos[0]
	var h0: Node = tac.unidades()[0]
	print("[perf] get_meta en enemigo: %.4f ms" % _ms(func(): e0.get_meta("grid_pos"), 500))
	print("[perf] get_meta en héroe: %.4f ms" % _ms(func(): h0.get_meta("grid_pos"), 500))
	print("[perf] get_meta en el DungeonManager: %.4f ms" % _ms(func(): dm.get_meta("version_mapa", 0), 500))
	print("[perf] Tactica.unidades: %.4f ms" % _ms(func(): tac.unidades(), 200))
	print("[perf] Sigilo.ve: %.4f ms" % _ms(func(): sig.ve(e0, tac.pos(h0), h0), 200))
	print("[perf] Sigilo.alcance: %.4f ms" % _ms(func(): sig.alcance(e0), 200))
	print("[perf] ObtenerTransitables: %.3f ms" % _ms(func(): dm.call("ObtenerTransitables"), 50))
	print("[perf] Tactica.solido (pared): %.3f ms" % _ms(func(): tac.solido(Vector2i(0, 0)), 200))
	print("[perf] Tactica.solido (suelo): %.3f ms" % _ms(func(): tac.solido(dm.get_meta("casilla_inicio")), 200))
	print("[perf] HayLineaDeVision: %.3f ms" % _ms(func(): dm.call("HayLineaDeVision", 1, 1, 10, 10), 200))
	print("[perf] _detectar (todos): %.2f ms" % _ms(func(): sig.despertar_quien_vea(), 5))
	print("[perf] acechados: %.2f ms" % _ms(func(): sig.acechados(), 5))
	var n := 0
	var a := Time.get_ticks_usec()
	for e in enemigos:
		if not sig._conos or not is_instance_valid(sig._conos):
			sig._conos = Node3D.new()
			dm.add_child(sig._conos)
		sig._cono(e, sig.mirada(e), 0.3, 0.0)
		n += 1
		if n >= 4:
			break
	print("[perf] un cono (media de %d): %.1f ms" % [n, (Time.get_ticks_usec() - a) / 1000.0 / maxi(1, n)])
	print("[perf] linea_de_vision (GDScript): %.4f ms" % _ms(func(): tac.linea_de_vision(Vector2i(1, 1), Vector2i(10, 10)), 500))
	# La línea de visión en GDScript tiene que dar lo mismo que la del C++
	var lado: int = tac.lado()
	var distintas := 0
	var probadas := 0
	var rng := RandomNumberGenerator.new()
	rng.seed = 12345
	for i in 3000:
		var p := Vector2i(rng.randi_range(0, lado - 1), rng.randi_range(0, lado - 1))
		var q := p + Vector2i(rng.randi_range(-9, 9), rng.randi_range(-9, 9))
		if q.x < 0 or q.y < 0 or q.x >= lado or q.y >= lado:
			continue
		probadas += 1
		if bool(dm.call("HayLineaDeVision", p.x, p.y, q.x, q.y)) != tac.linea_de_vision(p, q):
			distintas += 1
			if distintas <= 5:
				print("[perf] DISTINTA: ", p, " -> ", q)
	print("[perf] línea de visión: %d de %d iguales" % [probadas - distintas, probadas])
	# Y sólido: el mapa fijo frente a lo de antes (transitables + unidades)
	var libres: PackedByteArray = dm.call("ObtenerTransitables")
	var mal := 0
	for x in lado:
		for y in lado:
			var c := Vector2i(x, y)
			var antes: bool = libres[x * lado + y] != 1 and tac.unidad_en(c) == null
			if antes != tac.solido(c):
				mal += 1
				if mal <= 5:
					print("[perf] SÓLIDO DISTINTO en ", c, " antes ", antes, " ahora ", tac.solido(c))
	print("[perf] sólido: %d casillas distintas" % mal)
	quit()
	return false
