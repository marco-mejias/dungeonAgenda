extends Node

# Catálogo (autoload): al arrancar lee todo lo que hay en Data/Objetos, Data/Botin y
# Data/Enemigos (BaseDatos.cargar_todo). Para añadir algo basta con crear su .tres en una
# de esas carpetas (o subcarpetas): no hay que tocar código. El resto del juego (también
# el C++) pide las cosas por id.

var objetos: Dictionary = {}  # id -> ObjetoData
var tablas: Dictionary = {}   # id -> TablaBotin
var enemigos: Dictionary = {} # id -> EnemigoData
var avisos: PackedStringArray = []

func _ready() -> void:
	recargar()

# desde_disco: vuelve a leer los .tres aunque ya estén en memoria (para editarlos con el juego en marcha)
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
# CONSULTAS (las usa también el C++)
# ==========================================

func obtener(id: String) -> ObjetoData:
	if objetos.is_empty():
		recargar() # por si alguien pregunta antes de nuestro _ready
	return objetos.get(id)

func existe(id: String) -> bool:
	return objetos.has(id)

func ids_de_tipo(tipo: int) -> PackedStringArray:
	var out: PackedStringArray = []
	for id in objetos:
		if objetos[id].tipo == tipo:
			out.append(id)
	return out

func tabla(id: String) -> TablaBotin:
	return tablas.get(id)

# Lista de {recurso, id, cantidad}; vacía si la tabla no existe
func tirar_botin(id_tabla: String, piso: int) -> Array:
	var t: TablaBotin = tablas.get(id_tabla)
	if t == null:
		push_warning("[Catalogo] no existe la tabla de botín '%s'" % id_tabla)
		return []
	return t.tirar(piso)

func enemigo(id: String) -> EnemigoData:
	return enemigos.get(id)

# Un tipo de enemigo al azar entre los que valen para este piso (según su peso de aparición)
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
