extends Node

# Apariencia (autoload): monta el sprite de cada héroe = su persona + lo que lleva equipado + sus manos,
# en una hoja de 4 fotogramas (64x16), y es el ÚNICO reloj de su animación de reposo. El mapa, la mochila
# y la fogata leen de aquí, así que siempre se ven iguales y en el mismo fotograma.
# Las piezas salen de Tools/Sprites/objetos.py (plantilla común de 16x16, mismos 4 fotogramas).

const PERSONAS := preload("res://Assets/Sprites/Heroes32/personas.png") # una fila por héroe, sin manos
const MANOS := preload("res://Assets/Sprites/Heroes32/manos.png")
const POLLO := preload("res://Assets/Sprites/Heroes32/gallina.png") # poción "¿qué podría malir sal?"
const FILA := {"Ladron": 0, "Mago": 1, "Caballero": 2, "Tanque": 3}
const FOTOGRAMAS := 4
const FPS := 1000.0 / 220.0 # 220 ms por fotograma, como en LibreSprite
## De atrás adelante. "persona" y "manos" son el héroe; lo demás, ranuras de equipo.
const ORDEN := ["mochila", "luz", "persona", "cuerpo", "cabeza", "mano_secundaria", "mano_principal", "manos", "accesorio_1", "accesorio_2"]
const RUTA_DM := "/root/Main/ContenedorMapa/SubViewport/DungeonManager"
const RUTA_PM := "/root/Main/PartyManager"

var _imagenes := {}  # Texture2D -> Image descomprimida
var _hojas := {}     # firma -> ImageTexture
var _firma_puesta := {} # nombre del héroe -> firma de la hoja que tiene su sprite del mapa

func _ready() -> void:
	process_mode = Node.PROCESS_MODE_ALWAYS

# --- Reloj común ---

## Fotograma de reposo (0..3) de un héroe en este momento. Cada héroe va algo desfasado.
func fotograma(h: Node) -> int:
	var desfase := 0
	if h:
		desfase = int(FILA.get(str(h.get_meta("clase", "")), 0)) * 3 + h.get_index()
	return (int(Time.get_ticks_msec() / 1000.0 * FPS) + desfase) % FOTOGRAMAS

# --- Piezas ---

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

func _fila(h: Node) -> int:
	return int(FILA.get(str(h.get_meta("clase", "")), 2))

func _objeto(h: Node, ranura: String) -> EquipoData:
	var eq: Dictionary = h.get_meta("equipo", {})
	if not eq.has(ranura):
		return null
	var cat := get_node_or_null("/root/Catalogo")
	var o = cat.obtener(str(eq[ranura])) if cat else null
	return o if o is EquipoData else null

## Hoja de 64x16 (4 fotogramas) de una capa: la persona o las manos (su fila) o la pieza equipada.
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
	if im.get_width() < 16 * FOTOGRAMAS: # pieza antigua de un solo fotograma: la repite
		var rep := Image.create(16 * FOTOGRAMAS, 16, false, Image.FORMAT_RGBA8)
		for f in FOTOGRAMAS:
			rep.blit_rect(im, Rect2i(0, 0, 16, 16), Vector2i(f * 16, 0))
		return rep
	return im

func _firma(h: Node) -> String:
	return str(h.get_meta("clase", "")) + "|" + var_to_str(h.get_meta("equipo", {})) + ("|pollo" if h.get_meta("es_pollo", false) else "")

## El héroe entero (persona + equipo + manos) en una hoja de 4 fotogramas.
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

## Un fotograma del héroe entero, para un TextureRect (retratos, fogata...).
func textura(h: Node, f: int = -1) -> AtlasTexture:
	var at := AtlasTexture.new()
	at.atlas = hoja(h)
	at.region = Rect2((fotograma(h) if f < 0 else f) * 16, 0, 16, 16)
	return at

var _texturas_capa := {} # Image -> ImageTexture

## Un fotograma de UNA capa (persona, manos o la pieza de una ranura), para dibujarlas por separado
## en la mochila (resaltar o arrastrar una pieza). null si esa ranura está vacía.
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

# --- El mapa ---

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
			cuerpo.set_meta("anim_externa", true) # el renderer C++ no toca su fotograma
			_firma_puesta[h.name] = firma
		cuerpo.frame = fotograma(h)
