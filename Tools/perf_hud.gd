extends SceneTree
# Coste de cada parte del HUD por frame
var t := 0.0
var hecho := false
func _initialize():
	change_scene_to_file("res://Scenes/main.tscn")
func _ms(f: Callable) -> float:
	var a := Time.get_ticks_usec()
	for i in 20:
		f.call()
	return (Time.get_ticks_usec() - a) / 20000.0
func _process(d: float) -> bool:
	t += d
	if t < 4.0 or hecho:
		return false
	hecho = true
	var hud = root.get_node("Main/CanvasLayer/HUD")
	var pm = root.get_node("Main/PartyManager")
	var dm = root.get_node("Main/ContenedorMapa/SubViewport/DungeonManager")
	var act = pm.call("GetActiveCharacter")
	for par in [["columna", func(): hud._actualizar_columna(act)], ["turno", func(): hud._actualizar_turno(act)], ["activo", func(): hud._actualizar_activo(act)], ["habilidades", func(): hud._actualizar_habilidades(act)], ["objetos", func(): hud._actualizar_objetos(act)], ["botones", func(): hud._actualizar_botones(act)], ["ficha", func(): hud._colocar_ficha_objeto(act, 0.016)], ["process entero", func(): hud._process(0.016)], ["teclas", func(): hud._teclas()], ["atenuar", func(): hud._atenuar(0.016)],
		["DescribirInventario", func(): pm.call("DescribirInventario", str(act.name))], ["BuscarEnemigoAdyacente", func(): dm.call("BuscarEnemigoAdyacente")], ["Apariencia.textura", func(): root.get_node("Apariencia").textura(act)], ["Modo.lejanos", func(): root.get_node("Modo").lejanos()], ["Modo.grupos", func(): root.get_node("Modo").grupos_heroes()], ["Progreso.motivo_accion", func(): root.get_node("Progreso").motivo_accion("ayudar")]]:
		print("[perf] %-24s %7.3f ms" % [par[0], _ms(par[1])])
	quit()
	return false
