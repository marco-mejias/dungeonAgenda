@tool
class_name ArmaData
extends EquipoData

# Arma: va en la mano principal. Alcance 1 = cuerpo a cuerpo (el enemigo contraataca);
# alcance > 1 = a distancia (sin contraataque si no estás pegado).

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
## Tipo de animación de ataque (las animaciones por arma llegarán más adelante).
@export var animacion: Animacion = Animacion.TAJO

func _init() -> void:
	tipo = Tipo.ARMA
	ranura = Ranura.MANO_PRINCIPAL
