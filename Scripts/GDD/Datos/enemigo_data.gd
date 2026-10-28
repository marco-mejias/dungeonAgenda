@tool
class_name EnemigoData
extends Resource

# Ficha de un tipo de enemigo. Se crean en Data/Enemigos y el Catálogo elige cuáles salen
# en cada piso según 'piso_min', 'piso_max' y 'peso_aparicion'.

## TANQUE: va a por el héroe más cercano y se planta delante.
## CUERPO_A_CUERPO: va a por el héroe más débil que pueda alcanzar.
## LANZADOR: busca una casilla desde la que atacar a distancia, lejos de los héroes.
## COBARDE: ataca mientras está sano y huye cuando le queda poca vida.
enum Perfil { TANQUE, CUERPO_A_CUERPO, LANZADOR, COBARDE }

const HOJA := preload("res://Assets/2D Pixel Dungeon Asset Pack/character and tileset/Dungeon_Character_2.png")

@export_group("Identidad")
## Identificador único. Vacío = nombre del archivo.
@export var id: String = ""
@export var nombre: String = "Enemigo"
## Fotograma en la hoja Dungeon_Character_2 (7 columnas x 2 filas: 0-13)
@export_range(0, 13) var frame: int = 11:
	set(v):
		frame = v
		emit_changed()
@export var tinte: Color = Color(1, 0.85, 0.85)

@export_group("Stats (piso 1)")
@export var vida: int = 14
@export var fuerza: int = 10
@export var defensa: int = 2
@export var magia: int = 0
@export var suerte: int = 2
## Casillas que puede andar en su turno
@export var movimiento: int = 3

@export_group("Subida por piso")
@export var vida_por_piso: int = 3
@export var fuerza_por_piso: int = 1
@export var defensa_por_piso: int = 0

@export_group("Combate")
## Su arma (sin arma pega con Fuerza a 1 casilla). Tiene que estar en Data/Objetos.
@export var arma: ArmaData
## Botín al morir (vacío = tabla "enemigo")
@export var botin: TablaBotin

@export_group("IA")
@export var perfil: Perfil = Perfil.CUERPO_A_CUERPO
## Casillas a las que ve a los héroes (con línea de visión) y se despierta
@export var radio_deteccion: int = 6
## Huye con menos de este % de vida (0 = nunca huye)
@export_range(0, 100, 5, "suffix:%") var huir_con_vida: int = 0

@export_group("Aparición")
@export var piso_min: int = 1
## 0 = sin límite
@export var piso_max: int = 0
## Cuanto más alto, más a menudo sale frente al resto de enemigos válidos del piso
@export var peso_aparicion: int = 10

func obtener_id() -> String:
	return id if id != "" else resource_path.get_file().get_basename()

func vale_en_piso(piso: int) -> bool:
	return piso >= piso_min and (piso_max == 0 or piso <= piso_max)

func id_arma() -> String:
	return arma.obtener_id() if arma else ""

func id_botin() -> String:
	return botin.obtener_id() if botin else "enemigo"

# Icono para el Inspector y el panel de base de datos
func icono() -> Texture2D:
	var at := AtlasTexture.new()
	at.atlas = HOJA
	at.region = Rect2((frame % 7) * 16, (frame / 7) * 16, 16, 16)
	return at
