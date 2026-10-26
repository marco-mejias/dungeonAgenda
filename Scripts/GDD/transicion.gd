extends CanvasLayer

# Fundido a negro con el rótulo del piso ("PISO 2"). Bloquea el ratón mientras se ve.
#   fundido("PISO 2", callable_a_mitad)  -> negro, llama al callable, enseña el rótulo y vuelve

const T_FUNDIDO := 0.55
const T_ROTULO := 0.9

@onready var negro: ColorRect = $Negro
@onready var rotulo: Label = $Rotulo

func _ready() -> void:
	layer = 20
	# Entrada al juego: empezamos en negro con "PISO 1"
	negro.color.a = 1.0
	rotulo.text = "PISO 1"
	rotulo.modulate.a = 1.0
	negro.mouse_filter = Control.MOUSE_FILTER_STOP
	var tw := create_tween()
	tw.tween_interval(T_ROTULO * 0.7)
	tw.tween_property(rotulo, "modulate:a", 0.0, 0.35)
	tw.tween_property(negro, "color:a", 0.0, T_FUNDIDO)
	tw.tween_callback(_soltar_raton)

func fundido(texto: String, a_mitad: Callable) -> void:
	negro.mouse_filter = Control.MOUSE_FILTER_STOP
	rotulo.text = texto
	rotulo.modulate.a = 0.0
	var tw := create_tween()
	tw.tween_property(negro, "color:a", 1.0, T_FUNDIDO).set_trans(Tween.TRANS_SINE)
	tw.tween_callback(a_mitad)
	tw.tween_property(rotulo, "modulate:a", 1.0, 0.3)
	tw.tween_interval(T_ROTULO)
	tw.tween_property(rotulo, "modulate:a", 0.0, 0.3)
	tw.tween_property(negro, "color:a", 0.0, T_FUNDIDO).set_trans(Tween.TRANS_SINE)
	tw.tween_callback(_soltar_raton)

func _soltar_raton() -> void:
	negro.mouse_filter = Control.MOUSE_FILTER_IGNORE
