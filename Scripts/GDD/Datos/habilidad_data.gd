@tool
class_name HabilidadData
extends Resource

# Una habilidad que se aprende al subir de nivel (draft 1 de 3, Scripts/GDD/progreso.gd).
#   ACTIVA: se lanza desde la barra de habilidades. Tiene usos (se recuperan en la fogata, con el
#           elixir o al subir de nivel), enfriamiento en días y coste de PA.
#   PASIVA: reacciona sola a un disparador (al golpear, al matar...). Si se vuelve a elegir en el
#           draft se apila (más probabilidad / más fuerte) sin gastar otra ranura: así salen combos.
# clase: "" = global (cualquiera puede sacarla) o la clase que la tiene en su reserva.

enum Tipo { ACTIVA, PASIVA }
enum Objetivo { UNO_MISMO, ENEMIGO, ALIADO, CASILLA }
enum Disparador { AL_GOLPEAR, AL_CRITICO, AL_FALLAR, AL_MATAR, AL_RECIBIR_GOLPE, AL_EMPEZAR_DIA, AL_USAR_HABILIDAD }
enum Reaccion { DANO_EXTRA, REBOTE, CURAR, RECUPERAR_USO, BAJAR_ENFRIAMIENTO, PA_EXTRA, STAT_TEMPORAL, ORO, NO_GASTAR_USO, CONTRAGOLPE, APLICAR_ESTADO, NINGUNA }

@export_group("Identidad")
@export var id: String = ""
@export var nombre: String = "Habilidad"
@export_multiline var descripcion: String = ""
@export var icono: Texture2D
@export var clase: String = "" # "" (global), "Caballero", "Ladron", "Mago" o "Tanque"
@export var tipo: Tipo = Tipo.ACTIVA
@export var peso_draft: int = 10 # cuánto sale en el draft

@export_group("Activa")
@export var coste_pa: int = 2
@export var usos: int = 2
@export var enfriamiento: int = 1 # días que tarda en volver a estar lista
@export var objetivo: Objetivo = Objetivo.ENEMIGO
@export var alcance_min: int = 1
@export var alcance_max: int = 1
@export var requiere_vision: bool = true
@export var area: int = 0          # radio alrededor del objetivo (0 = solo él)
@export var saltos: int = 0        # rayo en cadena: a cuántos enemigos más salta
@export var tirada: bool = true    # tira d20 contra la CA (si no, acierta siempre)
@export var dano_base: int = 0
@export_enum("fuerza", "magia", "suerte", "defensa") var stat_dano: String = "fuerza"
@export var mult_stat: float = 1.0
@export var efectos_objetivo: Array[Efecto] = [] # a los héroes alcanzados (curas, buffs)
@export var efectos_propios: Array[Efecto] = []  # al que la lanza
@export var especial: String = ""  # comportamiento a medida: gallina, provocar...

@export_group("Posicionamiento y estados")
## Movimiento del que la lanza antes del efecto: "" | carga (en línea hasta el objetivo, +1 de daño por
## casilla) | teletransporte (a la casilla elegida) | intercambio (con el aliado elegido)
## | atraer (trae al objetivo hasta pegarlo a ti)
@export var movimiento: String = ""
@export var empuje: int = 0        # casillas que empuja a cada objetivo alcanzado (lejos del que la lanza)
@export var estado: String = ""    # estado para los objetivos alcanzados (Estados.gd: ardiendo, aturdido...)
@export var estado_dias: int = 2
@export var estado_potencia: int = 1
@export_range(0, 100, 5, "suffix:%") var estado_prob: int = 100
@export var estado_propio: String = "" # estado para el que la lanza (p. ej. desenganchado)
@export var estado_propio_dias: int = 1
@export var peligro: String = ""   # deja esto en el suelo del área (Tactica.PELIGROS: fuego)
@export var peligro_dias: int = 2
## Pasivas: solo saltan si el objetivo tiene este estado ("" = siempre)
@export var condicion_estado: String = ""
## Reglas que cambia mientras se tiene: escurridizo (sin ataques de oportunidad), guardaespaldas,
## estandarte (+1 a las tiradas de aliados a 2 casillas), inamovible, inmune_<estado>
@export var etiquetas: PackedStringArray = []

@export_group("Pasiva")
@export var disparador: Disparador = Disparador.AL_GOLPEAR
@export var reaccion: Reaccion = Reaccion.DANO_EXTRA
@export_range(0, 100, 5, "suffix:%") var probabilidad: int = 100 # por pila (tope 100)
@export var valor: int = 1         # por pila
@export_enum("fuerza", "defensa", "magia", "suerte", "velocidad", "radio_luz") var stat: String = "fuerza"
@export var duracion: int = 1      # días (STAT_TEMPORAL)
@export var max_pilas: int = 5

func obtener_id() -> String:
	return id if id != "" else resource_path.get_file().get_basename()

func es_activa() -> bool:
	return tipo == Tipo.ACTIVA

func etiqueta_clase() -> String:
	return "GLOBAL" if clase == "" else {"Caballero": "CABALLERO", "Ladron": "LADRÓN", "Mago": "MAGO", "Tanque": "TANQUE"}.get(clase, clase.to_upper())

# Línea corta con los números (para el draft y la barra)
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
