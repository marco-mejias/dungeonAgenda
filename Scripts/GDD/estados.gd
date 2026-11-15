extends Node

# Estados (autoload): status effects of heroes and enemies. One API for the whole game:
#   Estados.aplicar(nodo, "ardiendo", dias, potencia, fuente)
#   Estados.quitar(nodo, "ardiendo")   Estados.tiene(nodo, "aturdido")
#   Estados.potencia(nodo, "sangrando")   Estados.de(nodo)
# They live in the node's own metas: "estados" = {nombre: {dias, potencia, fuente}} (heroes: their
# PartyManager node; enemies: their token). The C++ reads that meta too.
#
# Every day (at the start) they apply their effect and wear off: "dias" = how many day starts they
# last. The icons are drawn over each character's head (placeholders in Assets/UI/Estados).

const CARPETA_ICONOS := "res://Assets/UI/Estados/"
const RUTA_DM := "/root/Main/ContenedorMapa/SubViewport/DungeonManager"
const RUTA_PM := "/root/Main/PartyManager"

# name -> definition. dano_dia: damage at the start of the day; dano_paso: damage per tile walked;
# pierde_dia: doesn't act (hero: no PA that day; enemy: skips its turn)
const DEF := {
	"ardiendo": {"nombre": "Ardiendo", "color": Color(1.0, 0.55, 0.2), "dano_dia": 3, "texto": "3 de daño cada día"},
	"envenenado": {"nombre": "Envenenado", "color": Color(0.55, 0.9, 0.35), "dano_dia": 2, "texto": "2 de daño cada día"},
	"sangrando": {"nombre": "Sangrando", "color": Color(0.9, 0.2, 0.2), "dano_paso": 1, "texto": "1 de daño por casilla que anda"},
	"aturdido": {"nombre": "Aturdido", "color": Color(1.0, 0.9, 0.35), "pierde_dia": true, "texto": "Pierde su próximo turno; le atacan con ventaja"},
	"derribado": {"nombre": "Derribado", "color": Color(0.7, 0.6, 0.5), "texto": "En el suelo: le pegan con ventaja de cerca, él ataca con desventaja"},
	"ayudado": {"nombre": "Ayudado", "color": Color(0.5, 0.85, 1.0), "texto": "+potencia a su próxima tirada de ataque"},
	"desenganchado": {"nombre": "Retirada", "color": Color(0.8, 0.8, 0.9), "texto": "Sin ataques de oportunidad hasta mañana"},
	"oculto": {"nombre": "Oculto", "color": Color(0.74, 0.58, 1.0), "texto": "Agachado: pegado a una pared no le ven de lado y, sin antorcha, tampoco de lejos. Andar cuesta 2 PA por casilla"},
	"sorprendido": {"nombre": "Sorprendido", "color": Color(0.74, 0.58, 1.0), "pierde_dia": true, "texto": "No os vio venir: pierde su primer turno"},
}

var dia_visto := -1

func _dm() -> Node:
	return get_node_or_null(RUTA_DM)

func _pm() -> Node:
	return get_node_or_null(RUTA_PM)

# ==========================================
# API
# ==========================================
func de(n: Node) -> Dictionary:
	return n.get_meta("estados", {}) if n and is_instance_valid(n) else {}

# Does the node have that state right now?
func tiene(n: Node, estado: String) -> bool:
	return de(n).has(estado)

# Strength of a state (e.g. bleeding damage per tile); 0 if it doesn't have it
func potencia(n: Node, estado: String) -> int:
	var e: Dictionary = de(n).get(estado, {})
	return int(e.get("potencia", 0))

# Readable name of a state for the interface
func nombre(estado: String) -> String:
	return DEF.get(estado, {}).get("nombre", estado.capitalize())

# One-line explanation for the tooltips
func describir(estado: String) -> String:
	return DEF.get(estado, {}).get("texto", "")

# Icon of a state (null if there's no PNG yet)
func icono(estado: String) -> Texture2D:
	var ruta := CARPETA_ICONOS + estado + ".png"
	return load(ruta) if ResourceLoader.exists(ruta) else null

## Applies (or renews) a state. If they already had it, the longest and strongest one stays. Returns
## false if it didn't stick (immune: an ability with the "inmune_<estado>" tag)
func aplicar(n: Node, estado: String, dias: int = 1, pot: int = 1, fuente: Node = null) -> bool:
	if n == null or not is_instance_valid(n) or estado == "":
		return false
	if Progreso.tiene_etiqueta(n, "inmune_" + estado):
		return false
	var est: Dictionary = de(n).duplicate()
	var previo: Dictionary = est.get(estado, {})
	est[estado] = {
		"dias": maxi(dias, int(previo.get("dias", 0))),
		"potencia": maxi(pot, int(previo.get("potencia", 0))),
		"fuente": str(fuente.name) if fuente else str(previo.get("fuente", "")),
	}
	n.set_meta("estados", est)
	if previo.is_empty() and DEF.has(estado) and not estado in ["ayudado", "desenganchado", "oculto"]:
		_texto(n, "¡%s!" % nombre(estado), DEF[estado]["color"])
		var hud := get_tree().get_first_node_in_group("hud")
		if hud:
			hud.registrar("%s: %s" % [str(n.get_meta("nombre_display", n.name)), nombre(estado)], DEF[estado]["color"])
	return true

# Removes one state
func quitar(n: Node, estado: String) -> void:
	if n == null or not is_instance_valid(n) or not n.has_meta("estados"):
		return
	var est: Dictionary = de(n).duplicate()
	est.erase(estado)
	n.set_meta("estados", est)

# Removes every state (new floor, death)
func limpiar(n: Node) -> void:
	if n and is_instance_valid(n) and n.has_meta("estados"):
		n.remove_meta("estados")

## Extra damage for moving one tile (sangrando). Called by Tactica.al_mover
func dano_por_paso(n: Node) -> int:
	var total := 0
	for estado in de(n):
		total += int(DEF.get(estado, {}).get("dano_paso", 0)) * maxi(1, int(de(n)[estado]["potencia"]))
	return total

# ==========================================
# EVERY DAY
# ==========================================
func _process(_delta: float) -> void:
	var pm := _pm()
	if pm == null:
		dia_visto = -1
		return
	var dia := int(pm.get_meta("turno_actual", 1))
	if dia_visto < 0:
		dia_visto = dia
	elif dia != dia_visto:
		dia_visto = dia
		nuevo_dia()
	_pintar_iconos()

# Start of a day: every state does its effect (burn, poison...) and loses one day
func nuevo_dia() -> void:
	for n in Tactica.unidades():
		var est: Dictionary = de(n).duplicate()
		if est.is_empty():
			continue
		for estado in est.keys():
			var def: Dictionary = DEF.get(estado, {})
			var pot := maxi(1, int(est[estado]["potencia"]))
			if def.get("dano_dia", 0) > 0:
				var fuente: Node = _pm().get_node_or_null(NodePath(str(est[estado]["fuente"]))) if str(est[estado]["fuente"]) != "" else null
				Tactica.danar(n, int(def["dano_dia"]) * pot, fuente)
				if not is_instance_valid(n):
					break
			if def.get("pierde_dia", false) and Tactica.es_heroe(n):
				n.set_meta("pa_actual", 0)
				_texto(n, "Aturdido: pierde el día", def["color"])
			est[estado]["dias"] = int(est[estado]["dias"]) - 1
			if int(est[estado]["dias"]) <= 0:
				est.erase(estado)
		if is_instance_valid(n):
			n.set_meta("estados", est)

# ==========================================
# ICONS OVER THE HEAD
# ==========================================
const TAM_ICONO := 0.14
const ALTURA := 1.55

var _raices := {} # Token instance_id -> Node3D with its icons (hangs from the map, not from the token)

# Keeps the state icons over every character's head in sync
func _pintar_iconos() -> void:
	var dm := _dm()
	if dm == null:
		_raices.clear()
		return
	var cam: Camera3D = dm.get_node_or_null("Camera3D")
	var vistos := {}
	for n in Tactica.unidades():
		var ficha := Tactica.ficha(n)
		if ficha == null:
			continue
		var est := de(n)
		var visibles := []
		for e in est:
			if icono(e):
				visibles.append(e)
		if visibles.is_empty() or not ficha.visible:
			continue
		var id := ficha.get_instance_id()
		vistos[id] = true
		var raiz: Node3D = _raices.get(id)
		if raiz == null or not is_instance_valid(raiz):
			raiz = Node3D.new()
			raiz.name = "IconosEstado"
			dm.add_child(raiz, true)
			_raices[id] = raiz
		var firma := ",".join(visibles)
		if raiz.get_meta("firma", "") != firma:
			raiz.set_meta("firma", firma)
			for c in raiz.get_children():
				c.queue_free()
			for i in visibles.size():
				var s := Sprite3D.new()
				s.texture = icono(visibles[i])
				s.texture_filter = BaseMaterial3D.TEXTURE_FILTER_NEAREST
				s.no_depth_test = true
				s.render_priority = 9
				s.shaded = false
				s.pixel_size = TAM_ICONO / float(maxi(1, s.texture.get_height()))
				s.position = Vector3((i - (visibles.size() - 1) * 0.5) * (TAM_ICONO + 0.02), 0, 0)
				raiz.add_child(s)
		raiz.global_position = ficha.global_position + Vector3(0, ALTURA, 0)
		if cam:
			raiz.global_basis = cam.global_basis
	for id in _raices.keys():
		if not vistos.has(id):
			if is_instance_valid(_raices[id]):
				_raices[id].queue_free()
			_raices.erase(id)

# Floating text over a character ("¡Arde!")
func _texto(n: Node, t: String, color: Color) -> void:
	var f := Tactica.ficha(n)
	var esp := get_node_or_null("/root/Especiales")
	if f and esp:
		esp.texto(f.position + Vector3(0, 0.5, 0), t, color, 26)
