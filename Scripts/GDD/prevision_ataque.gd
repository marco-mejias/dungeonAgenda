extends PanelContainer

# Tarjeta de previsión de ataque (abajo a la derecha), estilo tácticos de PS1.
# Sale al pasar el ratón sobre un enemigo visible o sobre ATAQUE en el menú táctico
# (que deja su objetivo en la meta "prevision_objetivo" del HUD).

const ORO := Color(1.0, 0.8, 0.3)

var titulo: Label
var linea_dano: Label
var linea_critico: Label
var barra_antes: ProgressBar
var barra_despues: ProgressBar
var resultado: Label
var coste: Label
var objetivo_actual := Vector2i(-99, -99)

func _ready() -> void:
	mouse_filter = Control.MOUSE_FILTER_IGNORE
	visible = false
	custom_minimum_size = Vector2(240, 0)
	var estilo := StyleBoxFlat.new()
	estilo.bg_color = Color(0.04, 0.05, 0.16, 0.94)
	estilo.set_border_width_all(2)
	estilo.border_color = Color(0.85, 0.3, 0.25)
	estilo.corner_detail = 1
	estilo.set_content_margin_all(8)
	estilo.shadow_color = Color(0, 0, 0, 0.5)
	estilo.shadow_size = 3
	add_theme_stylebox_override("panel", estilo)

	var v := VBoxContainer.new()
	v.add_theme_constant_override("separation", 3)
	v.mouse_filter = Control.MOUSE_FILTER_IGNORE
	add_child(v)

	titulo = _label(v, 13, ORO)
	linea_dano = _label(v, 12, Color(0.95, 0.95, 1.0))
	linea_critico = _label(v, 10, Color(1.0, 0.7, 0.3))

	# Vida del objetivo: fondo = vida actual, encima = lo que le quedará
	var barras := Control.new()
	barras.custom_minimum_size = Vector2(0, 8)
	barras.mouse_filter = Control.MOUSE_FILTER_IGNORE
	v.add_child(barras)
	barra_antes = _barra(barras, Color(0.45, 0.1, 0.1))
	barra_despues = _barra(barras, Color(0.92, 0.2, 0.15))

	resultado = _label(v, 12, Color(1, 1, 1))
	coste = _label(v, 10, Color(0.7, 0.75, 0.9))

func _label(padre: Control, tam: int, color: Color) -> Label:
	var l := Label.new()
	l.add_theme_font_size_override("font_size", tam)
	l.add_theme_color_override("font_color", color)
	l.mouse_filter = Control.MOUSE_FILTER_IGNORE
	padre.add_child(l)
	return l

func _barra(padre: Control, color: Color) -> ProgressBar:
	var b := ProgressBar.new()
	b.show_percentage = false
	b.set_anchors_preset(Control.PRESET_FULL_RECT)
	b.mouse_filter = Control.MOUSE_FILTER_IGNORE
	var fondo := StyleBoxFlat.new()
	fondo.bg_color = Color(0.02, 0.02, 0.05) if padre.get_child_count() == 0 else Color(0, 0, 0, 0)
	var relleno := StyleBoxFlat.new()
	relleno.bg_color = color
	b.add_theme_stylebox_override("background", fondo)
	b.add_theme_stylebox_override("fill", relleno)
	padre.add_child(b)
	return b

func _process(_delta: float) -> void:
	var dm := get_node_or_null("/root/Main/ContenedorMapa/SubViewport/DungeonManager")
	var hud := get_parent()
	if not dm or dm.get_meta("bloqueo_evento", false):
		visible = false
		return

	var objetivo: Vector2i = hud.get_meta("prevision_objetivo", Vector2i(-1, -1))
	if objetivo.x < 0:
		objetivo = dm.get_meta("hover_celda", Vector2i(-1, -1))
	var enemigo := dm.get_node_or_null("Enemy_%d_%d" % [objetivo.x, objetivo.y])
	if not enemigo or not enemigo.visible:
		visible = false
		return

	var combate := dm.get_node_or_null("CombatManager")
	var d: Dictionary = combate.call("PrevisionAtaque", objetivo.x, objetivo.y) if combate else {}
	if not d.get("valido", false):
		visible = false
		return
	visible = true
	_rellenar(d, objetivo, dm)

func _rellenar(d: Dictionary, objetivo: Vector2i, dm: Node) -> void:
	titulo.text = "%s  vs  %s" % [str(d["atacante"]).to_upper(), str(d["defensor"]).to_upper()]
	linea_dano.text = "Daño: %d" % d["dano"]
	linea_critico.text = "Crítico (%d%%): %d de daño" % [d["prob_critico"], d["dano_critico"]]

	var hp: int = d["hp"]
	var hp_max: int = d["hp_max"]
	barra_antes.max_value = hp_max
	barra_antes.value = hp
	barra_despues.max_value = hp_max
	barra_despues.value = maxi(0, hp - int(d["dano"]))

	if d["mata"]:
		resultado.text = "¡LO DERROTA!"
		resultado.add_theme_color_override("font_color", ORO)
	elif d["mata_critico"]:
		resultado.text = "Le quedan %d PV (con crítico, cae)" % (hp - int(d["dano"]))
		resultado.add_theme_color_override("font_color", Color(1, 1, 1))
	else:
		resultado.text = "Contraataque: -%d PV" % d["contra"]
		resultado.add_theme_color_override("font_color", Color(1.0, 0.45, 0.4))

	var pm := get_node_or_null("/root/Main/PartyManager")
	var pos: Vector2i = pm.call("GetActiveGridPos") if pm else Vector2i(-99, -99)
	var adyacente := absi(pos.x - objetivo.x) + absi(pos.y - objetivo.y) == 1
	var falta_pa := int(d["pa"]) < int(d["coste"])
	if not adyacente:
		coste.text = "Acércate para atacar · %d PA" % d["coste"]
		coste.add_theme_color_override("font_color", Color(0.7, 0.75, 0.9))
	elif falta_pa:
		coste.text = "Coste: %d PA (tienes %d)" % [d["coste"], d["pa"]]
		coste.add_theme_color_override("font_color", Color(1.0, 0.45, 0.4))
	else:
		coste.text = "Coste: %d PA (te quedan %d)" % [d["coste"], int(d["pa"]) - int(d["coste"])]
		coste.add_theme_color_override("font_color", Color(0.7, 0.75, 0.9))
