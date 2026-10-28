@tool
class_name Efecto
extends Resource

# Una pieza de efecto reutilizable: la usan los consumibles (al usarlos), las armas
# (al golpear), las opciones de los eventos y, más adelante, las habilidades.
# El motor que los aplica está en Scripts/CPP/Inventario/Efectos.hpp.

enum Tipo {
	CURAR,          # +valor de vida (sin pasar del máximo)
	DANAR,          # -valor de vida
	CORDURA,        # +/- valor de cordura
	HAMBRE,         # +/- valor de saciedad
	PA,             # +/- valor de PA este turno
	ORO,            # +/- valor de oro
	COMIDA,         # +/- valor de raciones en la bolsa
	MODIFICAR_STAT, # +valor a 'stat' durante 'duracion_turnos' días (0 = hasta el final del piso)
	DAR_OBJETO,     # mete 'objeto' x valor en el inventario
}

@export var tipo: Tipo = Tipo.CURAR
@export var valor: int = 10
@export_range(0, 100, 1, "suffix:%") var probabilidad: int = 100 # 100 = siempre

@export_group("Solo MODIFICAR_STAT")
@export_enum("hp_max", "velocidad", "fuerza", "defensa", "magia", "suerte", "radio_luz", "sanidad_max", "hambre_max") var stat: String = "fuerza"
@export var duracion_turnos: int = 3

@export_group("Solo DAR_OBJETO")
@export var objeto: ObjetoData

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
