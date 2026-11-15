extends SceneTree
# Backpack sprites (pixel art drawn with character grids):
#   - Assets/Sprites/Puesto/<id>.png: how each item looks WORN on the hero. They're 16x16
#     layers aligned with the hero sprite (all four share a silhouette: head up, right hand on
#     column 13, left hand on column 2).
#   - Assets/UI/Iconos/<stat>.png: stats bar icons (health, PA, sanity...).
#   godot --headless --path . -s res://Tools/generar_sprites_inventario.gd            (draws)
#   godot --headless --path . -s res://Tools/generar_sprites_inventario.gd -- asignar (after
#     importing: puts every sprite in the 'sprite_puesto' field of its .tres)

const PALETA := {
	"#": Color(0.08, 0.06, 0.1), "w": Color(0.92, 0.93, 0.95), "s": Color(0.62, 0.66, 0.74),
	"d": Color(0.38, 0.40, 0.48), "g": Color(0.95, 0.75, 0.25), "o": Color(0.70, 0.50, 0.15),
	"b": Color(0.55, 0.35, 0.18), "n": Color(0.36, 0.22, 0.12), "u": Color(0.35, 0.65, 1.0),
	"c": Color(0.75, 0.9, 1.0), "p": Color(0.50, 0.30, 0.70), "q": Color(0.33, 0.18, 0.48),
	"l": Color(0.62, 0.42, 0.25), "m": Color(0.45, 0.29, 0.16), "r": Color(0.80, 0.22, 0.17),
	"y": Color(1.0, 0.86, 0.3), "e": Color(0.44, 0.60, 0.27), "k": Color(0.85, 0.84, 0.80),
}

# Row -> text. Missing rows stay transparent.
const PUESTOS := {
	"espada_corta": {1: ".............#..", 2: "............#w#.", 3: "............#w#.", 4: "............#w#.",
		5: "............#w#.", 6: "............#s#.", 7: "............#s#.", 8: "............#s#.",
		9: "...........#gsg#", 10: "...........#ggg#", 11: "............#n#.", 12: "............#n#.", 13: ".............#.."},
	"daga": {5: ".............#..", 6: "............#w#.", 7: "............#w#.", 8: "............#s#.",
		9: "...........#gsg#", 10: "...........#ggg#", 11: "............#n#.", 12: ".............#.."},
	"baston_arcano": {0: "............###.", 1: "...........#cuu#", 2: "...........#uuq#", 3: "............#q#.",
		4: "............#b#.", 5: "............#b#.", 6: "............#n#.", 7: "............#b#.", 8: "............#b#.",
		9: "............#n#.", 10: "............#b#.", 11: "............#b#.", 12: "............#n#.", 13: "............#b#.", 14: ".............#.."},
	"maza": {1: "...........####.", 2: "..........#dssd#", 3: "..........#swsd#", 4: "..........#dssd#", 5: "...........#dd#.",
		6: "............#n#.", 7: "............#b#.", 8: "............#n#.", 9: "............#b#.", 10: "............#n#.",
		11: "............#b#.", 12: "............#n#.", 13: ".............#.."},
	"arco_corto": {1: "............##..", 2: "............#b#.", 3: "............w#b#", 4: "............w.#b", 5: "............w.#b",
		6: "............w.#b", 7: "............w.#o", 8: "............w.#b", 9: "............w.#b", 10: "............w#b#",
		11: "............#b#.", 12: "............##.."},
	"escudo_heraldico": {7: "######..........", 8: "#ssss#..........", 9: "#rggr#..........", 10: "#rggr#..........",
		11: "#rrrr#..........", 12: ".#rr#...........", 13: "..##............"},
	"cota_malla": {8: "...#sssssss#....", 9: "...#swsswss#....", 10: "...#sssssss#....", 11: "...#ssswsss#....",
		12: "...#ooooooo#....", 13: "...#sssssss#...."},
	"tunica": {8: "...#qpppppq#....", 9: "...#ppppppp#....", 10: "...#ggggggg#....", 11: "...#ppppppp#....",
		12: "..#ppppppppp#...", 13: "..#ppppppppp#...", 14: "..#qqqqqqqqq#...", 15: "..###########..."},
	"capucha": {0: "....#######.....", 1: "...#lllllll#....", 2: "..#llmmmmmll#...", 3: "..#lm.....ml#...",
		4: "..#lm.....ml#...", 5: "..#lm.....ml#...", 6: "..#llm...mll#...", 7: "...#llmmmll#...."},
	"anillo_suerte": {9: "...y............", 10: "..u.............", 11: ".g.g............", 12: "..g............."},
	"antorcha": {2: ".y..............", 3: "yry.............", 4: "rrr.............", 5: "#n#.............", 6: ".n..............",
		7: ".n..............", 8: ".n..............", 9: ".#.............."},
	"mochila_pequena": {7: "##..............", 8: "#l#.............", 9: "#lm.............", 10: "#lm.............",
		11: "#l#.............", 12: "##.............."},
	"mochila_grande": {5: ".##.............", 6: "#kk#............", 7: "###.............", 8: "#bb#............", 9: "#bn#............",
		10: "#bn#............", 11: "#bn#............", 12: "#bb#............", 13: "###............."},
}

# 12x12 icons for the stats bar
const ICONOS := {
	"vida": ["............", "..##....##..", ".#rr#..#rr#.", "#rrrr##rrrr#", "#rrrrrrrrrr#", "#rrrrrrrrrr#",
		".#rrrrrrrr#.", "..#rrrrrr#..", "...#rrrr#...", "....#rr#....", ".....##.....", "............"],
	"pa": ["............", ".....##.....", "....#yy#....", "...#yyyy#...", "..#yywyyy#..", ".#yywyyyyy#.",
		".#yyyyyyyy#.", "..#yyyyyy#..", "...#yyyy#...", "....#yy#....", ".....##.....", "............"],
	"cordura": ["............", "............", "...######...", "..#kkkkkk#..", ".#kk#pp#kk#.", "#kkk#pp#kkk#",
		".#kk#pp#kk#.", "..#kkkkkk#..", "...######...", "............", "............", "............"],
	"hambre": ["............", "....####....", "...#llll#...", "..#lllmll#..", "..#llllll#..", "..#lllmll#..",
		"...#llll#...", "....##k#....", "......#k#...", ".....#kk#k#.", "......#kk#..", ".......##..."],
	"fuerza": ["............", ".....###....", "....#lll#...", "...#lllll#..", "...#ll#ll#..", "..#lll#ll#..",
		"..#llllll#..", "..#lllll#...", "..#llll#....", "..#lll#.....", "...###......", "............"],
	"defensa": ["............", ".##########.", ".#ssssssss#.", ".#swsssssd#.", ".#ssssssss#.", ".#ssssssss#.",
		"..#ssssss#..", "..#ssssss#..", "...#ssss#...", "....#ss#....", ".....##.....", "............"],
	"magia": ["............", ".....##.....", ".....#c#....", "....#cc#....", "#####cc#####", "#cccccccccc#",
		".#cccuuccc#.", "..#ccuucc#..", "..#cc##cc#..", ".#cc#..#cc#.", ".###....###.", "............"],
	"suerte": ["............", "...##..##...", "..#ee##ee#..", "..#eeeeee#..", "...#eeee#...", ".##e#ee#e##.",
		"#eeeeeeeeee#", ".#ee#ee#ee#.", "..##.##.##..", ".....#n.....", ".....#n.....", "............"],
	"radio_luz": ["....#..#....", "...#y##y#...", "...#yryy#...", "..#yrryy#...", "..#yrrry#...", "..#yrrry#...",
		"...#rrr#....", "....###.....", "....#n#.....", "....#n#.....", "....#n#.....", ".....#......"],
}

func dibujar(filas, ancho: int, alto: int, escala: int) -> Image:
	var im := Image.create(ancho * escala, alto * escala, false, Image.FORMAT_RGBA8)
	im.fill(Color(0, 0, 0, 0))
	for y in alto:
		var fila: String = filas.get(y, "") if filas is Dictionary else (filas[y] if y < filas.size() else "")
		for x in mini(ancho, fila.length()):
			if PALETA.has(fila[x]):
				for dx in escala:
					for dy in escala:
						im.set_pixel(x * escala + dx, y * escala + dy, PALETA[fila[x]])
	return im

func _init() -> void:
	var dir_puesto := "res://Assets/Sprites/Puesto"
	if "asignar" in OS.get_cmdline_user_args():
		for id in PUESTOS:
			for carpeta in ["Armas", "Equipo", "Mochilas"]:
				var ruta := "res://Data/Objetos/%s/%s.tres" % [carpeta, id]
				if not FileAccess.file_exists(ruta):
					continue
				var r: Resource = load(ruta)
				r.set("sprite_puesto", load(dir_puesto.path_join(id + ".png")))
				ResourceSaver.save(r, ruta)
				print("asignado ", id)
		quit()
		return
	DirAccess.make_dir_recursive_absolute(ProjectSettings.globalize_path(dir_puesto))
	DirAccess.make_dir_recursive_absolute(ProjectSettings.globalize_path("res://Assets/UI/Iconos"))
	for id in PUESTOS:
		dibujar(PUESTOS[id], 16, 16, 1).save_png(ProjectSettings.globalize_path(dir_puesto.path_join(id + ".png")))
		print("puesto ", id)
	for nombre in ICONOS:
		dibujar(ICONOS[nombre], 12, 12, 2).save_png(ProjectSettings.globalize_path("res://Assets/UI/Iconos/%s.png" % nombre))
		print("icono ", nombre)
	quit()
