@tool
class_name ModificadorStat
extends Resource

# Un bono (o penalización) a una estadística: lo llevan el equipo y los encantamientos.
# Valor final = (base + suma de planos) * (1 + suma de porcentajes / 100)

@export_enum("hp_max", "velocidad", "fuerza", "defensa", "magia", "suerte", "radio_luz", "sanidad_max", "hambre_max") var stat: String = "fuerza"
@export var plano: int = 0
@export_range(-100, 300, 5, "suffix:%") var porcentaje: int = 0

func describir() -> String:
	var partes: PackedStringArray = []
	if plano != 0:
		partes.append("%+d" % plano)
	if porcentaje != 0:
		partes.append("%+d%%" % porcentaje)
	return "%s %s" % [stat, " ".join(partes)]
