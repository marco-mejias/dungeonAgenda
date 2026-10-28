@tool
extends EditorPlugin

# Añade el panel "Base de datos" a la parte de abajo del editor.

const PanelBaseDatos := preload("res://addons/base_datos/panel_base_datos.gd")

var panel: Control

func _enter_tree() -> void:
	panel = PanelBaseDatos.new()
	panel.name = "BaseDatos"
	add_control_to_bottom_panel(panel, "Base de datos")

func _exit_tree() -> void:
	if panel:
		remove_control_from_bottom_panel(panel)
		panel.queue_free()
		panel = null
