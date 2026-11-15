@tool
class_name Encantamiento
extends Resource

# Slot for enchanted items (they don't drop randomly yet): a name added to the item's, stat bonuses
# and extra on-hit effects.

@export var nombre: String = "de Fuego"
@export var modificadores: Array[ModificadorStat] = []
@export var efectos_al_golpear: Array[Efecto] = []
