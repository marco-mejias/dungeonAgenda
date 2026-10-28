#pragma once
// Fichas de personaje del HUD (columna izquierda): estilo de ventana PS1, barras con icono,
// rombos de PA animados, ficha de héroe y de enemigo, y qué fichas se ven en cada momento
// (AUTO = quien sale en pantalla + el héroe activo; además las fijadas y el enemigo bajo el cursor).

#include <Godot/godot.hpp>
#include <Godot/classes/node3d.hpp>
#include <Godot/classes/camera3d.hpp>
#include <Godot/classes/control.hpp>
#include <Godot/classes/label.hpp>
#include <Godot/classes/progress_bar.hpp>
#include <Godot/classes/color_rect.hpp>
#include <Godot/classes/texture_rect.hpp>
#include <Godot/classes/texture2d.hpp>
#include <Godot/classes/atlas_texture.hpp>
#include <Godot/classes/style_box_flat.hpp>
#include <Godot/classes/h_box_container.hpp>
#include <Godot/classes/resource_loader.hpp>
#include <Godot/classes/tween.hpp>
#include <Godot/classes/property_tweener.hpp>
#include <Godot/classes/interval_tweener.hpp>
#include <Godot/variant/utility_functions.hpp>
#include <vector>
#include "Scripts/CPP/Inventario/Inventario.hpp"

namespace ui {

using namespace godot;

const int MAX_FICHAS_ENEMIGO = 4;
const Color BORDE_NORMAL = Color(0.55, 0.6, 0.85);
const Color BORDE_ACTIVO = Color(1.0, 0.8, 0.25);
const Color BORDE_ENEMIGO = Color(0.6, 0.18, 0.16);
const Color BORDE_ENEMIGO_ALERTA = Color(1.0, 0.3, 0.2);

inline bool EstaFijada(const std::vector<String>& fijadas, const String& nombre) {
	for (const String& n : fijadas) if (n == nombre) return true;
	return false;
}

inline String GenerarTextoStats(Node* character) {
	if (!character) return "";
	
	int spd = character->has_meta("velocidad") ? (int)character->get_meta("velocidad") : 0;
	int str = character->has_meta("fuerza")    ? (int)character->get_meta("fuerza")    : 0;
	int def = character->has_meta("defensa")   ? (int)character->get_meta("defensa")   : 0;
	int mag = character->has_meta("magia")     ? (int)character->get_meta("magia")     : 0;
	int lck = character->has_meta("suerte")    ? (int)character->get_meta("suerte")    : 0;

	String texto = "Velocidad: " + String::num_int64(spd) + "\n" +
		   "Fuerza: "    + String::num_int64(str) + "\n" +
		   "Defensa: "   + String::num_int64(def) + "\n" +
		   "Magia: "     + String::num_int64(mag) + "\n" +
		   "Suerte: "    + String::num_int64(lck);

	// Arma y carga de la mochila (solo los héroes tienen equipo)
	if (character->has_meta("equipo")) {
		Ref<Resource> arma = inventario::Arma(character);
		texto += "\nArma: " + (arma.is_valid() ? inventario::NombreDe(arma) : String::utf8("pu\xC3\xB1os"));
		texto += "\nCarga: " + String::num(inventario::PesoInventario(character), 1) + " / " + String::num(inventario::Capacidad(character), 1) + " kg";
	}
	return texto;
}

inline Ref<StyleBoxFlat> EstiloFicha(Color borde) {
	Ref<StyleBoxFlat> s;
	s.instantiate();
	s->set_bg_color(Color(0.04, 0.05, 0.16, 0.9));
	s->set_border_width_all(2);
	s->set_border_color(borde);
	s->set_corner_radius_all(3);
	s->set_corner_detail(1);
	s->set_shadow_color(Color(0, 0, 0, 0.5));
	s->set_shadow_size(2);
	s->set_shadow_offset(Vector2(2, 2));
	s->set_content_margin(SIDE_LEFT, 5);
	s->set_content_margin(SIDE_RIGHT, 5);
	s->set_content_margin(SIDE_TOP, 4);
	s->set_content_margin(SIDE_BOTTOM, 4);
	return s;
}

inline void PintarBorde(Control* ficha, Color borde) {
	if (ficha->has_meta("borde") && (Color)ficha->get_meta("borde") == borde) return;
	ficha->set_meta("borde", borde);
	ficha->add_theme_stylebox_override("panel", EstiloFicha(borde));
}

inline void FijarTexto(Control* ficha, const char* ruta, const String& texto) {
	Label* l = Object::cast_to<Label>(ficha->get_node_or_null(ruta));
	if (l) l->set_text(texto);
}

inline void FijarBarra(Control* ficha, const char* fila, int valor, int maximo) {
	String base = String("V/Cabecera/Info/") + fila;
	ProgressBar* barra = Object::cast_to<ProgressBar>(ficha->get_node_or_null(NodePath(base + "/Barra")));
	if (barra) { barra->set_max(UtilityFunctions::maxi(1, maximo)); barra->set_value(valor); }
	Label* l = Object::cast_to<Label>(ficha->get_node_or_null(NodePath(base + "/Valor")));
	if (l) l->set_text(String::num_int64(valor));
}

// PA como rombos (llenos = disponibles)
inline void PintarPA(Control* ficha, int pa, int pa_max) {
	HBoxContainer* caja = Object::cast_to<HBoxContainer>(ficha->get_node_or_null("V/Cabecera/Info/Fila/PA"));
	if (!caja) return;
	int clave = pa * 100 + pa_max;
	if (caja->has_meta("pintado") && (int)caja->get_meta("pintado") == clave) return;
	int pa_antes = caja->has_meta("pa_antes") ? (int)caja->get_meta("pa_antes") : pa;
	caja->set_meta("pintado", clave);
	caja->set_meta("pa_antes", pa);

	const Color LLENO(0.4, 0.8, 1.0);
	const Color VACIO(0.16, 0.18, 0.32);
	TypedArray<Node> viejos = caja->get_children();
	for (int i = 0; i < viejos.size(); i++) {
		Node* n = Object::cast_to<Node>(viejos[i]);
		caja->remove_child(n);
		n->queue_free();
	}
	for (int i = 0; i < pa_max; i++) {
		ColorRect* r = memnew(ColorRect);
		r->set_custom_minimum_size(Vector2(5, 5));
		r->set_pivot_offset(Vector2(2.5, 2.5));
		r->set_rotation((float)Math_PI * 0.25f);
		r->set_mouse_filter(Control::MOUSE_FILTER_IGNORE);
		caja->add_child(r);
		bool recien_gastado = i >= pa && i < pa_antes;
		bool recien_ganado = i < pa && i >= pa_antes;
		if (recien_gastado || recien_ganado) {
			// Los rombos que cambian destellan y se vacían/llenan uno detrás de otro
			r->set_color(recien_gastado ? Color(1, 1, 1) : VACIO);
			Ref<Tween> tw = r->create_tween();
			float retraso = 0.07f * (recien_gastado ? (pa_antes - 1 - i) : (i - pa_antes));
			tw->tween_interval(retraso);
			tw->tween_property(r, "scale", Vector2(1.8, 1.8), 0.08);
			tw->parallel()->tween_property(r, "color", recien_gastado ? VACIO : LLENO, 0.2);
			tw->tween_property(r, "scale", Vector2(1, 1), 0.12);
		} else {
			r->set_color(i < pa ? LLENO : VACIO);
		}
	}
}

inline void RellenarFichaHeroe(Control* f, Node* p, Node* activo) {
	FijarTexto(f, "V/Cabecera/Info/Fila/Nombre", p->get_meta("nombre_display", p->get_name()));
	FijarBarra(f, "Vida", p->get_meta("hp_actual", 0), p->get_meta("hp_max", 100));
	FijarBarra(f, "Hambre", p->get_meta("hambre_actual", 0), p->get_meta("hambre_max", 10));
	FijarBarra(f, "Cordura", p->get_meta("sanidad_actual", 0), p->get_meta("sanidad_max", 100));
	PintarPA(f, p->get_meta("pa_actual", 0), p->get_meta("pa_max", 0));

	FijarTexto(f, "V/Detalle/Stats", GenerarTextoStats(p));
	FijarTexto(f, "V/Detalle/Recursos/Oro", String::num_int64((int)p->get_meta("oro", 0)));
	FijarTexto(f, "V/Detalle/Recursos/Comida", "Comida: " + String::num_int64((int)p->get_meta("comida_bolsa", 0)));

	// Las 4 casillas de la ficha muestran lo que lleva puesto
	const char* const RANURAS_FICHA[4] = { "mano_principal", "mano_secundaria", "cuerpo", "cabeza" };
	for (int s = 1; s <= 4; s++) {
		TextureRect* slot = Object::cast_to<TextureRect>(f->get_node_or_null(NodePath("V/Detalle/Slots/Slot" + String::num_int64(s))));
		if (!slot) continue;
		Ref<Resource> item = inventario::Equipado(p, RANURAS_FICHA[s - 1]);
		Ref<Texture2D> icono;
		if (item.is_valid()) icono = item->get("icono");
		slot->set_texture(icono);
		slot->set_tooltip_text(item.is_valid() ? inventario::NombreDe(item) : String(""));
	}

	bool muerto = p->get_meta("esta_muerto", false);
	bool escapado = p->get_meta("ha_escapado", false);
	bool agotado = p->get_meta("ha_movido", false) || (int)p->get_meta("pa_actual", 0) <= 0;
	if (muerto) f->set_modulate(Color(0.7, 0.3, 0.3, 0.8));
	else if (escapado) f->set_modulate(Color(0.6, 0.6, 0.7, 0.55));
	else if (agotado && p != activo) f->set_modulate(Color(0.6, 0.6, 0.65, 1.0));
	else f->set_modulate(Color(1, 1, 1, 1));
	PintarBorde(f, p == activo ? BORDE_ACTIVO : BORDE_NORMAL);
}

inline void RellenarFichaEnemigo(Control* f, Node3D* e) {
	FijarTexto(f, "V/Cabecera/Info/Fila/Nombre", e->get_meta("nombre_display", "Enemigo"));
	FijarBarra(f, "Vida", e->get_meta("hp_actual", 1), e->get_meta("hp_max", 1));
	FijarTexto(f, "V/Detalle/Stats", "Fuerza: " + String::num_int64((int)e->get_meta("fuerza", 0)) +
										 "\nDefensa: " + String::num_int64((int)e->get_meta("defensa", 0)));
	f->set_meta("enemigo", e->get_name());

	int frame = e->get_meta("frame", 11);
	if (!f->has_meta("frame_pintado") || (int)f->get_meta("frame_pintado") != frame) {
		f->set_meta("frame_pintado", frame);
		// Se carga la primera vez que hace falta (no al cargar la DLL)
		static Ref<Texture2D> hoja = ResourceLoader::get_singleton()->load("res://Assets/2D Pixel Dungeon Asset Pack/character and tileset/Dungeon_Character_2.png");
		Ref<AtlasTexture> retrato;
		retrato.instantiate();
		retrato->set_atlas(hoja);
		retrato->set_region(Rect2((frame % 7) * 16, (frame / 7) * 16, 16, 16));
		TextureRect* r = Object::cast_to<TextureRect>(f->get_node_or_null("V/Cabecera/Retrato"));
		if (r) r->set_texture(retrato);
	}
	bool alerta = e->get_meta("in_combat", false);
	PintarBorde(f, alerta ? BORDE_ENEMIGO_ALERTA : BORDE_ENEMIGO);
}

// Qué fichas se ven: fijadas, el activo y lo que hay en pantalla (en AUTO) y el enemigo bajo el cursor
inline void ActualizarVisibilidad(Control* panel, Node* pm, Node* dm, const TypedArray<Node>& heroes, bool modo_auto, const std::vector<String>& fijadas) {
	if (!panel || !pm || !dm) return;
	Camera3D* cam = Object::cast_to<Camera3D>(dm->get_node_or_null("Camera3D"));
	Node* activo = Object::cast_to<Node>(pm->call("GetActiveCharacter"));

	auto en_pantalla = [&](Node3D* n) {
		return n && n->is_visible() && cam && cam->is_position_in_frustum(n->get_global_position() + Vector3(0, 0.5, 0));
	};

	for (int i = 0; i < heroes.size(); i++) {
		Node* p = Object::cast_to<Node>(heroes[i]);
		String nombre = p->get_name();
		Control* f = Object::cast_to<Control>(panel->get_node_or_null(NodePath(nombre)));
		if (!f) continue;
		Node3D* token = Object::cast_to<Node3D>(dm->get_node_or_null(NodePath(nombre)));
		f->set_visible(EstaFijada(fijadas, nombre) || (modo_auto && (p == activo || en_pantalla(token))));
	}

	Vector2i hover = dm->get_meta("hover_celda", Vector2i(-1, -1));
	String nombre_hover = "Enemy_" + String::num_int64(hover.x) + "_" + String::num_int64(hover.y);
	std::vector<Node3D*> enemigos;
	TypedArray<Node> hijos = dm->get_children();
	for (int i = 0; i < hijos.size(); i++) {
		Node3D* e = Object::cast_to<Node3D>(hijos[i]);
		if (!e || !e->is_visible()) continue;
		String n = e->get_name();
		if (!n.begins_with("Enemy_") || n.ends_with("_muerto")) continue;
		if (n == nombre_hover) enemigos.insert(enemigos.begin(), e);
		else if (modo_auto && en_pantalla(e)) enemigos.push_back(e);
	}
	for (int k = 0; k < MAX_FICHAS_ENEMIGO; k++) {
		Control* f = Object::cast_to<Control>(panel->get_node_or_null(NodePath("Enemigo" + String::num_int64(k))));
		if (!f) continue;
		bool usada = k < (int)enemigos.size();
		f->set_visible(usada);
		if (usada) RellenarFichaEnemigo(f, enemigos[k]);
	}
}

} // namespace ui
