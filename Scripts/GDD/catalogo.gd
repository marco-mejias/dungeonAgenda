extends Node

# Catalogo (autoload): at startup it reads everything in Data/Objetos, Data/Botin and Data/Enemigos
# (BaseDatos.cargar_todo). To add something you just create its .tres in one of those folders (or
# subfolders): no code to touch. The rest of the game (C++ too) asks for things by id.

var objetos: Dictionary = {}  # id -> ObjetoData
var tablas: Dictionary = {}   # id -> TablaBotin
var enemigos: Dictionary = {} # id -> EnemigoData
var avisos: PackedStringArray = []

func _ready() -> void:
	recargar()

# desde_disco: reads the .tres again even if they're already in memory (to edit them with the game
# running)
func recargar(desde_disco: bool = false) -> void:
	var datos := BaseDatos.cargar_todo(desde_disco)
	objetos = datos["objetos"]
	tablas = datos["tablas"]
	enemigos = datos["enemigos"]
	avisos = datos["avisos"]
	for a in avisos:
		push_warning("[Catalogo] " + a)
	print("[Catalogo] %d objetos, %d tablas de botín, %d enemigos" % [objetos.size(), tablas.size(), enemigos.size()])

# ==========================================
# QUERIES (C++ uses them too)
# ==========================================

# The ObjetoData with this id (null if it doesn't exist)
func obtener(id: String) -> ObjetoData:
	if objetos.is_empty():
		recargar() # In case someone asks before our _ready
	return objetos.get(id)

func existe(id: String) -> bool:
	return objetos.has(id)

# Every item id of one type (weapon, armour, consumable...)
func ids_de_tipo(tipo: int) -> PackedStringArray:
	var out: PackedStringArray = []
	for id in objetos:
		if objetos[id].tipo == tipo:
			out.append(id)
	return out

func tabla(id: String) -> TablaBotin:
	return tablas.get(id)

# List of {recurso, id, cantidad}; empty if the table doesn't exist
func tirar_botin(id_tabla: String, piso: int) -> Array:
	var t: TablaBotin = tablas.get(id_tabla)
	if t == null:
		push_warning("[Catalogo] no existe la tabla de botín '%s'" % id_tabla)
		return []
	return t.tirar(piso)

func enemigo(id: String) -> EnemigoData:
	return enemigos.get(id)

# A random enemy type among the valid ones for this floor (by its spawn weight)
func elegir_enemigo(piso: int) -> EnemigoData:
	if enemigos.is_empty():
		recargar()
	var validos: Array = []
	var total := 0
	for id in enemigos:
		var e: EnemigoData = enemigos[id]
		if e.vale_en_piso(piso) and e.peso_aparicion > 0:
			validos.append(e)
			total += e.peso_aparicion
	if total <= 0:
		return null
	var r := randi() % total
	for e in validos:
		r -= e.peso_aparicion
		if r < 0:
			return e
	return validos[-1]
