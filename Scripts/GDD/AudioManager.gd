extends Node

# Creamos dos altavoces: uno para efectos y otro para música
var sfx_player: AudioStreamPlayer
var ui_player: AudioStreamPlayer
var bgm_player: AudioStreamPlayer

# Diccionario con todos los sonidos cargados en memoria y su configuración
var sonidos = {
	
	# --- MUSIC ---
	"dungeon": {
		"stream": preload("res://Assets/Audio/kaden_cook-epic-dungeon-beat-259883.mp3"),
		"volumen": -25.0, # Bajita, para dar ambiente sin estorbar
		"pitch": 1.3
	},
	"fireplace": {
		"stream": preload("res://Assets/Audio/poopie pack/regrowth wip.wav"),
		"volumen": -12.0,
		"pitch": 1.0
	},
	
	# --- UI ---
	"hover": {
		"stream": preload("res://Assets/Audio/10_UI_Menu_SFX/001_Hover_01.wav"),
		"volumen": -15.0, # Muy sutil para no volver loco al jugador
		"pitch": 1.0
	},
	"clic": {
		"stream": preload("res://Assets/Audio/10_UI_Menu_SFX/013_Confirm_03.wav"),
		"volumen": -30.0,
		"pitch": 1.0
	},
	"no": {
		"stream": preload("res://Assets/Audio/10_UI_Menu_SFX/033_Denied_03.wav"),
		"volumen": -20.0,
		"pitch": 0.9 # Le bajo el tono un 10% para que suene más grave (error)
	},
	"open": {
		"stream": preload("res://Assets/Audio/UI Soundpack/WAV/Coffee1.wav"),
		"volumen": -20.0,
		"pitch": 1.0
	},
	
	# --- EXPLORE ---
	"walk": {
		"stream": preload("res://Assets/Audio/12_Player_Movement_SFX/12_Step_wood_03.wav"),
		"volumen": -8.0, 
		"pitch": 1.0
	},
	"stairs": {
		"stream": preload("res://Assets/Audio/12_Player_Movement_SFX/42_Cling_climb_03.wav"),
		"volumen": -2.0,
		"pitch": 1.0
	},
	
	# --- EVENTS ---
	"fail": {
		"stream": preload("res://Assets/Audio/Announcer Pack/failure.wav"),
		"volumen": -20.0,
		"pitch": 1.0
	},
	"event": {
		"stream": preload("res://Assets/Audio/47313572-experimental-8-bit-sound-270302.mp3"),
		"volumen": -20.0,
		"pitch": 1.0
	},
	"dice": {
		"stream": preload("res://Assets/Audio/558119__abdrtar__dice.mp3"),
		"volumen": 0.0, # A volumen real, que se escuche bien el bote
		"pitch": 1.0
	},
	"success": {
		"stream": preload("res://Assets/Audio/Announcer Pack/amazing.wav"),
		"volumen": -20.0,
		"pitch": 1.0
	}
	
	# MECÁNICS (Puedes seguir añadiendo aquí)
}

func _ready():
	# Esta línea le dice al AudioManager y a todos sus altavoces: "¡Vosotros nunca os pauséis!"
	process_mode = Node.PROCESS_MODE_ALWAYS
	
	sfx_player = AudioStreamPlayer.new()
	add_child(sfx_player)
	
	ui_player = AudioStreamPlayer.new()
	add_child(ui_player)
	
	bgm_player = AudioStreamPlayer.new()
	add_child(bgm_player)
	
	# Conectamos a bus de musica
	bgm_player.bus = "Musica"
	sfx_player.bus = "SFX"
	ui_player.bus = "SFX" # La UI también se considera un efecto de sonido
	
	# Arrancar la música de la mazmorra por defecto
	play_music("dungeon")

func play_sfx(nombre: String):
	if sonidos.has(nombre):
		var config = sonidos[nombre]
		sfx_player.stream = config["stream"]
		sfx_player.volume_db = config["volumen"]
		
		# Variación de pitch aleatoria para los pasos (rompe la monotonía)
		if nombre == "walk":
			sfx_player.pitch_scale = config["pitch"] + randf_range(-0.15, 0.15)
		else:
			sfx_player.pitch_scale = config["pitch"]
			
		sfx_player.play()

func play_ui(nombre: String):
	if sonidos.has(nombre):
		var config = sonidos[nombre]
		ui_player.stream = config["stream"]
		ui_player.volume_db = config["volumen"]
		ui_player.pitch_scale = config["pitch"]
		ui_player.play()

func play_music(nombre: String):
	if sonidos.has(nombre):
		var config = sonidos[nombre]
		bgm_player.stream = config["stream"]
		bgm_player.volume_db = config["volumen"]
		bgm_player.pitch_scale = config["pitch"]
		bgm_player.play()
