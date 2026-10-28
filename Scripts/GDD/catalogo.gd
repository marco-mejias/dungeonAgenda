extends Node

# Catálogo (autoload): al arrancar lee todo lo que hay en Data/Objetos y Data/Botin.
# Para añadir un objeto basta con crear su .tres en una de esas carpetas (o subcarpetas):
# no hay que tocar código. El resto del juego (también el C++) pide las cosas por id.

const CARPETA_OBJETOS := "res://Data/Objetos"
const CARPETA_BOTIN := "res://Data/Botin"

var objetos: Dictionary = {} # id -> ObjetoData
var tablas: Dictionary = {}  # id -> TablaBotin
var avisos: PackedStringArray = []

func _ready() -> void:
	recargar()

func recargar() -> void:
	objetos.clear()
	tablas.clear()
	avisos.clear()
	for r in _cargar_carpeta(CARPETA_OBJETOS):
		if r is ObjetoData:
			_registrar_objeto(r)
	for r in _cargar_carpeta(CARPETA_BOTIN):
		if r is TablaBotin:
			tablas[r.obtener_id()] = r
	for a in avisos:
		push_warning("[Catalogo] " + a)
	print("[Catalogo] %d objetos, %d tablas de botín" % [objetos.size(), tablas.size()])

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

func _cargar_carpeta(ruta: String) -> Array:
	var salida: Array = []
	var dir := DirAccess.open(ruta)
	if dir == null:
		return salida
	for sub in dir.get_directories():
		salida.append_array(_cargar_carpeta(ruta.path_join(sub)))
	for f in dir.get_files():
		# En un juego exportado los .tres aparecen como .tres.remap
		var nombre := f.trim_suffix(".remap")
		if nombre.get_extension() in ["tres", "res"]:
			var r := load(ruta.path_join(nombre))
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
