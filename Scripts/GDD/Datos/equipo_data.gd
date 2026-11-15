@tool
@icon("res://Assets/Iconos/Objetos/cota_malla.png")
class_name EquipoData
extends ObjetoData

# Item that is worn. Equipped items DON'T weigh in the backpack.

enum Ranura { MANO_PRINCIPAL, MANO_SECUNDARIA, CABEZA, CUERPO, ACCESORIO, LUZ, MOCHILA }

@export_group("Equipo")
@export var ranura: Ranura = Ranura.CUERPO
@export var modificadores: Array[ModificadorStat] = []
## Classes that can wear it (empty = all). E.g. "Caballero", "Ladron", "Mago", "Tanque".
@export var clases_permitidas: PackedStringArray = []
## How it looks when worn on the backpack doll: a 16x16 layer aligned with the hero sprite (head up,
## right hand on column 13, left on column 2). See Tools/generar_sprites_inventario.gd
@export var sprite_puesto: Texture2D

func es_equipo() -> bool:
	return true
