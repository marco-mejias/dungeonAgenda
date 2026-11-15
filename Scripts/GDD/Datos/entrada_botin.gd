@tool
class_name EntradaBotin
extends Resource

# One line of a loot table: an item, gold or food, with its probability weight.

enum Recurso { OBJETO, ORO, COMIDA }

@export var recurso: Recurso = Recurso.OBJETO
@export var objeto: ObjetoData
@export var cantidad_min: int = 1
@export var cantidad_max: int = 1
## The higher, the more likely compared to the other entries in the table.
@export var peso: int = 10
@export var piso_min: int = 1
## 0 = no limit
@export var piso_max: int = 0

# Can this entry drop on this floor? (piso_max 0 = no limit)
func vale_en_piso(piso: int) -> bool:
	return piso >= piso_min and (piso_max == 0 or piso <= piso_max)
