class_name PixelHD
extends RefCounted

# Escalado de pixel art "a mayor resolución" con Scale2x (EPX): cada píxel se convierte en 4 y
# las esquinas siguen las diagonales vecinas, así los bordes salen suavizados en vez de en bloques.
# Dos pasadas = x4 (un sprite de 16x16 pasa a 64x64). Se ve al mismo tamaño en pantalla, pero
# con escalones cuatro veces más finos. No inventa detalle: redondea lo que ya hay.

static var _cache := {}

static func _igual(a: Color, b: Color) -> bool:
	if a.a < 0.5 and b.a < 0.5:
		return true # todo lo transparente cuenta como lo mismo
	return a.is_equal_approx(b)

static func escalar2x(src: Image) -> Image:
	var w := src.get_width()
	var h := src.get_height()
	var dst := Image.create(w * 2, h * 2, false, Image.FORMAT_RGBA8)
	for y in h:
		for x in w:
			var p := src.get_pixel(x, y)
			var a := src.get_pixel(x, maxi(y - 1, 0))
			var b := src.get_pixel(mini(x + 1, w - 1), y)
			var c := src.get_pixel(maxi(x - 1, 0), y)
			var d := src.get_pixel(x, mini(y + 1, h - 1))
			var e0 := p
			var e1 := p
			var e2 := p
			var e3 := p
			if _igual(c, a) and not _igual(c, d) and not _igual(a, b):
				e0 = a
			if _igual(a, b) and not _igual(a, c) and not _igual(b, d):
				e1 = b
			if _igual(d, c) and not _igual(d, b) and not _igual(c, a):
				e2 = c
			if _igual(b, d) and not _igual(b, a) and not _igual(d, c):
				e3 = d
			dst.set_pixel(x * 2, y * 2, e0)
			dst.set_pixel(x * 2 + 1, y * 2, e1)
			dst.set_pixel(x * 2, y * 2 + 1, e2)
			dst.set_pixel(x * 2 + 1, y * 2 + 1, e3)
	return dst

# Imagen x4 (dos pasadas de Scale2x)
static func imagen_x4(src: Image) -> Image:
	var im := src.duplicate() as Image
	if im.is_compressed():
		im.decompress()
	im.convert(Image.FORMAT_RGBA8)
	return escalar2x(escalar2x(im))

# Textura x4 de otra textura (también de una AtlasTexture: se queda solo con su región)
static func textura_x4(tex: Texture2D) -> Texture2D:
	if tex == null:
		return null
	if _cache.has(tex):
		return _cache[tex]
	var im := tex.get_image()
	if im == null:
		return tex
	var hd := ImageTexture.create_from_image(imagen_x4(im))
	_cache[tex] = hd
	return hd
