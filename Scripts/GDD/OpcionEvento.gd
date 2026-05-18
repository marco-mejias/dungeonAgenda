extends Resource
class_name OpcionEvento

@export var texto_boton: String = "Nueva Acción"
@export_enum("hp_actual", "hp_max", "velocidad", "fuerza", "defensa", "magia", "ninguna") var stat_usada: String = "fuerza"

@export_category("Dificultad y RNG")
@export_range(0.1, 3.0) var multiplicador_dificultad: float = 1.0 

@export_category("Resolución Narrativa")
@export_multiline var texto_exito: String = "¡Lo conseguiste con gran habilidad!"
@export_multiline var texto_fracaso: String = "Has fallado estrepitosamente..."

@export_category("Consecuencias Mecánicas (Motor)")
@export var funcion_exito: String = ""
@export var parametro_exito: int = 0
@export var funcion_fracaso: String = ""
@export var parametro_fracaso: int = 0
