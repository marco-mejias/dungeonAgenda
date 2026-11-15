@tool
@icon("res://Assets/Iconos/Objetos/anillo_suerte.png")
class_name ObjetoData
extends Resource

# Item definition (what you create in the Inspector). Heroes don't keep the resource: they keep its
# 'id' and the amount. The Catalogo (autoload) finds them by id.

enum Tipo { ARMA, ARMADURA, ACCESORIO, LUZ, MOCHILA, CONSUMIBLE, COMIDA, LLAVE, MATERIAL }

@export_group("Identidad")
## Unique identifier. If you leave it empty the file name is used (espada_corta.tres ->
## "espada_corta").
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
## Uses before it's spent (0 = never spent). Consumables usually have 1.
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

# Name plus its enchantments ("Espada corta Afilada")
func nombre_completo() -> String:
	var n := nombre
	for e in encantamientos:
		if e:
			n += " " + e.nombre
	return n
