@tool
class_name ObjetoData
extends Resource

# Definición de un objeto (lo que creas en el Inspector). Los héroes no guardan el
# recurso: guardan su 'id' y la cantidad. El Catálogo (autoload) los encuentra por id.

enum Tipo { ARMA, ARMADURA, ACCESORIO, LUZ, MOCHILA, CONSUMIBLE, COMIDA, LLAVE, MATERIAL }

@export_group("Identidad")
## Identificador único. Si lo dejas vacío se usa el nombre del archivo (espada_corta.tres -> "espada_corta").
@export var id: String = ""
@export var nombre: String = "Objeto"
@export_multiline var descripcion: String = ""
@export var icono: Texture2D
@export var tipo: Tipo = Tipo.MATERIAL

@export_group("Inventario")
@export_range(0.0, 50.0, 0.1, "suffix:kg") var peso: float = 1.0
@export var valor: int = 1
@export var apilable: bool = false
@export var max_pila: int = 1
## Usos antes de gastarse (0 = no se gasta). Los consumibles suelen tener 1.
@export var usos: int = 0

@export_group("Al usarlo")
@export var efectos_al_usar: Array[Efecto] = []

@export_group("Encantamientos")
@export var encantamientos: Array[Encantamiento] = []

func obtener_id() -> String:
	if id != "":
		return id
	return resource_path.get_file().get_basename()

func es_equipo() -> bool:
	return false

func nombre_completo() -> String:
	var n := nombre
	for e in encantamientos:
		if e:
			n += " " + e.nombre
	return n
