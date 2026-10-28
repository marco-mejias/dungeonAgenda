@tool
class_name MochilaData
extends EquipoData

# La mochila que lleva el héroe decide cuánto peso cabe en su inventario.

@export_group("Mochila")
@export_range(1.0, 100.0, 0.5, "suffix:kg") var capacidad: float = 10.0

func _init() -> void:
	tipo = Tipo.MOCHILA
	ranura = Ranura.MOCHILA
