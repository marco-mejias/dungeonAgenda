@tool
class_name HabilidadData
extends Resource

# An ability learnt when levelling up (draft 1 of 3, Scripts/GDD/progreso.gd).
#   ACTIVA: cast from the ability bar. It has uses (recovered at the campfire, with the
#           elixir or when levelling up), a cooldown in days and a PA cost.
#   PASIVA: reacts on its own to a trigger (on hit, on kill...). If picked again in the
#           draft it stacks (more chance / stronger) without using another slot: combos.
# clase: "" = global (anyone can get it) or the class that has it in its pool.

enum Tipo { ACTIVA, PASIVA }
enum Objetivo { UNO_MISMO, ENEMIGO, ALIADO, CASILLA }
enum Disparador { AL_GOLPEAR, AL_CRITICO, AL_FALLAR, AL_MATAR, AL_RECIBIR_GOLPE, AL_EMPEZAR_DIA, AL_USAR_HABILIDAD }
enum Reaccion { DANO_EXTRA, REBOTE, CURAR, RECUPERAR_USO, BAJAR_ENFRIAMIENTO, PA_EXTRA, STAT_TEMPORAL, ORO, NO_GASTAR_USO, CONTRAGOLPE, APLICAR_ESTADO, NINGUNA }

@export_group("Identidad")
@export var id: String = ""
@export var nombre: String = "Habilidad"
@export_multiline var descripcion: String = ""
@export var icono: Texture2D
@export var clase: String = "" # "" (global), "Caballero", "Ladron", "Mago" or "Tanque"
@export var tipo: Tipo = Tipo.ACTIVA
@export var peso_draft: int = 10 # How often it shows up in the draft

@export_group("Activa")
@export var coste_pa: int = 2
@export var usos: int = 2
@export var enfriamiento: int = 1 # Days until it's ready again
@export var objetivo: Objetivo = Objetivo.ENEMIGO
@export var alcance_min: int = 1
@export var alcance_max: int = 1
@export var requiere_vision: bool = true
@export var area: int = 0          # Radius around the target (0 = only them)
@export var saltos: int = 0        # Chain lightning: how many more enemies it jumps to
@export var tirada: bool = true    # Rolls d20 against the AC (otherwise it always hits)
@export var dano_base: int = 0
@export_enum("fuerza", "magia", "suerte", "defensa") var stat_dano: String = "fuerza"
@export var mult_stat: float = 1.0
@export var efectos_objetivo: Array[Efecto] = [] # To the heroes hit (heals, buffs)
@export var efectos_propios: Array[Efecto] = []  # To the caster
@export var especial: String = ""  # Custom behaviour: chicken, taunt...

@export_group("Posicionamiento y estados")
## Caster movement before the effect: "" | carga (in a line to the target, +1 damage per tile) |
## teletransporte (to the chosen tile) | intercambio (with the chosen ally) | atraer (pulls the
## target until it's next to you)
@export var movimiento: String = ""
@export var empuje: int = 0        # Tiles every hit target is pushed (away from the caster)
@export var estado: String = ""    # State for the hit targets (Estados.gd: ardiendo, aturdido...)
@export var estado_dias: int = 2
@export var estado_potencia: int = 1
@export_range(0, 100, 5, "suffix:%") var estado_prob: int = 100
@export var estado_propio: String = "" # State for the caster (e.g. desenganchado)
@export var estado_propio_dias: int = 1
@export var peligro: String = ""   # Leaves this on the floor of the area (Tactica.PELIGROS: fuego)
@export var peligro_dias: int = 2
## Passives: they only trigger if the target has this state ("" = always)
@export var condicion_estado: String = ""
## Rules it changes while you have it: escurridizo (no attacks of opportunity), guardaespaldas,
## estandarte (+1 to the rolls of allies within 2 tiles), inamovible, inmune_<estado>
@export var etiquetas: PackedStringArray = []

@export_group("Pasiva")
@export var disparador: Disparador = Disparador.AL_GOLPEAR
@export var reaccion: Reaccion = Reaccion.DANO_EXTRA
@export_range(0, 100, 5, "suffix:%") var probabilidad: int = 100 # Per stack (cap 100)
@export var valor: int = 1         # Per stack
@export_enum("fuerza", "defensa", "magia", "suerte", "velocidad", "radio_luz") var stat: String = "fuerza"
@export var duracion: int = 1      # Days (STAT_TEMPORAL)
@export var max_pilas: int = 5

# The id, or the file name if it was left empty
func obtener_id() -> String:
	return id if id != "" else resource_path.get_file().get_basename()

func es_activa() -> bool:
	return tipo == Tipo.ACTIVA

# Class tag shown on the card ("GLOBAL", "MAGO"...)
func etiqueta_clase() -> String:
	return "GLOBAL" if clase == "" else {"Caballero": "CABALLERO", "Ladron": "LADRÓN", "Mago": "MAGO", "Tanque": "TANQUE"}.get(clase, clase.to_upper())

# Short line with the numbers (for the draft and the bar)
func resumen() -> String:
	if es_activa():
		var partes := []
		partes.append("%d PA" % coste_pa if coste_pa > 0 else "Sin PA")
		if usos > 0:
			partes.append("%d uso%s" % [usos, "" if usos == 1 else "s"])
		if enfriamiento > 0:
			partes.append("recarga %d día%s" % [enfriamiento, "" if enfriamiento == 1 else "s"])
		if objetivo != Objetivo.UNO_MISMO:
			partes.append("alcance %d" % alcance_max if alcance_min <= 1 else "alcance %d-%d" % [alcance_min, alcance_max])
		return " · ".join(partes)
	return "Pasiva · se apila (máx. %d)" % max_pilas
