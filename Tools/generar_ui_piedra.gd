extends SceneTree
# Genera los marcos pixel art de "piedra tallada" de la interfaz en res://Assets/UI/Piedra.
#   godot --headless --path . -s res://Tools/generar_ui_piedra.gd
# Cada píxel de arte ocupa 2x2 píxeles reales. Los marcos se usan como StyleBoxTexture
# de 9 trozos (ver Scripts/GDD/estilo.gd): bisel claro arriba-izquierda y oscuro abajo-derecha
# = bloque en relieve; al revés = hueco tallado en la piedra.

const ESCALA := 2
const DIR := "res://Assets/UI/Piedra"

const CONTORNO := Color("0e0f13")
const LUZ := Color("7a7d8e")
const LUZ_2 := Color("515465")
const SOMBRA := Color("15161b")
const SOMBRA_2 := Color("2a2b33")
const CARA := Color("3b3d4a")
const CARA_HOVER := Color("474a59")
const CARA_OFF := Color("2d2e37")
const HUECO := Color("22232b")
const HUECO_LUZ := Color("565868")
const ORO := Color("d7b24a")
const ORO_LUZ := Color("f0d27a")
const ROJO := Color("8c3a32")
const ROJO_VIVO := Color("d0473b")

var semilla := 7

# Ruido determinista para el moteado de la piedra
func _ruido(x: int, y: int) -> float:
	var h := (x * 374761393 + y * 668265263 + semilla * 1442695041) & 0x7fffffff
	h = (h ^ (h >> 13)) * 1274126177 & 0x7fffffff
	return float(h % 1000) / 1000.0

func _pintar(img: Image, x: int, y: int, c: Color) -> void:
	for dx in ESCALA:
		for dy in ESCALA:
			img.set_pixel(x * ESCALA + dx, y * ESCALA + dy, c)

func _moteado(base: Color, x: int, y: int, fuerza: float) -> Color:
	var r := _ruido(x, y)
	if r < 0.12:
		return base.darkened(fuerza)
	if r > 0.93:
		return base.lightened(fuerza * 0.8)
	return base

# Bloque (relieve) o hueco (tallado). lado = tamaño en píxeles de arte.
func marco(lado: int, cara: Color, arriba: Color, abajo: Color, arriba2: Color, abajo2: Color, contorno: Color, moteado: float, esquinas: bool = true) -> Image:
	var img := Image.create(lado * ESCALA, lado * ESCALA, false, Image.FORMAT_RGBA8)
	img.fill(Color(0, 0, 0, 0))
	var n := lado - 1
	for x in lado:
		for y in lado:
			var borde := mini(mini(x, y), mini(n - x, n - y))
			var c := cara
			if borde == 0:
				c = contorno
				if esquinas and (x == 0 or x == n) and (y == 0 or y == n):
					continue # esquina recortada: bloque achaflanado
			elif borde == 1:
				c = arriba if (x == 1 or y == 1) and not (x == n - 1 or y == n - 1) else abajo
				if (x == 1 and y == n - 1) or (x == n - 1 and y == 1):
					c = cara # las dos esquinas mixtas quedan neutras
			elif borde == 2:
				c = arriba2 if (x == 2 or y == 2) and not (x == n - 2 or y == n - 2) else abajo2
				if (x == 2 and y == n - 2) or (x == n - 2 and y == 2):
					c = cara
			else:
				c = _moteado(cara, x, y, moteado)
			_pintar(img, x, y, c)
	return img

# Relleno de barra: color con una línea de brillo arriba y una de sombra abajo
func barra(color: Color) -> Image:
	var lado := 6
	var img := Image.create(lado * ESCALA, lado * ESCALA, false, Image.FORMAT_RGBA8)
	for x in lado:
		for y in lado:
			var c := color
			if y == 0:
				c = color.lightened(0.35)
			elif y == lado - 1:
				c = color.darkened(0.35)
			_pintar(img, x, y, c)
	return img

func guardar(img: Image, nombre: String) -> void:
	img.save_png(ProjectSettings.globalize_path(DIR.path_join(nombre + ".png")))
	print("ui ", nombre)

func _init() -> void:
	DirAccess.make_dir_recursive_absolute(ProjectSettings.globalize_path(DIR))
	# Ventanas y fichas (bloques en relieve)
	guardar(marco(16, CARA, LUZ, SOMBRA, LUZ_2, SOMBRA_2, CONTORNO, 0.06), "panel")
	guardar(marco(16, CARA, LUZ, SOMBRA, LUZ_2, SOMBRA_2, ORO, 0.06), "panel_oro")
	guardar(marco(16, CARA, LUZ, SOMBRA, LUZ_2, SOMBRA_2, ROJO, 0.06), "panel_rojo")
	guardar(marco(16, CARA, LUZ, SOMBRA, LUZ_2, SOMBRA_2, ROJO_VIVO, 0.06), "panel_alerta")
	guardar(marco(16, Color("1b1c23"), Color("3b3d4a"), SOMBRA, Color("2a2b33"), Color("15161b"), CONTORNO, 0.04), "tooltip")
	# Botones
	guardar(marco(10, CARA, LUZ, SOMBRA, LUZ_2, SOMBRA_2, CONTORNO, 0.0), "boton")
	guardar(marco(10, CARA_HOVER, Color("8a8d9e"), SOMBRA, Color("5e6172"), SOMBRA_2, ORO, 0.0), "boton_hover")
	guardar(marco(10, HUECO, SOMBRA, HUECO_LUZ, SOMBRA_2, Color("3b3d4a"), ORO, 0.0), "boton_pulsado")
	guardar(marco(10, CARA_OFF, Color("454757"), Color("1a1b20"), Color("383a47"), Color("23242b"), CONTORNO, 0.0), "boton_off")
	# Huecos tallados (casillas, barras, campos de texto)
	guardar(marco(12, HUECO, SOMBRA, HUECO_LUZ, SOMBRA_2, Color("34363f"), CONTORNO, 0.05), "hueco")
	guardar(marco(12, HUECO, SOMBRA, HUECO_LUZ, SOMBRA_2, Color("34363f"), ORO, 0.05), "hueco_oro")
	guardar(marco(12, Color("2c2d37"), SOMBRA, HUECO_LUZ, SOMBRA_2, Color("34363f"), Color("8a8d9e"), 0.05), "hueco_hover") # ratón encima: gris claro (el oro es para lo elegido)
	guardar(marco(6, Color("121318"), SOMBRA, HUECO_LUZ, SOMBRA, Color("121318"), CONTORNO, 0.0, false), "barra_fondo")
	# Rellenos de barra: vida, PA, cordura, hambre
	guardar(barra(Color("b8433a")), "barra_vida")
	guardar(barra(Color("d7b24a")), "barra_pa")
	guardar(barra(Color("8a63c9")), "barra_cordura")
	guardar(barra(Color("6f9a46")), "barra_hambre")
	quit()
