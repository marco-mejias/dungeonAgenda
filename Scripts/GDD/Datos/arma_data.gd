@tool
@icon("res://Assets/Iconos/Objetos/espada_corta.png")
class_name ArmaData
extends EquipoData

# Weapon: goes in the main hand. Reach 1 = melee (the enemy counterattacks); reach > 1 = ranged (no
# counterattack if you're not adjacent).

enum Animacion { TAJO, ESTOCADA, GOLPE, DISPARO, CONJURO }

@export_group("Ataque")
@export var dano: int = 3
@export_enum("fuerza", "magia", "suerte") var stat_ataque: String = "fuerza"
@export var coste_pa: int = 2
@export_range(1, 10) var alcance_min: int = 1
@export_range(1, 10) var alcance_max: int = 1
@export var requiere_vision: bool = true
@export var dos_manos: bool = false

@export_group("Crítico")
@export_range(0, 100, 1, "suffix:%") var critico_extra: int = 0
@export_range(1.0, 5.0, 0.1) var mult_critico: float = 2.0

@export_group("Al golpear")
@export var efectos_al_golpear: Array[Efecto] = []
## Attack animation type (per-weapon animations will come later).
@export var animacion: Animacion = Animacion.TAJO

func _init() -> void:
	tipo = Tipo.ARMA
	ranura = Ranura.MANO_PRINCIPAL
