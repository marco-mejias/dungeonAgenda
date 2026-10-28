extends Node

# Estilo (autoload): la interfaz "piedra tallada" en un solo sitio.
#   - Paleta con significado fijo: vida = rojo, PA = oro, cordura = violeta, hambre = verde;
#     el oro también marca lo seleccionado / el héroe activo.
#   - Marcos pixel art de Assets/UI/Piedra (generados con Tools/generar_ui_piedra.gd):
#     bloques en relieve para ventanas y botones, huecos tallados para casillas y barras.
#   - crear_tema() monta el Theme; Tools/guardar_tema_piedra.gd lo guarda como tema del
#     proyecto (gui/theme/custom), así lo heredan todos los Control y se ve también en el editor.
# El C++ y los scripts piden aquí sus cajas y colores: Estilo.caja("panel"), Estilo.color("oro").

const DIR := "res://Assets/UI/Piedra/"
const ESCALA := 2 # píxeles reales por píxel de arte

const COLORES := {
	"hueso": Color("d9d6cc"),        # texto principal
	"texto_suave": Color("9a9cab"),  # texto secundario
	"texto_off": Color("62647a"),
	"oro": Color("d7b24a"),          # acento: seleccionado, héroe activo, PA
	"oro_claro": Color("f0d27a"),
	"vida": Color("b8433a"),
	"pa": Color("d7b24a"),
	"cordura": Color("8a63c9"),
	"hambre": Color("6f9a46"),
	"bien": Color("8fcf6a"),         # mejora / éxito
	"mal": Color("e0675a"),          # empeora / error
	"piedra": Color("3b3d4a"),
	"piedra_oscura": Color("22232b"),
	"piedra_clara": Color("6c6f80"),
	"sombra": Color("15161b"),
	"enemigo": Color("8c3a32"),
	"velo": Color(0.03, 0.03, 0.05, 0.62), # fondo oscurecido tras una ventana
}

# nombre -> [archivo, margen en píxeles de arte, relleno extra del contenido]
const CAJAS := {
	"panel": ["panel", 3, 6], "panel_oro": ["panel_oro", 3, 6], "panel_rojo": ["panel_rojo", 3, 6],
	"panel_alerta": ["panel_alerta", 3, 6], "tooltip": ["tooltip", 3, 5],
	"ficha": ["panel", 3, 1], "ficha_activa": ["panel_oro", 3, 1], "ficha_enemigo": ["panel_rojo", 3, 1], "ficha_alerta": ["panel_alerta", 3, 1],
	"boton": ["boton", 3, 5], "boton_hover": ["boton_hover", 3, 5], "boton_pulsado": ["boton_pulsado", 3, 5], "boton_off": ["boton_off", 3, 5],
	"hueco": ["hueco", 3, 3], "hueco_oro": ["hueco_oro", 3, 3], "hueco_hover": ["hueco_hover", 3, 3],
	"barra_fondo": ["barra_fondo", 2, 0],
	"barra_vida": ["barra_vida", 1, 0], "barra_pa": ["barra_pa", 1, 0], "barra_cordura": ["barra_cordura", 1, 0], "barra_hambre": ["barra_hambre", 1, 0],
}

var _cajas := {}

func color(nombre: String) -> Color:
	return COLORES.get(nombre, Color.MAGENTA)

func caja(nombre: String) -> StyleBox:
	if _cajas.has(nombre):
		return _cajas[nombre]
	var def: Array = CAJAS.get(nombre, CAJAS["panel"])
	var s := StyleBoxTexture.new()
	s.texture = load(DIR + def[0] + ".png")
	var m: int = def[1] * ESCALA
	s.texture_margin_left = m
	s.texture_margin_top = m
	s.texture_margin_right = m
	s.texture_margin_bottom = m
	s.axis_stretch_horizontal = StyleBoxTexture.AXIS_STRETCH_MODE_TILE_FIT
	s.axis_stretch_vertical = StyleBoxTexture.AXIS_STRETCH_MODE_TILE_FIT
	var c: int = m + def[2]
	s.content_margin_left = c
	s.content_margin_right = c
	s.content_margin_top = maxi(0, c - 1)
	s.content_margin_bottom = maxi(0, c - 1)
	_cajas[nombre] = s
	return s

# Etiqueta con la letra del juego en uno de los tres tamaños fijos (titulo 22, normal 14, pequeno 11)
func etiqueta(texto: String, tamano: String = "normal", color_nombre: String = "hueso") -> Label:
	var l := Label.new()
	l.text = texto
	l.add_theme_font_size_override("font_size", {"titulo": 22, "normal": 14, "pequeno": 11}.get(tamano, 14))
	l.add_theme_color_override("font_color", color(color_nombre))
	return l

func crear_tema() -> Theme:
	var t := Theme.new()
	var hueso := color("hueso")
	var oro := color("oro")
	var vacio := StyleBoxEmpty.new()

	for clase in ["Button", "OptionButton", "MenuButton", "CheckBox", "CheckButton"]:
		t.set_stylebox("normal", clase, caja("boton"))
		t.set_stylebox("hover", clase, caja("boton_hover"))
		t.set_stylebox("pressed", clase, caja("boton_pulsado"))
		t.set_stylebox("hover_pressed", clase, caja("boton_pulsado"))
		t.set_stylebox("disabled", clase, caja("boton_off"))
		t.set_stylebox("focus", clase, vacio)
		t.set_color("font_color", clase, hueso)
		t.set_color("font_hover_color", clase, color("oro_claro"))
		t.set_color("font_pressed_color", clase, oro)
		t.set_color("font_hover_pressed_color", clase, oro)
		t.set_color("font_focus_color", clase, hueso)
		t.set_color("font_disabled_color", clase, color("texto_off"))
		t.set_color("font_outline_color", clase, color("sombra"))
	# Las casillas de verificación van sin marco de botón
	for clase in ["CheckBox", "CheckButton"]:
		for e in ["normal", "hover", "pressed", "hover_pressed", "disabled"]:
			t.set_stylebox(e, clase, vacio)

	t.set_stylebox("panel", "Panel", caja("panel"))
	t.set_stylebox("panel", "PanelContainer", caja("panel"))
	t.set_stylebox("panel", "PopupPanel", caja("panel"))
	t.set_stylebox("panel", "TooltipPanel", caja("tooltip"))
	t.set_color("font_color", "TooltipLabel", hueso)
	t.set_stylebox("panel", "AcceptDialog", caja("panel"))

	t.set_stylebox("panel", "PopupMenu", caja("tooltip"))
	t.set_stylebox("hover", "PopupMenu", caja("hueco_oro"))
	t.set_color("font_color", "PopupMenu", hueso)
	t.set_color("font_hover_color", "PopupMenu", color("oro_claro"))
	t.set_color("font_disabled_color", "PopupMenu", color("texto_off"))
	t.set_color("font_separator_color", "PopupMenu", color("texto_suave"))

	t.set_color("font_color", "Label", hueso)
	t.set_color("font_outline_color", "Label", color("sombra"))
	t.set_color("default_color", "RichTextLabel", hueso)

	t.set_stylebox("background", "ProgressBar", caja("barra_fondo"))
	t.set_stylebox("fill", "ProgressBar", caja("barra_pa"))

	for clase in ["LineEdit", "TextEdit"]:
		t.set_stylebox("normal", clase, caja("hueco"))
		t.set_stylebox("focus", clase, caja("hueco_oro"))
		t.set_stylebox("read_only", clase, caja("hueco"))
		t.set_color("font_color", clase, hueso)
		t.set_color("caret_color", clase, oro)

	t.set_stylebox("slider", "HSlider", caja("barra_fondo"))
	t.set_stylebox("grabber_area", "HSlider", caja("barra_pa"))
	t.set_stylebox("grabber_area_highlight", "HSlider", caja("barra_pa"))

	for clase in ["VScrollBar", "HScrollBar"]:
		t.set_stylebox("scroll", clase, caja("barra_fondo"))
		t.set_stylebox("grabber", clase, caja("boton"))
		t.set_stylebox("grabber_highlight", clase, caja("boton_hover"))
		t.set_stylebox("grabber_pressed", clase, caja("boton_pulsado"))
	return t
