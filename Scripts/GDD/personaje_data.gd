class_name PersonajeData
extends Resource

@export var nombre_display: String = "Héroe"
@export_file("*.png", "*.jpg", "*.webp", "*.tres") var retrato_path: String
@export var vida_max: int = 100
@export var velocidad: int = 3
@export var defensa: int = 5
@export var fuerza: int = 10
@export var magia: int = 5
@export var suerte: int = 5
@export var ruta_cuerpo: String = "res://prefabs/CuerpoBase.tscn"
