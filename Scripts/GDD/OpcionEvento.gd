extends Resource
class_name OpcionEvento

@export var texto_boton: String = "Nueva Acción"
@export_enum("hp_actual", "hp_max", "velocidad", "fuerza", "defensa", "magia", "suerte", "sanidad_actual", "ninguna") var stat_usada: String = "fuerza"

@export_category("Dificultad y RNG")
## Facilidad: la dificultad del d20 es CD = 16 - 5 x multiplicador (1.0 -> CD 11, 2.0 -> CD 6)
@export_range(0.1, 3.0) var multiplicador_dificultad: float = 1.0 

@export_category("Arte de la resolución")
## Imagen al ganar / perder con ESTA opción (si no hay, la del evento)
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
# Se aplican al héroe además de la función de arriba (que sigue funcionando)
@export var efectos_exito: Array[Efecto] = []
@export var efectos_fracaso: Array[Efecto] = []
## Extra con un 20 natural (éxito crítico) / un 1 natural (pifia), además de los de arriba
@export var efectos_critico: Array[Efecto] = []
@export var efectos_pifia: Array[Efecto] = []
