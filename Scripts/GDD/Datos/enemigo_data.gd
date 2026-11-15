@tool
class_name EnemigoData
extends Resource

# Card of an enemy type. They're created in Data/Enemigos and the Catalogo picks which ones spawn on
# each floor from 'piso_min', 'piso_max' and 'peso_aparicion'.

## TANQUE: goes for the closest hero and stands in front of them. CUERPO_A_CUERPO: goes for the
## weakest hero it can reach. LANZADOR: looks for a tile to attack from range, far from the heroes.
## COBARDE: attacks while healthy and runs away when low on health.
enum Perfil { TANQUE, CUERPO_A_CUERPO, LANZADOR, COBARDE }

const HOJA := preload("res://Assets/2D Pixel Dungeon Asset Pack/character and tileset/Dungeon_Character_2.png")

@export_group("Identidad")
## Unique identifier. Empty = file name.
@export var id: String = ""
@export var nombre: String = "Enemigo"
## Frame in the Dungeon_Character_2 sheet (7 columns x 2 rows: 0-13)
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
## Tiles it can walk on its turn
@export var movimiento: int = 3

@export_group("Subida por piso")
@export var vida_por_piso: int = 3
@export var fuerza_por_piso: int = 1
@export var defensa_por_piso: int = 0

@export_group("Combate")
## Its weapon (without a weapon it hits with Strength at 1 tile). It has to be in Data/Objetos.
@export var arma: ArmaData
## Loot on death (empty = "enemigo" table)
@export var botin: TablaBotin

@export_group("IA")
@export var perfil: Perfil = Perfil.CUERPO_A_CUERPO
## Tiles at which it sees the heroes (with line of sight) and wakes up
@export var radio_deteccion: int = 6
## Runs away under this % of health (0 = never runs)
@export_range(0, 100, 5, "suffix:%") var huir_con_vida: int = 0

@export_group("Aparición")
@export var piso_min: int = 1
## 0 = no limit
@export var piso_max: int = 0
## The higher, the more often it spawns compared to the other valid enemies on the floor
@export var peso_aparicion: int = 10

# The id, or the file name if it was left empty
func obtener_id() -> String:
	return id if id != "" else resource_path.get_file().get_basename()

# Can it spawn on this floor? (piso_max 0 = no limit)
func vale_en_piso(piso: int) -> bool:
	return piso >= piso_min and (piso_max == 0 or piso <= piso_max)

func id_arma() -> String:
	return arma.obtener_id() if arma else ""

func id_botin() -> String:
	return botin.obtener_id() if botin else "enemigo"

# Icon for the Inspector and the database panel
func icono() -> Texture2D:
	var at := AtlasTexture.new()
	at.atlas = HOJA
	at.region = Rect2((frame % 7) * 16, (frame / 7) * 16, 16, 16)
	return at
