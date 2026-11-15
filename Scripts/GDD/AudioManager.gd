extends Node

# AudioManager (autoload): music, effects and interface sounds. The sounds (Assets/Audio) are NOT in
# the repository: they are third-party packs, several with a licence that doesn't allow
# redistribution. That's why they're loaded at startup ONLY if the file exists: without the folder
# the game works the same, just silent.

# Three speakers: effects, interface and music
var sfx_player: AudioStreamPlayer
var ui_player: AudioStreamPlayer
var bgm_player: AudioStreamPlayer

# Dictionary with every sound loaded in memory and its settings
var sonidos = {
	
	# --- MUSIC ---
	"dungeon": {
		"ruta": "res://Assets/Audio/kaden_cook-epic-dungeon-beat-259883.mp3",
		"volumen": -25.0, # Low, to set the mood without getting in the way
		"pitch": 1.3
	},
	"fireplace": {
		"ruta": "res://Assets/Audio/poopie pack/regrowth wip.wav",
		"volumen": -12.0,
		"pitch": 1.0
	},
	
	# --- UI ---
	"hover": {
		"ruta": "res://Assets/Audio/10_UI_Menu_SFX/001_Hover_01.wav",
		"volumen": -15.0, # Very subtle so the player doesn't go crazy
		"pitch": 1.0
	},
	"clic": {
		"ruta": "res://Assets/Audio/10_UI_Menu_SFX/013_Confirm_03.wav",
		"volumen": -30.0,
		"pitch": 1.0
	},
	"no": {
		"ruta": "res://Assets/Audio/10_UI_Menu_SFX/033_Denied_03.wav",
		"volumen": -20.0,
		"pitch": 0.9 # Pitch lowered 10% so it sounds deeper (error)
	},
	"open": {
		"ruta": "res://Assets/Audio/UI Soundpack/WAV/Coffee1.wav",
		"volumen": -20.0,
		"pitch": 1.0
	},
	
	# --- EXPLORE ---
	"walk": {
		"ruta": "res://Assets/Audio/12_Player_Movement_SFX/12_Step_wood_03.wav",
		"volumen": -8.0, 
		"pitch": 1.0
	},
	"stairs": {
		"ruta": "res://Assets/Audio/12_Player_Movement_SFX/42_Cling_climb_03.wav",
		"volumen": -2.0,
		"pitch": 1.0
	},
	
	# --- EVENTS ---
	"fail": {
		"ruta": "res://Assets/Audio/Announcer Pack/failure.wav",
		"volumen": -20.0,
		"pitch": 1.0
	},
	"event": {
		"ruta": "res://Assets/Audio/47313572-experimental-8-bit-sound-270302.mp3",
		"volumen": -20.0,
		"pitch": 1.0
	},
	"dice": {
		"ruta": "res://Assets/Audio/558119__abdrtar__dice.mp3",
		"volumen": 0.0, # Full volume, so the bounce is heard well
		"pitch": 1.0
	},
	"success": {
		"ruta": "res://Assets/Audio/Announcer Pack/amazing.wav",
		"volumen": -20.0,
		"pitch": 1.0
	}
	
	# MECHANICS (keep adding here)
}

func _ready():
	# This line tells the AudioManager and all its speakers: "never pause!"
	process_mode = Node.PROCESS_MODE_ALWAYS
	# Loads the ones on disk; the missing ones stay silent
	for nombre in sonidos.keys():
		var ruta: String = sonidos[nombre]["ruta"]
		sonidos[nombre]["stream"] = load(ruta) if ResourceLoader.exists(ruta) else null
	
	sfx_player = AudioStreamPlayer.new()
	add_child(sfx_player)
	
	ui_player = AudioStreamPlayer.new()
	add_child(ui_player)
	
	bgm_player = AudioStreamPlayer.new()
	add_child(bgm_player)
	
	# We connect to the music bus
	bgm_player.bus = "Musica"
	sfx_player.bus = "SFX"
	ui_player.bus = "SFX" # The UI counts as a sound effect too
	
	# Start the dungeon music by default
	play_music("dungeon")

# Plays a sound effect by name (silent if it isn't loaded)
func play_sfx(nombre: String):
	if sonidos.has(nombre) and sonidos[nombre].get("stream") != null:
		var config = sonidos[nombre]
		sfx_player.stream = config["stream"]
		sfx_player.volume_db = config["volumen"]
		
		# Random pitch variation for the footsteps (breaks the monotony)
		if nombre == "walk":
			sfx_player.pitch_scale = config["pitch"] + randf_range(-0.15, 0.15)
		else:
			sfx_player.pitch_scale = config["pitch"]
			
		sfx_player.play()

# Plays an interface sound
func play_ui(nombre: String):
	if sonidos.has(nombre) and sonidos[nombre].get("stream") != null:
		var config = sonidos[nombre]
		ui_player.stream = config["stream"]
		ui_player.volume_db = config["volumen"]
		ui_player.pitch_scale = config["pitch"]
		ui_player.play()

# Changes the background music
func play_music(nombre: String):
	if sonidos.has(nombre) and sonidos[nombre].get("stream") != null:
		var config = sonidos[nombre]
		bgm_player.stream = config["stream"]
		bgm_player.volume_db = config["volumen"]
		bgm_player.pitch_scale = config["pitch"]
		bgm_player.play()
