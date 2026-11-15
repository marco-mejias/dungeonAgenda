extends Panel

# Pantalla de evento (CanvasLayer2/PantallaEvento): la abre el EventManager (vía uiManager) y se
# resuelve con una tirada d20 como la del combate:
#   éxito si  d20 + bono de la stat  >=  CD,  con CD = 16 - 5 x multiplicador_dificultad.
#   20 natural: éxito crítico (efectos_critico además).  1 natural: pifia (efectos_pifia además).
#   Bono = stat x (1 + suerte x nivel x 3%) / 2 (igual que en combate); con cordura < 20 la stat vale la mitad.
# Arte (Assets/Eventos/<evento>/): ilustración de la escena, imagen al ganar y al perder (por opción,
# o la del evento) y sprite del mapa. Lo que falte se ve con un placeholder.

const RUTA_DM := "/root/Main/ContenedorMapa/SubViewport/DungeonManager"
const RUTA_PM := "/root/Main/PartyManager"
const PH_ESCENA := preload("res://Assets/Eventos/_placeholder/escena.png")
const PH_EXITO := preload("res://Assets/Eventos/_placeholder/exito.png")
const PH_FRACASO := preload("res://Assets/Eventos/_placeholder/fracaso.png")
const LADO_FOTO := 192 # las imágenes son de 96x96: se ven a 2x, píxel nítido
const T_GIRO := 1.3
const NOMBRES_STAT := {"fuerza": "Fuerza", "magia": "Magia", "suerte": "Suerte", "velocidad": "Velocidad",
	"defensa": "Defensa", "hp_actual": "Vida", "hp_max": "Vida", "sanidad_actual": "Cordura", "ninguna": "Suerte pura"}

var evento: Resource = null
var heroe: Node = null
var resolviendo := false
var ultima_tirada := {} # para los tests: {d20, bono, cd, exito, critico, pifia, opcion}

@onready var titulo: Label = $Titulo
@onready var historia: RichTextLabel = $TextoHistoria
@onready var foto: TextureRect = $Foto
@onready var marco: Panel = $MarcoFoto
@onready var dado: Control = $Dado
@onready var botones: VBoxContainer = $CajaBotones
@onready var continuar: Button = $CajaBotones/BtnContinuar

func _ready() -> void:
	add_theme_stylebox_override("panel", Estilo.caja("panel"))
	Estilo.como_titulo(titulo, 30)
	marco.add_theme_stylebox_override("panel", Estilo.caja("hueco"))
	foto.texture_filter = CanvasItem.TEXTURE_FILTER_NEAREST
	foto.expand_mode = TextureRect.EXPAND_IGNORE_SIZE
	foto.stretch_mode = TextureRect.STRETCH_KEEP_ASPECT_CENTERED
	historia.bbcode_enabled = true
	resized.connect(_colocar)
	_colocar()
	if not continuar.pressed.is_connected(cerrar):
		continuar.pressed.connect(cerrar)

# Ilustración grande y centrada; el texto y los botones debajo
func _colocar() -> void:
	var w := size.x
	var x := (w - LADO_FOTO) * 0.5
	foto.position = Vector2(x, 70)
	foto.size = Vector2(LADO_FOTO, LADO_FOTO)
	marco.position = foto.position - Vector2(6, 6)
	marco.size = foto.size + Vector2(12, 12)
	dado.position = foto.position
	dado.size = foto.size
	historia.position = Vector2(28, 70 + LADO_FOTO + 14)
	historia.size = Vector2(w - 56, 150)
	botones.position = Vector2(36, historia.position.y + historia.size.y + 8)
	botones.size = Vector2(w - 72, 0)

func _pm() -> Node:
	return get_node_or_null(RUTA_PM)

func _dm() -> Node:
	return get_node_or_null(RUTA_DM)

# ---------------- La tirada ----------------

## {stat, valor, bono, cd, prob} de una opción para este héroe
func info_tirada(op: Resource, h: Node) -> Dictionary:
	var stat: String = op.stat_usada
	var valor := 0 if stat == "ninguna" else int(h.get_meta(stat, 0))
	if int(h.get_meta("sanidad_actual", 100)) < 20:
		valor = valor / 2
	var suerte := int(h.get_meta("suerte", 1))
	var nivel := int(h.get_meta("nivel", 1))
	var bono := floori(valor * (1.0 + suerte * nivel * 0.03) / 2.0)
	var cd := clampi(roundi(16.0 - 5.0 * float(op.multiplicador_dificultad)), 2, 20)
	var caras := 0
	for d in range(1, 21):
		if _exito(d, bono, cd):
			caras += 1
	return {"stat": stat, "valor": valor, "bono": bono, "cd": cd, "prob": caras * 5}

static func _exito(d20: int, bono: int, cd: int) -> bool:
	if d20 == 1:
		return false
	if d20 == 20:
		return true
	return d20 + bono >= cd

func _tirar_d20() -> int:
	if Engine.has_meta("tirada_forzada"):
		return clampi(int(Engine.get_meta("tirada_forzada")), 1, 20)
	return randi_range(1, 20)

# ---------------- Abrir / resolver / cerrar ----------------

func mostrar(ev: Resource, h: Node) -> void:
	evento = ev
	heroe = h
	resolviendo = false
	ultima_tirada = {}
	titulo.text = ev.titulo
	historia.text = ev.descripcion
	foto.texture = ev.ilustracion if ev.ilustracion else PH_ESCENA
	foto.visible = true
	dado.ocultar()
	for c in botones.get_children():
		if c != continuar:
			c.queue_free()
	continuar.visible = ev.opciones.is_empty()
	for i in ev.opciones.size():
		var op: Resource = ev.opciones[i]
		var info := info_tirada(op, h)
		var b := Button.new()
		b.text = "%s   ·   %s %d%%" % [op.texto_boton, NOMBRES_STAT.get(info["stat"], info["stat"]), info["prob"]]
		b.tooltip_text = "d20 %+d contra CD %d" % [info["bono"], info["cd"]]
		b.custom_minimum_size = Vector2(0, 44)
		b.pressed.connect(resolver.bind(i))
		botones.add_child(b)
		botones.move_child(b, botones.get_child_count() - 2)
	# Entra desde abajo
	var alto := get_viewport_rect().size.y
	modulate = Color.WHITE
	position.y = alto
	visible = true
	var dm := _dm()
	if dm:
		dm.call("SubirMapa")
	var tw := create_tween()
	tw.tween_property(self, "position:y", 0.0, 0.5).set_trans(Tween.TRANS_CUBIC).set_ease(Tween.EASE_OUT)

func resolver(indice: int) -> void:
	if resolviendo or evento == null or heroe == null or indice >= evento.opciones.size():
		return
	resolviendo = true
	var op: Resource = evento.opciones[indice]
	var info := info_tirada(op, heroe)
	var d20 := _tirar_d20()
	var exito := _exito(d20, info["bono"], info["cd"])
	ultima_tirada = {"d20": d20, "bono": info["bono"], "cd": info["cd"], "exito": exito,
		"critico": d20 == 20, "pifia": d20 == 1, "opcion": indice}
	for c in botones.get_children():
		c.visible = false
	foto.visible = false
	var sello := "¡CRÍTICO!" if d20 == 20 else ("¡PIFIA!" if d20 == 1 else ("ÉXITO" if exito else "FALLO"))
	dado.tirar(d20, exito, T_GIRO, sello)
	historia.text = "[center]%s  ·  [b]d20 %+d[/b] contra [b]CD %d[/b]\n\n[shake rate=20.0 level=5]Tirando el dado...[/shake][/center]" % [NOMBRES_STAT.get(info["stat"], info["stat"]), info["bono"], info["cd"]]
	var tw := create_tween()
	tw.tween_interval(T_GIRO + 0.2)
	tw.tween_callback(func():
		historia.text = "[center]%s  ·  d20 [b]%d[/b] %+d = [b]%d[/b]  contra  CD %d[/center]" % [NOMBRES_STAT.get(info["stat"], info["stat"]), d20, info["bono"], d20 + info["bono"], info["cd"]])
	tw.tween_interval(1.1)
	tw.tween_callback(_aplicar.bind(indice))

func _aplicar(indice: int) -> void:
	var op: Resource = evento.opciones[indice]
	var exito: bool = ultima_tirada["exito"]
	var pm := _pm()
	# Imagen del resultado: la de la opción, si no la del evento, si no el placeholder
	var img: Texture2D = op.imagen_exito if exito else op.imagen_fracaso
	if img == null:
		img = evento.imagen_exito if exito else evento.imagen_fracaso
	if img == null:
		img = PH_EXITO if exito else PH_FRACASO
	dado.ocultar()
	foto.texture = img
	foto.visible = true
	# Efectos
	var efectos: Array = (op.efectos_exito if exito else op.efectos_fracaso).duplicate()
	if ultima_tirada["critico"]:
		efectos.append_array(op.efectos_critico)
	if ultima_tirada["pifia"]:
		efectos.append_array(op.efectos_pifia)
	var resumen := []
	if not efectos.is_empty() and pm:
		var t: String = pm.call("AplicarEfectosA", str(heroe.name), efectos)
		if t != "":
			resumen.append(t)
	if not exito:
		heroe.set_meta("sanidad_actual", maxi(0, int(heroe.get_meta("sanidad_actual", 100)) - 10))
		resumen.append("-10 cordura")
	var fn: String = op.funcion_exito if exito else op.funcion_fracaso
	var param: int = op.parametro_exito if exito else op.parametro_fracaso
	if fn != "" and pm:
		pm.call(fn, param, heroe)
		if fn == "RecibirDano":
			resumen.append("-%d PV" % param)
	var cabecera := "[color=#f0c040]¡CRÍTICO![/color]\n" if ultima_tirada["critico"] else ("[color=#e05040]¡PIFIA![/color]\n" if ultima_tirada["pifia"] else "")
	var texto: String = op.texto_exito if exito else op.texto_fracaso
	var color := "#8fd070" if exito else "#e07060"
	historia.text = "%s%s%s" % [cabecera, texto, ("\n\n[color=%s]%s[/color]" % [color, ", ".join(resumen)]) if not resumen.is_empty() else ""]
	var audio := get_node_or_null("/root/AudioManager")
	if audio:
		audio.play_sfx("success" if exito else "fail")
	var ui := get_node_or_null("/root/Main/CanvasLayer")
	if ui:
		ui.call("UpdateWarriorUI")
	for c in botones.get_children():
		c.visible = c == continuar

func cerrar() -> void:
	if not visible:
		return
	var tw := create_tween()
	tw.tween_property(self, "position:y", get_viewport_rect().size.y, 0.4).set_trans(Tween.TRANS_CUBIC).set_ease(Tween.EASE_IN)
	tw.tween_callback(hide)
	var dm := _dm()
	if dm:
		dm.call("BajarMapa")
		dm.call("FinalizarEvento")
	evento = null
	heroe = null

## Sin animación (cambio de piso, depuración)
func cerrar_ya() -> void:
	visible = false
	resolviendo = false
	evento = null
	heroe = null
	var dm := _dm()
	if dm:
		dm.call("BajarMapa")
