@tool
extends VBoxContainer

# Panel "Base de datos" del editor: todos los .tres de Data/Objetos, Data/Enemigos, Data/Events y
# Data/Botin en una tabla, con filtro por tipo y búsqueda. Clic = abrirlo en el Inspector;
# doble clic = mostrarlo en el Sistema de archivos. Crear (con valores por defecto y en
# su carpeta), duplicar y borrar (a la papelera). A la derecha, los avisos de
# BaseDatos.validar (los mismos que da el juego al arrancar).

const FILTROS := ["Todo", "Armas", "Equipo", "Mochilas", "Consumibles y otros", "Enemigos", "Tablas de botín", "Eventos"]
const NUEVOS := [
	["Arma", "res://Scripts/GDD/Datos/arma_data.gd", "res://Data/Objetos/Armas"],
	["Equipo (armadura, accesorio, luz)", "res://Scripts/GDD/Datos/equipo_data.gd", "res://Data/Objetos/Equipo"],
	["Mochila", "res://Scripts/GDD/Datos/mochila_data.gd", "res://Data/Objetos/Mochilas"],
	["Consumible", "res://Scripts/GDD/Datos/objeto_data.gd", "res://Data/Objetos/Consumibles"],
	["Objeto (llave, material...)", "res://Scripts/GDD/Datos/objeto_data.gd", "res://Data/Objetos/Varios"],
	["Enemigo", "res://Scripts/GDD/Datos/enemigo_data.gd", "res://Data/Enemigos"],
	["Tabla de botín", "res://Scripts/GDD/Datos/tabla_botin.gd", "res://Data/Botin"],
	["Evento", "res://Scripts/GDD/EventoData.gd", "res://Data/Events"],
]
const GATILLOS := ["manual", "proximidad", "condicional (trampa)"]
const CARPETA_EVENTOS := "res://Data/Events"
const PERFILES := ["Tanque", "Cuerpo a cuerpo", "Lanzador", "Cobarde"]
const RANURAS := ["mano principal", "mano secundaria", "cabeza", "cuerpo", "accesorio", "luz", "mochila"]

var opt_filtro: OptionButton
var buscar: LineEdit
var menu_nuevo: MenuButton
var arbol: Tree
var avisos: RichTextLabel
var lbl_cuenta: Label
var dialogo_id: ConfirmationDialog
var campo_id: LineEdit
var dialogo_borrar: ConfirmationDialog

var datos := {}
var nuevo_pendiente := -1

func _ready() -> void:
	custom_minimum_size = Vector2(0, 260)
	var barra := HBoxContainer.new()
	add_child(barra)

	opt_filtro = OptionButton.new()
	for f in FILTROS:
		opt_filtro.add_item(f)
	opt_filtro.item_selected.connect(func(_i): _pintar())
	barra.add_child(opt_filtro)

	buscar = LineEdit.new()
	buscar.placeholder_text = "Buscar por nombre o id..."
	buscar.custom_minimum_size = Vector2(220, 0)
	buscar.text_changed.connect(func(_t): _pintar())
	barra.add_child(buscar)

	menu_nuevo = MenuButton.new()
	menu_nuevo.text = "Nuevo..."
	menu_nuevo.flat = false
	for i in NUEVOS.size():
		menu_nuevo.get_popup().add_item(NUEVOS[i][0], i)
	menu_nuevo.get_popup().id_pressed.connect(_pedir_id)
	barra.add_child(menu_nuevo)

	barra.add_child(_boton("Duplicar", _duplicar))
	barra.add_child(_boton("Borrar", func():
		if _seleccionado() != "":
			dialogo_borrar.dialog_text = "¿Mandar a la papelera %s?" % _seleccionado()
			dialogo_borrar.popup_centered()))
	barra.add_child(_boton("Recargar", recargar))
	lbl_cuenta = Label.new()
	lbl_cuenta.modulate = Color(0.7, 0.75, 0.85)
	barra.add_child(lbl_cuenta)

	var split := HSplitContainer.new()
	split.size_flags_vertical = Control.SIZE_EXPAND_FILL
	split.split_offset = 900
	add_child(split)

	arbol = Tree.new()
	arbol.columns = 6
	arbol.column_titles_visible = true
	var columnas := [["Nombre", 220], ["Id", 150], ["Tipo", 110], ["Datos", 380], ["Peso / aparición", 110], ["Pisos / valor", 90]]
	for i in columnas.size():
		arbol.set_column_title(i, columnas[i][0])
		arbol.set_column_custom_minimum_width(i, columnas[i][1])
		arbol.set_column_expand(i, i == 3)
	arbol.hide_root = true
	arbol.size_flags_horizontal = Control.SIZE_EXPAND_FILL
	arbol.item_selected.connect(_al_seleccionar)
	arbol.item_activated.connect(_al_activar)
	split.add_child(arbol)

	avisos = RichTextLabel.new()
	avisos.bbcode_enabled = true
	avisos.custom_minimum_size = Vector2(260, 0)
	split.add_child(avisos)

	dialogo_id = ConfirmationDialog.new()
	dialogo_id.title = "Nuevo"
	campo_id = LineEdit.new()
	campo_id.placeholder_text = "id_del_objeto (será el nombre del archivo)"
	dialogo_id.add_child(campo_id)
	dialogo_id.register_text_enter(campo_id)
	dialogo_id.confirmed.connect(func(): crear(nuevo_pendiente, campo_id.text))
	add_child(dialogo_id)

	dialogo_borrar = ConfirmationDialog.new()
	dialogo_borrar.title = "Borrar"
	dialogo_borrar.confirmed.connect(_borrar)
	add_child(dialogo_borrar)

	recargar()

func _boton(texto: String, accion: Callable) -> Button:
	var b := Button.new()
	b.text = texto
	b.pressed.connect(accion)
	return b

# ==========================================
# DATOS
# ==========================================

func recargar() -> void:
	datos = BaseDatos.cargar_todo(true)
	_pintar()

func _filas() -> Array:
	var filas: Array = []
	for id in datos.get("objetos", {}):
		filas.append(datos["objetos"][id])
	for id in datos.get("enemigos", {}):
		filas.append(datos["enemigos"][id])
	for id in datos.get("tablas", {}):
		filas.append(datos["tablas"][id])
	for e in BaseDatos.cargar_carpeta(CARPETA_EVENTOS, true):
		if e is EventoData:
			filas.append(e)
	return filas

func _pasa_filtro(r: Resource) -> bool:
	match opt_filtro.selected:
		1: return r is ArmaData
		2: return r is EquipoData and not r is ArmaData and not r is MochilaData
		3: return r is MochilaData
		4: return r is ObjetoData and not r is EquipoData
		5: return r is EnemigoData
		6: return r is TablaBotin
		7: return r is EventoData
	return true

func _pintar() -> void:
	arbol.clear()
	var raiz := arbol.create_item()
	var texto := buscar.text.strip_edges().to_lower()
	var n := 0
	for r in _filas():
		if not _pasa_filtro(r):
			continue
		var fila := describir(r)
		if texto != "" and not (fila[0].to_lower().contains(texto) or fila[1].to_lower().contains(texto)):
			continue
		var item := arbol.create_item(raiz)
		for c in 6:
			item.set_text(c, str(fila[c]))
		item.set_icon(0, fila[6])
		item.set_icon_max_width(0, 18)
		item.set_metadata(0, r.resource_path)
		item.set_tooltip_text(0, r.resource_path)
		n += 1
	lbl_cuenta.text = "  %d de %d" % [n, _filas().size()]

	var lista: PackedStringArray = datos.get("avisos", PackedStringArray())
	if lista.is_empty():
		avisos.text = "[color=#7f7]Sin avisos[/color]"
	else:
		avisos.text = "[color=#fc6][b]Avisos (%d)[/b][/color]\n• " % lista.size() + "\n• ".join(lista)

# [nombre, id, tipo, datos, peso, pisos/valor, icono]
func describir(r: Resource) -> Array:
	if r is EventoData:
		var stats: PackedStringArray = []
		var sin_arte := 0
		for o in r.opciones:
			if o:
				stats.append(o.stat_usada)
				if o.imagen_exito == null or o.imagen_fracaso == null:
					sin_arte += 1
		var arte := "" if r.sprite_mapa and r.ilustracion and sin_arte == 0 else " · falta arte"
		return [r.titulo, r.resource_path.get_file().get_basename(), "Evento", "%s · %d opciones (%s)%s" % [GATILLOS[r.gatillo], r.opciones.size(), ", ".join(stats), arte], "", "", r.sprite_mapa]
	if r is EnemigoData:
		var pisos: String = "%d+" % r.piso_min if r.piso_max == 0 else "%d-%d" % [r.piso_min, r.piso_max]
		var arma: String = r.arma.nombre if r.arma else "sin arma"
		return [r.nombre, r.obtener_id(), "Enemigo", "PV %d · F %d · D %d · mov %d · %s · %s" % [r.vida, r.fuerza, r.defensa, r.movimiento, PERFILES[r.perfil], arma], "aparición %d" % r.peso_aparicion, pisos, r.icono()]
	if r is TablaBotin:
		return [r.obtener_id(), r.obtener_id(), "Botín", "%d entradas · %d tirada(s) · %d%%" % [r.entradas.size(), r.tiradas, r.probabilidad], "", "", null]
	var datos_txt := ""
	var tipo := "Objeto"
	if r is ArmaData:
		tipo = "Arma"
		datos_txt = "daño %d (%s) · %d PA · alcance %d-%d%s%s" % [r.dano, r.stat_ataque, r.coste_pa, r.alcance_min, r.alcance_max, " · 2 manos" if r.dos_manos else "", " · crít +%d%%" % r.critico_extra if r.critico_extra else ""]
	elif r is MochilaData:
		tipo = "Mochila"
		datos_txt = "%.1f kg de capacidad" % r.capacidad
	elif r is EquipoData:
		tipo = "Equipo"
		var mods: PackedStringArray = []
		for m in r.modificadores:
			if m:
				mods.append(m.describir())
		datos_txt = RANURAS[r.ranura] + " · " + ", ".join(mods)
		if not r.clases_permitidas.is_empty():
			datos_txt += " · solo " + ", ".join(r.clases_permitidas)
	else:
		tipo = "Consumible" if r.tipo == ObjetoData.Tipo.CONSUMIBLE else "Objeto"
		var efs: PackedStringArray = []
		for e in r.efectos_al_usar:
			if e:
				efs.append(e.describir())
		datos_txt = ", ".join(efs)
		if r.usos > 0:
			datos_txt += " · %d uso(s)" % r.usos
	return [r.nombre_completo(), r.obtener_id(), tipo, datos_txt, "%.1f kg" % r.peso, "%d oro" % r.valor, r.icono]

func _seleccionado() -> String:
	var item := arbol.get_selected()
	return item.get_metadata(0) if item else ""

func _al_seleccionar() -> void:
	var ruta := _seleccionado()
	if ruta != "" and Engine.is_editor_hint():
		EditorInterface.edit_resource(load(ruta))

func _al_activar() -> void:
	var ruta := _seleccionado()
	if ruta != "" and Engine.is_editor_hint():
		EditorInterface.get_file_system_dock().navigate_to_path(ruta)

# ==========================================
# CREAR, DUPLICAR, BORRAR
# ==========================================

func _pedir_id(indice: int) -> void:
	nuevo_pendiente = indice
	dialogo_id.title = "Nuevo: " + NUEVOS[indice][0]
	campo_id.text = ""
	dialogo_id.popup_centered(Vector2(380, 90))
	campo_id.grab_focus()

# Crea el recurso con valores de partida razonables y lo guarda en su carpeta. Devuelve la ruta.
func crear(indice: int, id: String) -> String:
	id = id.strip_edges().to_snake_case().validate_filename()
	if indice < 0 or id == "":
		return ""
	var tipo: Array = NUEVOS[indice]
	var ruta: String = tipo[2].path_join(id + ".tres")
	if FileAccess.file_exists(ruta):
		push_warning("[Base de datos] ya existe " + ruta)
		return ""
	var r: Resource = load(tipo[1]).new()
	var nombre := id.replace("_", " ").capitalize()
	if r is EventoData:
		# Arranca con una opción y el arte de placeholder (Assets/Eventos/_placeholder)
		r.titulo = nombre
		r.ilustracion = load("res://Assets/Eventos/_placeholder/escena.png")
		r.sprite_mapa = load("res://Assets/Eventos/_placeholder/mapa.png")
		r.imagen_exito = load("res://Assets/Eventos/_placeholder/exito.png")
		r.imagen_fracaso = load("res://Assets/Eventos/_placeholder/fracaso.png")
		var o := OpcionEvento.new()
		r.opciones.append(o)
	elif r is EnemigoData:
		r.nombre = nombre
	elif r is TablaBotin:
		r.id = id
	else:
		r.nombre = nombre
		if indice == 3: # consumible: se apila, se gasta y cura 10 por defecto
			r.tipo = ObjetoData.Tipo.CONSUMIBLE
			r.apilable = true
			r.max_pila = 5
			r.usos = 1
			r.peso = 0.5
			var e := Efecto.new()
			r.efectos_al_usar.append(e)
		elif indice == 1:
			r.tipo = ObjetoData.Tipo.ARMADURA
	DirAccess.make_dir_recursive_absolute(ProjectSettings.globalize_path(tipo[2]))
	if ResourceSaver.save(r, ruta) != OK:
		push_error("[Base de datos] no se pudo guardar " + ruta)
		return ""
	_tras_cambiar_archivos(ruta)
	return ruta

func _duplicar() -> void:
	var ruta := _seleccionado()
	if ruta == "":
		return
	var original: Resource = load(ruta)
	var base := ruta.get_basename()
	var nueva := base + "_copia.tres"
	var n := 2
	while FileAccess.file_exists(nueva):
		nueva = base + "_copia%d.tres" % n
		n += 1
	var copia: Resource = original.duplicate(true)
	if "id" in copia:
		copia.id = "" # usará el nombre del archivo nuevo: así no se repite el id
	if "nombre" in copia:
		copia.nombre += " (copia)"
	if ResourceSaver.save(copia, nueva) != OK:
		push_error("[Base de datos] no se pudo duplicar " + ruta)
		return
	_tras_cambiar_archivos(nueva)

func _borrar() -> void:
	var ruta := _seleccionado()
	if ruta == "":
		return
	OS.move_to_trash(ProjectSettings.globalize_path(ruta))
	_tras_cambiar_archivos("")

func _tras_cambiar_archivos(seleccionar: String) -> void:
	if Engine.is_editor_hint():
		EditorInterface.get_resource_filesystem().scan()
	recargar()
	if seleccionar == "":
		return
	var item := arbol.get_root().get_first_child() if arbol.get_root() else null
	while item:
		if item.get_metadata(0) == seleccionar:
			item.select(0)
			arbol.scroll_to_item(item)
			break
		item = item.get_next()
