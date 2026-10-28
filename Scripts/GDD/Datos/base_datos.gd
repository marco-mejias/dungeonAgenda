@tool
class_name BaseDatos
extends RefCounted

# Lectura y validación de los datos del juego (Data/...). Lo usan el Catalogo (en partida)
# y el panel "Base de datos" del editor, así los dos avisan de los mismos errores.

const CARPETA_OBJETOS := "res://Data/Objetos"
const CARPETA_BOTIN := "res://Data/Botin"
const CARPETA_ENEMIGOS := "res://Data/Enemigos"

# Todos los .tres de una carpeta y sus subcarpetas.
# desde_disco: vuelve a leerlos aunque ya estén en memoria (para editarlos con el juego en marcha)
static func cargar_carpeta(ruta: String, desde_disco: bool = false) -> Array:
	var salida: Array = []
	var dir := DirAccess.open(ruta)
	if dir == null:
		return salida
	for sub in dir.get_directories():
		salida.append_array(cargar_carpeta(ruta.path_join(sub), desde_disco))
	for f in dir.get_files():
		# En un juego exportado los .tres aparecen como .tres.remap
		var nombre := f.trim_suffix(".remap")
		if nombre.get_extension() in ["tres", "res"]:
			var modo := ResourceLoader.CACHE_MODE_REPLACE_DEEP if desde_disco else ResourceLoader.CACHE_MODE_REUSE
			var r := ResourceLoader.load(ruta.path_join(nombre), "", modo)
			if r:
				salida.append(r)
	return salida

# Lee las tres carpetas: {objetos: id -> ObjetoData, tablas: id -> TablaBotin, enemigos: id -> EnemigoData, avisos}
static func cargar_todo(desde_disco: bool = false) -> Dictionary:
	var objetos := {}
	var tablas := {}
	var enemigos := {}
	var avisos: PackedStringArray = []
	for r in cargar_carpeta(CARPETA_OBJETOS, desde_disco):
		if r is ObjetoData:
			var id: String = r.obtener_id()
			if objetos.has(id):
				avisos.append("id repetido '%s' (%s y %s)" % [id, objetos[id].resource_path, r.resource_path])
			else:
				objetos[id] = r
	for r in cargar_carpeta(CARPETA_BOTIN, desde_disco):
		if r is TablaBotin:
			tablas[r.obtener_id()] = r
	for r in cargar_carpeta(CARPETA_ENEMIGOS, desde_disco):
		if r is EnemigoData:
			var id: String = r.obtener_id()
			if enemigos.has(id):
				avisos.append("enemigo con id repetido '%s'" % id)
			else:
				enemigos[id] = r
	avisos.append_array(validar(objetos, tablas, enemigos))
	return {"objetos": objetos, "tablas": tablas, "enemigos": enemigos, "avisos": avisos}

static func validar(objetos: Dictionary, tablas: Dictionary, enemigos: Dictionary) -> PackedStringArray:
	var avisos: PackedStringArray = []
	for id in objetos:
		var o: ObjetoData = objetos[id]
		if o.icono == null:
			avisos.append("'%s' no tiene icono" % id)
		if o.apilable and o.max_pila < 2:
			avisos.append("'%s' es apilable pero max_pila < 2" % id)
		if o is ArmaData and o.alcance_min > o.alcance_max:
			avisos.append("'%s': alcance_min > alcance_max" % id)
		if o is MochilaData and o.capacidad <= 0:
			avisos.append("'%s': la mochila no tiene capacidad" % id)
		for e in o.efectos_al_usar:
			if e and e.tipo == Efecto.Tipo.DAR_OBJETO and e.objeto == null:
				avisos.append("'%s': un efecto DAR_OBJETO no tiene objeto" % id)
	for id in tablas:
		var t: TablaBotin = tablas[id]
		if t.entradas.is_empty():
			avisos.append("la tabla '%s' está vacía" % id)
		for e in t.entradas:
			if e and e.recurso == EntradaBotin.Recurso.OBJETO and (e.objeto == null or not objetos.has(e.objeto.obtener_id())):
				avisos.append("la tabla '%s' tiene un objeto que no está en Data/Objetos" % id)
	for id in enemigos:
		var e: EnemigoData = enemigos[id]
		if e.arma and not objetos.has(e.arma.obtener_id()):
			avisos.append("el arma de '%s' no está en Data/Objetos" % id)
		if e.botin and not tablas.has(e.botin.obtener_id()):
			avisos.append("la tabla de botín de '%s' no está en Data/Botin" % id)
		if e.piso_max != 0 and e.piso_max < e.piso_min:
			avisos.append("'%s': piso_max < piso_min" % id)
	return avisos
