extends Resource
class_name EventoData

@export_category("Historia Principal")
@export var titulo: String = "Evento Misterioso"
@export_multiline var descripcion: String = "Un anciano encapuchado te corta el paso..."
@export var ilustracion: Texture2D

@export_category("Caminos Posibles")
@export var opciones: Array[OpcionEvento] = []
