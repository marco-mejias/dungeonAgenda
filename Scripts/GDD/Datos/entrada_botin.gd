@tool
class_name EntradaBotin
extends Resource

# Una línea de una tabla de botín: un objeto, oro o comida, con su peso de probabilidad.

enum Recurso { OBJETO, ORO, COMIDA }

@export var recurso: Recurso = Recurso.OBJETO
@export var objeto: ObjetoData
@export var cantidad_min: int = 1
@export var cantidad_max: int = 1
## Cuanto más alto, más probable frente al resto de entradas de la tabla.
@export var peso: int = 10
@export var piso_min: int = 1
## 0 = sin límite
@export var piso_max: int = 0

func vale_en_piso(piso: int) -> bool:
	return piso >= piso_min and (piso_max == 0 or piso <= piso_max)
