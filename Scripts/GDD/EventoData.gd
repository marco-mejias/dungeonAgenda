extends Resource
class_name EventoData

@export_category("Historia Principal")
@export var titulo: String = "Evento Misterioso"
@export_multiline var descripcion: String = "Un anciano encapuchado te corta el paso..."
@export var ilustracion: Texture2D

@export_category("Caminos Posibles")
@export var opciones: Array[OpcionEvento] = []
enum TipoTrigger { MANUAL, PROXIMIDAD, CONDICIONAL }

@export_category("Comportamiento del Evento")
@export var gatillo: TipoTrigger = TipoTrigger.MANUAL

@export_group("Si es de PROXIMIDAD")
@export var radio_activacion: int = 3 # A cuántas casillas de distancia salta

@export_group("Si es CONDICIONAL")
# La estadística que va a mirar (puedes añadir más si quieres)
@export_enum("ninguno", "hp_actual", "hambre_actual", "sanidad_actual") var stat_objetivo: String = "ninguno"
# El valor por DEBAJO del cual el evento saltará (Ej: Si sanidad < 30)
@export var valor_critico: int = 30
