#pragma once
// Animaciones de combate compartidas por el CombatManager (ataques de los héroes) y el
// EnemyManager (turno de los enemigos): números flotantes, destellos, empujones,
// estallidos y proyectiles.

#include <Godot/godot.hpp>
#include <Godot/classes/node.hpp>
#include <Godot/classes/node3d.hpp>
#include <Godot/classes/sprite3d.hpp>
#include <Godot/classes/label3d.hpp>
#include <Godot/classes/mesh_instance3d.hpp>
#include <Godot/classes/cpu_particles3d.hpp>
#include <Godot/classes/quad_mesh.hpp>
#include <Godot/classes/standard_material3d.hpp>
#include <Godot/classes/base_material3d.hpp>
#include <Godot/classes/gradient.hpp>
#include <Godot/classes/tween.hpp>
#include <Godot/classes/property_tweener.hpp>
#include <Godot/classes/callback_tweener.hpp>

namespace animacion {

using namespace godot;

// Número flotante sobre una ficha (daño, "¡CRÍTICO!", etc.) con un pequeño rebote
inline void TextoFlotante(Node* dungeon, Vector3 pos, const String& texto, Color color, int tamano = 48) {
	Label3D* label = memnew(Label3D);
	label->set_text(texto);
	label->set_billboard_mode(BaseMaterial3D::BILLBOARD_ENABLED);
	label->set_draw_flag(Label3D::FLAG_DISABLE_DEPTH_TEST, true);
	label->set_texture_filter(BaseMaterial3D::TEXTURE_FILTER_NEAREST);
	label->set_modulate(color);
	label->set_outline_modulate(Color(0, 0, 0));
	label->set_font_size(tamano);
	label->set_outline_size(10);
	label->set_pixel_size(0.008f);
	label->set_position(pos + Vector3(0, 1.0, 0));
	label->set_scale(Vector3(0.4, 0.4, 0.4));
	dungeon->add_child(label);

	Ref<Tween> tw = label->create_tween();
	tw->tween_property(label, "scale", Vector3(1.3, 1.3, 1.3), 0.08)->set_trans(Tween::TRANS_BACK)->set_ease(Tween::EASE_OUT);
	tw->tween_property(label, "scale", Vector3(1, 1, 1), 0.1);
	tw->parallel()->tween_property(label, "position:y", pos.y + 1.6, 0.7)->set_trans(Tween::TRANS_QUAD)->set_ease(Tween::EASE_OUT);
	tw->parallel()->tween_property(label, "modulate:a", 0.0, 0.5)->set_delay(0.35);
	tw->tween_callback(Callable(label, StringName("queue_free")));
}

// Destello blanco (o del color dado) del sprite
inline void Destello(Node3D* ficha, Color color) {
	if (!ficha) return;
	Sprite3D* cuerpo = Object::cast_to<Sprite3D>(ficha->get_node_or_null("Cuerpo"));
	if (!cuerpo) return;
	if (!cuerpo->has_meta("mod_base")) cuerpo->set_meta("mod_base", cuerpo->get_modulate());
	Color base = cuerpo->get_meta("mod_base");
	Ref<Tween> tw = cuerpo->create_tween();
	tw->tween_property(cuerpo, "modulate", color, 0.04);
	tw->tween_property(cuerpo, "modulate", base, 0.22);
}

// El cuerpo se desplaza hacia 'dir' y vuelve (empujón del atacante o retroceso del golpeado)
inline void Desplazar(Node3D* ficha, Vector3 dir, float distancia, float t_ida, float t_vuelta) {
	if (!ficha) return;
	Node3D* cuerpo = Object::cast_to<Node3D>(ficha->get_node_or_null("Cuerpo"));
	if (!cuerpo) return;
	float y_base = cuerpo->has_meta("y_base") ? (float)cuerpo->get_meta("y_base") : 0.44f;
	Vector3 reposo(0, y_base, 0);
	Ref<Tween> tw = cuerpo->create_tween();
	tw->tween_property(cuerpo, "position", reposo + dir * distancia, t_ida)->set_trans(Tween::TRANS_QUAD)->set_ease(Tween::EASE_OUT);
	tw->tween_property(cuerpo, "position", reposo, t_vuelta)->set_trans(Tween::TRANS_QUAD)->set_ease(Tween::EASE_IN_OUT);
}

inline Vector3 Direccion(Node3D* desde, Node3D* hacia) {
	if (!desde || !hacia) return Vector3();
	Vector3 d = hacia->get_position() - desde->get_position();
	d.y = 0;
	return d.length() > 0.001f ? d.normalized() : Vector3();
}

// Estallido de píxeles al morir un enemigo
inline void Estallido(Node* dungeon, Vector3 pos, Color color) {
	CPUParticles3D* p = memnew(CPUParticles3D);
	Ref<QuadMesh> quad;
	quad.instantiate();
	quad->set_size(Vector2(0.07, 0.07));
	Ref<StandardMaterial3D> mat;
	mat.instantiate();
	mat->set_shading_mode(BaseMaterial3D::SHADING_MODE_UNSHADED);
	mat->set_billboard_mode(BaseMaterial3D::BILLBOARD_ENABLED);
	mat->set_flag(BaseMaterial3D::FLAG_ALBEDO_FROM_VERTEX_COLOR, true);
	quad->set_material(mat);
	p->set_mesh(quad);
	p->set_amount(36);
	p->set_lifetime(0.7);
	p->set_one_shot(true);
	p->set_explosiveness_ratio(1.0);
	p->set_emission_shape(CPUParticles3D::EMISSION_SHAPE_SPHERE);
	p->set_emission_sphere_radius(0.2);
	p->set_direction(Vector3(0, 1, 0));
	p->set_spread(70.0);
	p->set_param_min(CPUParticles3D::PARAM_INITIAL_LINEAR_VELOCITY, 1.2);
	p->set_param_max(CPUParticles3D::PARAM_INITIAL_LINEAR_VELOCITY, 2.6);
	p->set_gravity(Vector3(0, -6.0, 0));
	Ref<Gradient> rampa;
	rampa.instantiate();
	rampa->set_color(0, color);
	rampa->set_color(1, Color(color.r * 0.4f, color.g * 0.4f, color.b * 0.4f, 0.0));
	p->set_color_ramp(rampa);
	p->set_position(pos + Vector3(0, 0.45, 0));
	dungeon->add_child(p);
	p->set_emitting(true);
	Ref<Tween> tw = p->create_tween();
	tw->tween_interval(1.2);
	tw->tween_callback(Callable(p, StringName("queue_free")));
}

// Flecha o chispa que viaja de un atacante a su objetivo (armas a distancia). Devuelve la duración.
inline float Proyectil(Node* dungeon, Vector3 desde, Vector3 hasta, Color color) {
	MeshInstance3D* bala = memnew(MeshInstance3D);
	Ref<QuadMesh> quad;
	quad.instantiate();
	quad->set_size(Vector2(0.14, 0.14));
	Ref<StandardMaterial3D> mat;
	mat.instantiate();
	mat->set_shading_mode(BaseMaterial3D::SHADING_MODE_UNSHADED);
	mat->set_billboard_mode(BaseMaterial3D::BILLBOARD_ENABLED);
	mat->set_albedo(color);
	mat->set_flag(BaseMaterial3D::FLAG_DISABLE_DEPTH_TEST, true);
	quad->set_material(mat);
	bala->set_mesh(quad);
	Vector3 alto(0, 0.55, 0);
	bala->set_position(desde + alto);
	dungeon->add_child(bala);
	float t = 0.08f + 0.05f * desde.distance_to(hasta);
	Ref<Tween> tw = bala->create_tween();
	tw->tween_property(bala, "position", hasta + alto, t)->set_trans(Tween::TRANS_SINE)->set_ease(Tween::EASE_IN);
	tw->tween_callback(Callable(bala, StringName("queue_free")));
	return t;
}

} // namespace animacion
