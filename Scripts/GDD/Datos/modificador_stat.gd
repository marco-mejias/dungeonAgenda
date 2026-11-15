@tool
class_name ModificadorStat
extends Resource

# A bonus (or penalty) to a stat: carried by equipment and enchantments. Final value = (base + sum
# of flats) * (1 + sum of percentages / 100)

@export_enum("hp_max", "velocidad", "fuerza", "defensa", "magia", "suerte", "radio_luz", "sanidad_max", "hambre_max") var stat: String = "fuerza"
@export var plano: int = 0
@export_range(-100, 300, 5, "suffix:%") var porcentaje: int = 0

# "+2", "+10%" or both
func describir() -> String:
	var partes: PackedStringArray = []
	if plano != 0:
		partes.append("%+d" % plano)
	if porcentaje != 0:
		partes.append("%+d%%" % porcentaje)
	return "%s %s" % [stat, " ".join(partes)]
