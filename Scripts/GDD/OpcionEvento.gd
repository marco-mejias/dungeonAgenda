extends Resource
class_name OpcionEvento

@export var texto_boton: String = "Nueva Acción"
@export_enum("hp_actual", "hp_max", "velocidad", "fuerza", "defensa", "magia", "suerte", "sanidad_actual", "ninguna") var stat_usada: String = "fuerza"

@export_category("Dificultad y RNG")
## Ease: the d20 difficulty is DC = 16 - 5 x multiplier (1.0 -> DC 11, 2.0 -> DC 6)
@export_range(0.1, 3.0) var multiplicador_dificultad: float = 1.0 

@export_category("Arte de la resolución")
## Image on win / lose with THIS option (if there's none, the event's one)
@export var imagen_exito: Texture2D
@export var imagen_fracaso: Texture2D

@export_category("Resolución Narrativa")
@export_multiline var texto_exito: String = "¡Lo conseguiste con gran habilidad!"
@export_multiline var texto_fracaso: String = "Has fallado estrepitosamente..."

@export_category("Consecuencias Mecánicas (Motor)")
@export var funcion_exito: String = ""
@export var parametro_exito: int = 0
@export var funcion_fracaso: String = ""
@export var parametro_fracaso: int = 0

@export_category("Efectos (nuevo sistema)")
# Applied to the hero on top of the function above (which still works)
@export var efectos_exito: Array[Efecto] = []
@export var efectos_fracaso: Array[Efecto] = []
## Extra on a natural 20 (critical success) / a natural 1 (fumble), on top of the ones above
@export var efectos_critico: Array[Efecto] = []
@export var efectos_pifia: Array[Efecto] = []
