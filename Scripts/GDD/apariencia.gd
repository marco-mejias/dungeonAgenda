extends Node

# Apariencia (autoload): builds every hero's sprite = their person + whatever they have equipped +
# their hands, in a 4-frame sheet (64x16), and it's the ONLY clock for their idle animation. The
# map, the backpack and the campfire read from here, so they always look the same and on the same
# frame. The pieces come from Tools/Sprites/objetos.py (common 16x16 template, same 4 frames).

const PERSONAS := preload("res://Assets/Sprites/Heroes32/personas.png") # One row per hero, without hands
const MANOS := preload("res://Assets/Sprites/Heroes32/manos.png")
const POLLO := preload("res://Assets/Sprites/Heroes32/gallina.png") # "What could possibly go wrong?" potion
const FILA := {"Ladron": 0, "Mago": 1, "Caballero": 2, "Tanque": 3}
const FOTOGRAMAS := 4
const FPS := 1000.0 / 220.0 # 220 ms per frame, like in LibreSprite
## Back to front. "persona" and "manos" are the hero; the rest are equipment slots.
const ORDEN := ["mochila", "luz", "persona", "cuerpo", "cabeza", "mano_secundaria", "mano_principal", "manos", "accesorio_1", "accesorio_2"]
const RUTA_DM := "/root/Main/ContenedorMapa/SubViewport/DungeonManager"
const RUTA_PM := "/root/Main/PartyManager"

var _imagenes := {}  # Texture2D -> decompressed Image
var _hojas := {}     # signature -> ImageTexture
var _firma_puesta := {} # hero name -> signature of the sheet that holds their map sprite

func _ready() -> void:
	process_mode = Node.PROCESS_MODE_ALWAYS

# --- Shared clock ---

## Idle frame (0..3) of a hero right now. Every hero is slightly out of phase.
func fotograma(h: Node) -> int:
	var desfase := 0
	if h:
		desfase = int(FILA.get(str(h.get_meta("clase", "")), 0)) * 3 + h.get_index()
	return (int(Time.get_ticks_msec() / 1000.0 * FPS) + desfase) % FOTOGRAMAS

# --- Pieces ---

# Decompressed Image of a texture (cached: get_image() is slow)
func _imagen(tex: Texture2D) -> Image:
	if tex == null:
		return null
	if not _imagenes.has(tex):
		var im := tex.get_image()
		if im and im.is_compressed():
			im.decompress()
		if im:
			im.convert(Image.FORMAT_RGBA8)
		_imagenes[tex] = im
	return _imagenes[tex]

# Row of the hero sheet for their class
func _fila(h: Node) -> int:
	return int(FILA.get(str(h.get_meta("clase", "")), 2))

# The EquipoData in that slot, if it has a worn sprite
func _objeto(h: Node, ranura: String) -> EquipoData:
	var eq: Dictionary = h.get_meta("equipo", {})
	if not eq.has(ranura):
		return null
	var cat := get_node_or_null("/root/Catalogo")
	var o = cat.obtener(str(eq[ranura])) if cat else null
	return o if o is EquipoData else null

## 64x16 sheet (4 frames) of one layer: the person or the hands (their row) or the equipped piece.
func hoja_capa(h: Node, capa: String) -> Image:
	if capa == "persona" or capa == "manos":
		var im := _imagen(PERSONAS if capa == "persona" else MANOS)
		return im.get_region(Rect2i(0, _fila(h) * 16, 16 * FOTOGRAMAS, 16)) if im else null
	var o := _objeto(h, capa)
	if o == null or o.sprite_puesto == null:
		return null
	var im := _imagen(o.sprite_puesto)
	if im == null:
		return null
	if im.get_width() < 16 * FOTOGRAMAS: # Old single-frame piece: repeat it
		var rep := Image.create(16 * FOTOGRAMAS, 16, false, Image.FORMAT_RGBA8)
		for f in FOTOGRAMAS:
			rep.blit_rect(im, Rect2i(0, 0, 16, 16), Vector2i(f * 16, 0))
		return rep
	return im

# What makes two sprites identical: class + equipment (+ chicken). Used as the cache key
func _firma(h: Node) -> String:
	return str(h.get_meta("clase", "")) + "|" + var_to_str(h.get_meta("equipo", {})) + ("|pollo" if h.get_meta("es_pollo", false) else "")

## The whole hero (person + equipment + hands) in a 4-frame sheet.
func hoja(h: Node) -> ImageTexture:
	var firma := _firma(h)
	if _hojas.has(firma):
		return _hojas[firma]
	if h.get_meta("es_pollo", false):
		var pollo := ImageTexture.create_from_image(_imagen(POLLO))
		_hojas[firma] = pollo
		return pollo
	var im := Image.create(16 * FOTOGRAMAS, 16, false, Image.FORMAT_RGBA8)
	for capa in ORDEN:
		var c := hoja_capa(h, capa)
		if c:
			im.blend_rect(c, Rect2i(0, 0, 16 * FOTOGRAMAS, 16), Vector2i.ZERO)
	var tex := ImageTexture.create_from_image(im)
	_hojas[firma] = tex
	return tex

## One frame of the whole hero, for a TextureRect (portraits, campfire...).
func textura(h: Node, f: int = -1) -> AtlasTexture:
	var at := AtlasTexture.new()
	at.atlas = hoja(h)
	at.region = Rect2((fotograma(h) if f < 0 else f) * 16, 0, 16, 16)
	return at

var _texturas_capa := {} # Image -> ImageTexture

## One frame of ONE layer (person, hands or the piece in a slot), to draw them separately in the
## backpack (highlight or drag a piece). null if that slot is empty.
func textura_capa(h: Node, capa: String, f: int) -> AtlasTexture:
	var im := hoja_capa(h, capa)
	if im == null:
		return null
	if not _texturas_capa.has(im):
		_texturas_capa[im] = ImageTexture.create_from_image(im)
	var at := AtlasTexture.new()
	at.atlas = _texturas_capa[im]
	at.region = Rect2(f * 16, 0, 16, 16)
	return at

# --- The map ---

func _process(_dt: float) -> void:
	var dm := get_node_or_null(RUTA_DM)
	var pm := get_node_or_null(RUTA_PM)
	if not dm or not pm:
		return
	for h in pm.get_children():
		if h.name == "DatosEquipo":
			continue
		var cuerpo := dm.get_node_or_null(NodePath(str(h.name) + "/Cuerpo")) as Sprite3D
		if not cuerpo:
			continue
		var firma := _firma(h)
		if _firma_puesta.get(h.name, "") != firma or cuerpo.texture != _hojas.get(firma):
			cuerpo.texture = hoja(h)
			cuerpo.hframes = FOTOGRAMAS
			cuerpo.vframes = 1
			cuerpo.set_meta("anim_externa", true) # The C++ renderer doesn't touch its frame
			_firma_puesta[h.name] = firma
		cuerpo.frame = fotograma(h)
