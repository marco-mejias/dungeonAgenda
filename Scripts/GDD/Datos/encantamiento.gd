@tool
class_name Encantamiento
extends Resource

# Hueco para los objetos encantados (todavía no salen al azar): un nombre que se
# añade al del objeto, bonos de stats y efectos extra al golpear.

@export var nombre: String = "de Fuego"
@export var modificadores: Array[ModificadorStat] = []
@export var efectos_al_golpear: Array[Efecto] = []
