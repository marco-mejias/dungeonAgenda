@tool
class_name TablaBotin
extends Resource

# Tabla de botín (cofres, enemigos, eventos): se hacen 'tiradas' y en cada una sale
# una entrada al azar según su peso, filtrando por piso.

@export var id: String = ""
@export var tiradas: int = 1
## Probabilidad de que cada tirada dé algo (100 = siempre)
@export_range(0, 100, 1, "suffix:%") var probabilidad: int = 100
@export var entradas: Array[EntradaBotin] = []

func obtener_id() -> String:
	return id if id != "" else resource_path.get_file().get_basename()

# Devuelve una lista de {recurso, id, cantidad}
func tirar(piso: int) -> Array:
	var resultado: Array = []
	var validas: Array[EntradaBotin] = []
	var total := 0
	for e in entradas:
		if e and e.vale_en_piso(piso) and (e.recurso != EntradaBotin.Recurso.OBJETO or e.objeto):
			validas.append(e)
			total += maxi(0, e.peso)
	if total <= 0:
		return resultado
	for t in tiradas:
		if randi() % 100 >= probabilidad:
			continue
		var r := randi() % total
		for e in validas:
			r -= maxi(0, e.peso)
			if r < 0:
				resultado.append({
					"recurso": e.recurso,
					"id": e.objeto.obtener_id() if e.objeto else "",
					"cantidad": randi_range(e.cantidad_min, maxi(e.cantidad_min, e.cantidad_max)),
				})
				break
	return resultado
