extends Resource
class_name EventoData

@export_category("Historia Principal")
@export var titulo: String = "Evento Misterioso"
@export_multiline var descripcion: String = "Un anciano encapuchado te corta el paso..."
@export var ilustracion: Texture2D

@export_category("Arte (Assets/Eventos/<evento>/)")
## How it looks on the map: 16x16, or a 16 px tall strip with several frames (it gets animated)
@export var sprite_mapa: Texture2D
## Image on win / lose (if the option doesn't have its own)
@export var imagen_exito: Texture2D
@export var imagen_fracaso: Texture2D

@export_category("Caminos Posibles")
@export var opciones: Array[OpcionEvento] = []
enum TipoTrigger { MANUAL, PROXIMIDAD, CONDICIONAL }

@export_category("Comportamiento del Evento")
@export var gatillo: TipoTrigger = TipoTrigger.MANUAL

@export_group("Si es de PROXIMIDAD")
@export var radio_activacion: int = 3 # How many tiles away it triggers

@export_group("Si es CONDICIONAL")
# The stat it will look at (you can add more if you want)
@export_enum("ninguno", "hp_actual", "hambre_actual", "sanidad_actual") var stat_objetivo: String = "ninguno"
# The value BELOW which the event triggers (e.g. if sanity < 30)
@export var valor_critico: int = 30
