extends SceneTree
# Headless Jenova build:
#   godot --headless --editor --path . -s res://Tools/build_jenova.gd [-- rebuild]
# Waits for the Jenova plugin to be ready, compiles and leaves a few frames so Jenova finishes
# dumping the module into its database before quitting.
var frames := 0
var lanzado := false
var done_at := -1
var ok := false

func _process(_delta):
	frames += 1
	if done_at >= 0:
		if frames - done_at > 120:
			quit(0 if ok else 1)
		return false
	# Jenova processes frames while compiling: without this flag we'd come in here again
	if lanzado or frames < 30:
		return false
	var inst = ClassDB.class_call_static("JenovaEditorPlugin", "GetInstance")
	if inst == null:
		if frames > 600:
			print("[build] ERROR: Jenova plugin never initialized")
			quit(2)
		return false
	lanzado = true
	var rebuild := "rebuild" in OS.get_cmdline_user_args()
	print("[build] ", "Rebuilding" if rebuild else "Building", " Jenova project...")
	ok = inst.call("RebuildProject" if rebuild else "BuildProject")
	print("[build] Result: ", ok)
	done_at = frames
	return false
