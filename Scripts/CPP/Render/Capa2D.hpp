#pragma once
// Capa 2D del DungeonRenderer: elementos de interfaz anclados a posiciones 3D
// (bocadillos de cómic, "!" y barra de vida de enemigos). Se dibujan dentro del
// SubViewport de baja resolución, así que se pixelan igual que el mundo.

#include <Godot/godot.hpp>
#include <Godot/classes/camera3d.hpp>
#include <Godot/classes/viewport.hpp>
#include <Godot/classes/control.hpp>
#include <Godot/classes/panel_container.hpp>
#include <Godot/classes/label.hpp>
#include <Godot/classes/progress_bar.hpp>
#include <Godot/classes/polygon2d.hpp>
#include <Godot/classes/style_box_flat.hpp>
#include <Godot/classes/tween.hpp>
#include <Godot/classes/property_tweener.hpp>
#include <Godot/variant/utility_functions.hpp>
#include <cmath>

namespace render {

using namespace godot;

// Estilos de bocadillo
const int B_NORMAL = 0;
const int B_SIN_PA = 1;
const int B_HOVER = 2;


inline Ref<StyleBoxFlat> EstiloGlobo(int estilo) {
	Ref<StyleBoxFlat> s;
	s.instantiate();
	Color fondo = estilo == B_SIN_PA ? Color(1.0, 0.86, 0.84) : (estilo == B_HOVER ? Color(0.9, 0.92, 1.0, 0.94) : Color(0.98, 0.96, 0.88));
	s->set_bg_color(fondo);
	s->set_border_color(Color(0.05, 0.04, 0.06));
	s->set_border_width_all(1);
	s->set_corner_radius_all(3);
	s->set_corner_detail(2);
	s->set_content_margin(SIDE_LEFT, 4);
	s->set_content_margin(SIDE_RIGHT, 4);
	s->set_content_margin(SIDE_TOP, 1);
	s->set_content_margin(SIDE_BOTTOM, 2);
	return s;
}

inline Polygon2D* Triangulo(const String& nombre, Vector2 a, Vector2 b, Vector2 c, Color color) {
	Polygon2D* p = memnew(Polygon2D);
	p->set_name(nombre);
	PackedVector2Array pts;
	pts.push_back(a); pts.push_back(b); pts.push_back(c);
	p->set_polygon(pts);
	p->set_color(color);
	return p;
}

// wrapper (posición + zoom) > Escala (animación de aparición) > Cola + Globo > Texto
inline Control* CrearBocadillo(const String& nombre, const String& texto, int estilo) {
	Control* wrapper = memnew(Control);
	wrapper->set_name(nombre);
	wrapper->set_mouse_filter(Control::MOUSE_FILTER_IGNORE);

	Control* escala = memnew(Control);
	escala->set_name("Escala");
	escala->set_mouse_filter(Control::MOUSE_FILTER_IGNORE);
	wrapper->add_child(escala);

	Ref<StyleBoxFlat> estilo_globo = EstiloGlobo(estilo);
	escala->add_child(Triangulo("ColaBorde", Vector2(-4, -6), Vector2(4, -6), Vector2(0, 1), Color(0.05, 0.04, 0.06)));

	PanelContainer* globo = memnew(PanelContainer);
	globo->set_name("Globo");
	globo->set_mouse_filter(Control::MOUSE_FILTER_IGNORE);
	globo->add_theme_stylebox_override("panel", estilo_globo);
	escala->add_child(globo);

	escala->add_child(Triangulo("Cola", Vector2(-3, -7), Vector2(3, -7), Vector2(0, -1), estilo_globo->get_bg_color()));

	Label* label = memnew(Label);
	label->set_name("Texto");
	label->set_text(texto);
	label->set_horizontal_alignment(HORIZONTAL_ALIGNMENT_CENTER);
	label->add_theme_font_size_override("font_size", estilo == B_HOVER ? 7 : 8);
	label->add_theme_color_override("font_color", estilo == B_SIN_PA ? Color(0.6, 0.05, 0.05) : Color(0.08, 0.06, 0.1));
	label->add_theme_constant_override("line_spacing", -2);
	globo->add_child(label);

	wrapper->set_meta("texto", texto);
	wrapper->set_meta("estilo", estilo);

	escala->set_scale(Vector2(0.2, 0.2));
	Ref<Tween> tw = escala->create_tween();
	tw->tween_property(escala, "scale", Vector2(1, 1), 0.18)->set_trans(Tween::TRANS_BACK)->set_ease(Tween::EASE_OUT);
	return wrapper;
}

inline float EscalaZoom(Camera3D* cam) {
	float z = cam && cam->has_meta("zoom_factor") ? (float)cam->get_meta("zoom_factor") : 2.0f;
	// Con menos reducción de resolución (opción del menú) el viewport es mayor: compensamos
	float shrink = 2.0f;
	Node* sub = cam ? cam->get_viewport() : nullptr;
	Node* contenedor = sub ? sub->get_parent() : nullptr;
	if (contenedor && contenedor->has_method("get_stretch_shrink")) shrink = (float)(int)contenedor->call("get_stretch_shrink");
	return UtilityFunctions::clampf(z * 0.5f, 0.7f, 1.4f) * (2.0f / UtilityFunctions::maxf(1.0f, shrink));
}

// Coloca un elemento 2D sobre un punto 3D; lo oculta si queda detrás de la cámara
inline void Anclar(Control* c, Camera3D* cam, Vector3 mundo, float escala) {
	if (!c || !cam) return;
	if (cam->is_position_behind(mundo)) { c->set_visible(false); return; }
	c->set_position(cam->unproject_position(mundo).round());
	c->set_scale(Vector2(escala, escala));
}

inline void ColocarGlobo(Control* wrapper) {
	PanelContainer* globo = Object::cast_to<PanelContainer>(wrapper->get_node_or_null("Escala/Globo"));
	if (!globo) return;
	Vector2 sz = globo->get_combined_minimum_size();
	globo->set_size(sz);
	globo->set_position(Vector2(std::round(-sz.x * 0.5f), std::round(-sz.y - 5.0f)));
}

inline Control* CrearUIEnemigo(const String& nombre) {
	Control* ui = memnew(Control);
	ui->set_name(nombre);
	ui->set_mouse_filter(Control::MOUSE_FILTER_IGNORE);

	Label* alerta = memnew(Label);
	alerta->set_name("Alerta");
	alerta->set_text("!");
	alerta->add_theme_font_size_override("font_size", 14);
	alerta->add_theme_color_override("font_color", Color(1.0, 0.85, 0.15));
	alerta->add_theme_color_override("font_outline_color", Color(0.15, 0.0, 0.0));
	alerta->add_theme_constant_override("outline_size", 3);
	alerta->set_position(Vector2(-3, -30));
	alerta->set_visible(false);
	ui->add_child(alerta);

	ProgressBar* vida = memnew(ProgressBar);
	vida->set_name("Vida");
	vida->set_show_percentage(false);
	vida->set_custom_minimum_size(Vector2(18, 3));
	vida->set_size(Vector2(18, 3));
	vida->set_position(Vector2(-9, -6));
	Ref<StyleBoxFlat> fondo; fondo.instantiate(); fondo->set_bg_color(Color(0.05, 0.02, 0.02));
	fondo->set_border_width_all(1); fondo->set_border_color(Color(0.05, 0.02, 0.02));
	Ref<StyleBoxFlat> relleno; relleno.instantiate(); relleno->set_bg_color(Color(0.9, 0.15, 0.12));
	vida->add_theme_stylebox_override("background", fondo);
	vida->add_theme_stylebox_override("fill", relleno);
	vida->set_visible(false);
	ui->add_child(vida);
	return ui;
}

// Segundo frame de la animación idle de cada sprite (Dungeon_Character_2: pares de frames)
inline int Pareja(int f) {
	switch (f) {
		case 0: return 1; case 1: return 0; case 2: return 3; case 3: return 2;
		case 4: return 5; case 5: return 4; case 6: return 5;
		case 9: return 10; case 10: return 9; case 11: return 12; case 12: return 11; case 13: return 12;
		default: return f;
	}
}

} // namespace render
