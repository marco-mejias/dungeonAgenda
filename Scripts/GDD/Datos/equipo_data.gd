@tool
@icon("res://Assets/Iconos/Objetos/cota_malla.png")
class_name EquipoData
extends ObjetoData

# Objeto que se lleva puesto. Lo equipado NO pesa en la mochila.

enum Ranura { MANO_PRINCIPAL, MANO_SECUNDARIA, CABEZA, CUERPO, ACCESORIO, LUZ, MOCHILA }

@export_group("Equipo")
@export var ranura: Ranura = Ranura.CUERPO
@export var modificadores: Array[ModificadorStat] = []
## Clases que pueden llevarlo (vacío = todas). Ej: "Caballero", "Ladron", "Mago", "Tanque".
@export var clases_permitidas: PackedStringArray = []
## Cómo se ve puesto en el muñeco de la mochila: capa de 16x16 alineada con el sprite del héroe
## (cabeza arriba, mano derecha en la columna 13, izquierda en la 2). Ver Tools/generar_sprites_inventario.gd
@export var sprite_puesto: Texture2D

func es_equipo() -> bool:
	return true
