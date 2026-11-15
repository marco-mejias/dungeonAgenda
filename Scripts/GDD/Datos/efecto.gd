@tool
class_name Efecto
extends Resource

# A reusable effect piece: used by consumables (when used), weapons (on hit), event options and,
# later on, abilities. The engine that applies them lives in Scripts/CPP/Inventario/Inventario.hpp.

enum Tipo {
	CURAR,          # +valor health (not over the max)
	DANAR,          # -valor health
	CORDURA,        # +/- valor sanity
	HAMBRE,         # +/- valor satiety
	PA,             # +/- valor PA this turn
	ORO,            # +/- valor gold
	COMIDA,         # +/- valor rations in the bag
	MODIFICAR_STAT, # +valor to 'stat' for 'duracion_turnos' days (0 = until the end of the floor)
	DAR_OBJETO,     # Puts 'objeto' x valor in the inventory
}

@export var tipo: Tipo = Tipo.CURAR
@export var valor: int = 10
@export_range(0, 100, 1, "suffix:%") var probabilidad: int = 100 # 100 = always

@export_group("Solo MODIFICAR_STAT")
@export_enum("hp_max", "velocidad", "fuerza", "defensa", "magia", "suerte", "radio_luz", "sanidad_max", "hambre_max") var stat: String = "fuerza"
@export var duracion_turnos: int = 3

@export_group("Solo DAR_OBJETO")
@export var objeto: ObjetoData

# Short readable text for the effect ("Cura 10")
func describir() -> String:
	match tipo:
		Tipo.CURAR: return "Cura %d" % valor
		Tipo.DANAR: return "%d de daño" % valor
		Tipo.CORDURA: return "%+d cordura" % valor
		Tipo.HAMBRE: return "%+d saciedad" % valor
		Tipo.PA: return "%+d PA" % valor
		Tipo.ORO: return "%+d oro" % valor
		Tipo.COMIDA: return "%+d comida" % valor
		Tipo.MODIFICAR_STAT: return "%s %+d (%d días)" % [stat, valor, duracion_turnos]
		Tipo.DAR_OBJETO: return "Da %s" % (objeto.nombre if objeto else "?")
	return ""
