extends PanelContainer

# Tarjeta de previsión de ataque (abajo a la derecha), estilo tácticos de PS1.
# Sale al pasar el ratón sobre un enemigo visible o sobre ATAQUE en el menú táctico
# (que deja su objetivo en la meta "prevision_objetivo" del HUD).

var ORO: Color = Estilo.color("oro")

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
	add_theme_stylebox_override("panel", Estilo.caja("panel_rojo"))

	var v := VBoxContainer.new()
	v.add_theme_constant_override("separation", 3)
	v.mouse_filter = Control.MOUSE_FILTER_IGNORE
	add_child(v)

	titulo = _label(v, 13, ORO)
	linea_dano = _label(v, 12, Estilo.color("hueso"))
	linea_critico = _label(v, 10, Estilo.color("oro"))

	# Vida del objetivo: fondo = vida actual, encima = lo que le quedará
	var barras := Control.new()
	barras.custom_minimum_size = Vector2(0, 8)
	barras.mouse_filter = Control.MOUSE_FILTER_IGNORE
	v.add_child(barras)
	barra_antes = _barra(barras, Estilo.color("vida").darkened(0.55))
	barra_despues = _barra(barras, Estilo.color("vida"))

	resultado = _label(v, 12, Color(1, 1, 1))
	coste = _label(v, 10, Estilo.color("texto_suave"))

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
	fondo.bg_color = Estilo.color("sombra") if padre.get_child_count() == 0 else Color(0, 0, 0, 0)
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
		resultado.add_theme_color_override("font_color", Estilo.color("hueso"))
	elif d.get("a_distancia", false):
		resultado.text = "A distancia: sin contraataque"
		resultado.add_theme_color_override("font_color", Estilo.color("bien"))
	else:
		resultado.text = "Contraataque: -%d PV" % d["contra"]
		resultado.add_theme_color_override("font_color", Estilo.color("mal"))

	var falta_pa := int(d["pa"]) < int(d["coste"])
	if not d.get("en_alcance", false):
		coste.text = "Acércate para atacar · %d PA" % d["coste"]
		coste.add_theme_color_override("font_color", Estilo.color("texto_suave"))
	elif falta_pa:
		coste.text = "Coste: %d PA (tienes %d)" % [d["coste"], d["pa"]]
		coste.add_theme_color_override("font_color", Estilo.color("mal"))
	else:
		coste.text = "Coste: %d PA (te quedan %d)" % [d["coste"], int(d["pa"]) - int(d["coste"])]
		coste.add_theme_color_override("font_color", Estilo.color("texto_suave"))
