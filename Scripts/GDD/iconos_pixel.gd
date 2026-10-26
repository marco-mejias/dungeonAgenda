class_name IconosPixel
extends RefCounted

# Iconos pixel art dibujados a partir de rejillas de caracteres (menú táctico, cursor...).
#   '.' transparente  '#' contorno  'w' claro  'g' oro  'y' amarillo  'r' rojo  'b' azul

const PALETA := {
	"#": Color(0.05, 0.04, 0.08),
	"w": Color(0.95, 0.93, 0.85),
	"g": Color(0.9, 0.68, 0.2),
	"y": Color(1.0, 0.86, 0.3),
	"r": Color(0.92, 0.16, 0.1),
	"b": Color(0.42, 0.72, 1.0),
}

const DIBUJOS := {
	"espada": [
		".......##",
		"......#w#",
		".....#w#.",
		"....#w#..",
		"#..#w#...",
		".##w#....",
		"..#g#....",
		".#g.##...",
		"#.#......",
	],
	"chispa": [
		"....y....",
		"....y....",
		"...yyy...",
		"..yywyy..",
		"yyywwwyyy",
		"..yywyy..",
		"...yyy...",
		"....y....",
		"....y....",
	],
	"reloj": [
		"#########",
		"#wwwwwww#",
		".#wyyyw#.",
		"..#wyw#..",
		"...#y#...",
		"..#wyw#..",
		".#wwyww#.",
		"#wyyyyyw#",
		"#########",
	],
	"flecha": [
		"#...........",
		"##..........",
		"#w#.........",
		"#ww#........",
		"#www#.......",
		"#wwww#......",
		"#wwwww#.....",
		"#wwwwww#....",
		"#wwww####...",
		"#w##w#......",
		"##..#w#.....",
		".....##.....",
	],
	"mano": [
		"...##.......",
		"..#ww#......",
		"..#ww#......",
		"..#ww###....",
		"..#ww#ww##..",
		"###ww#ww#w#.",
		"#ww#wwwwwww#",
		"#wwwwwwwwww#",
		".#wwwwwwwww#",
		"..#wwwwwww#.",
		"...#wwwwww#.",
		"....######..",
	],
	"aspa": [
		"##......##",
		"#r#....#r#",
		".#r#..#r#.",
		"..#r##r#..",
		"...#rr#...",
		"...#rr#...",
		"..#r##r#..",
		".#r#..#r#.",
		"#r#....#r#",
		"##......##",
	],
}

static func imagen(nombre: String, escala: int = 1) -> Image:
	var filas: Array = DIBUJOS.get(nombre, DIBUJOS["flecha"])
	var alto := filas.size()
	var ancho: int = filas[0].length()
	var img := Image.create_empty(ancho * escala, alto * escala, false, Image.FORMAT_RGBA8)
	img.fill(Color(0, 0, 0, 0))
	for y in alto:
		var fila: String = filas[y]
		for x in ancho:
			var c := fila[x]
			if c == ".":
				continue
			img.fill_rect(Rect2i(x * escala, y * escala, escala, escala), PALETA.get(c, Color.MAGENTA))
	return img

static func textura(nombre: String, escala: int = 1) -> ImageTexture:
	return ImageTexture.create_from_image(imagen(nombre, escala))
