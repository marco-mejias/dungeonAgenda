extends Node

# Catálogo (autoload): al arrancar lee todo lo que hay en Data/Objetos y Data/Botin.
# Para añadir un objeto basta con crear su .tres en una de esas carpetas (o subcarpetas):
# no hay que tocar código. El resto del juego (también el C++) pide las cosas por id.

const CARPETA_OBJETOS := "res://Data/Objetos"
const CARPETA_BOTIN := "res://Data/Botin"
const CARPETA_ENEMIGOS := "res://Data/Enemigos"

var objetos: Dictionary = {} # id -> ObjetoData
var tablas: Dictionary = {}  # id -> TablaBotin
var enemigos: Dictionary = {} # id -> EnemigoData
var avisos: PackedStringArray = []

func _ready() -> void:
	recargar()

# desde_disco: vuelve a leer los .tres aunque ya estén en memoria (para editarlos con el juego en marcha)
func recargar(desde_disco: bool = false) -> void:
	objetos.clear()
	tablas.clear()
	enemigos.clear()
	avisos.clear()
	for r in _cargar_carpeta(CARPETA_OBJETOS, desde_disco):
		if r is ObjetoData:
			_registrar_objeto(r)
	for r in _cargar_carpeta(CARPETA_BOTIN, desde_disco):
		if r is TablaBotin:
			tablas[r.obtener_id()] = r
	for r in _cargar_carpeta(CARPETA_ENEMIGOS, desde_disco):
		if r is EnemigoData:
			_registrar_enemigo(r)
	for a in avisos:
		push_warning("[Catalogo] " + a)
	print("[Catalogo] %d objetos, %d tablas de botín, %d enemigos" % [objetos.size(), tablas.size(), enemigos.size()])

func _registrar_objeto(o: ObjetoData) -> void:
	var id := o.obtener_id()
	if objetos.has(id):
		avisos.append("id repetido '%s' (%s y %s)" % [id, objetos[id].resource_path, o.resource_path])
		return
	if o.icono == null:
		avisos.append("'%s' no tiene icono" % id)
	if o.apilable and o.max_pila < 2:
		avisos.append("'%s' es apilable pero max_pila < 2" % id)
	if o is ArmaData and o.alcance_min > o.alcance_max:
		avisos.append("'%s': alcance_min > alcance_max" % id)
	objetos[id] = o

func _registrar_enemigo(e: EnemigoData) -> void:
	var id := e.obtener_id()
	if enemigos.has(id):
		avisos.append("enemigo con id repetido '%s'" % id)
		return
	if e.arma and not objetos.has(e.arma.obtener_id()):
		avisos.append("el arma de '%s' no está en Data/Objetos" % id)
	if e.botin and not tablas.has(e.botin.obtener_id()):
		avisos.append("la tabla de botín de '%s' no está en Data/Botin" % id)
	enemigos[id] = e

func _cargar_carpeta(ruta: String, desde_disco: bool = false) -> Array:
	var salida: Array = []
	var dir := DirAccess.open(ruta)
	if dir == null:
		return salida
	for sub in dir.get_directories():
		salida.append_array(_cargar_carpeta(ruta.path_join(sub), desde_disco))
	for f in dir.get_files():
		# En un juego exportado los .tres aparecen como .tres.remap
		var nombre := f.trim_suffix(".remap")
		if nombre.get_extension() in ["tres", "res"]:
			var modo := ResourceLoader.CACHE_MODE_REPLACE_DEEP if desde_disco else ResourceLoader.CACHE_MODE_REUSE
			var r := ResourceLoader.load(ruta.path_join(nombre), "", modo)
			if r:
				salida.append(r)
	return salida

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
