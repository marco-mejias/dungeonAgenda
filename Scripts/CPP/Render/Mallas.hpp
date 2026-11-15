#pragma once
// Piezas de construcción del mundo 3D del DungeonRenderer: bloques con UV por cara,
// materiales de overlay, MultiMesh, partículas y sprites del tileset. Sin estado propio.

#include "Scripts/CPP/Mazmorra/Alturas.hpp"
#include <Godot/godot.hpp>
#include <Godot/classes/node3d.hpp>
#include <Godot/classes/mesh_instance3d.hpp>
#include <Godot/classes/multi_mesh_instance3d.hpp>
#include <Godot/classes/multi_mesh.hpp>
#include <Godot/classes/array_mesh.hpp>
#include <Godot/classes/mesh.hpp>
#include <Godot/classes/material.hpp>
#include <Godot/classes/standard_material3d.hpp>
#include <Godot/classes/base_material3d.hpp>
#include <Godot/classes/texture2d.hpp>
#include <Godot/classes/cpu_particles3d.hpp>
#include <Godot/classes/quad_mesh.hpp>
#include <Godot/classes/gradient.hpp>
#include <Godot/classes/sprite3d.hpp>

namespace render {

using namespace godot;

// Tipos de casilla que nos manda el DungeonManager
const uint8_t T_MURO = 0;
const uint8_t T_SUELO = 1;
const uint8_t T_PUERTA = 2;
const uint8_t T_PUERTA_ABIERTA = 3;
const uint8_t T_COLUMNA = 4; // columna dentro de una sala (suelo debajo y un pilar fino encima)

const float WALL_H = 1.0f;
const float TILE = 0.1f; // 16px de 160px en el atlas

inline Vector4 Tile(int col, int fila) { return Vector4(col * TILE, fila * TILE, TILE, TILE); }

// Centro de la casilla, a la altura de su suelo (Mazmorra/Alturas.hpp)
inline Vector3 GridToWorld(int x, int y) { return Vector3(x + 0.5f, alturas::Y(x, y), y + 0.5f); }

// Bloque de base 1x1 centrado en el origen, desde y=0 hasta y=h. UV2.x = 1 en la tapa superior.
inline Ref<ArrayMesh> CrearBloque(float h, bool con_lados = true) {
	PackedVector3Array v, n;
	PackedVector2Array uv, uv2;
	PackedInt32Array idx;

	auto cara = [&](Vector3 a, Vector3 b, Vector3 c, Vector3 d, Vector3 normal, float top) {
		int base = v.size();
		v.push_back(a); v.push_back(b); v.push_back(c); v.push_back(d);
		for (int i = 0; i < 4; i++) { n.push_back(normal); uv2.push_back(Vector2(top, 0)); }
		uv.push_back(Vector2(0, 0)); uv.push_back(Vector2(1, 0)); uv.push_back(Vector2(1, 1)); uv.push_back(Vector2(0, 1));
		idx.push_back(base); idx.push_back(base + 1); idx.push_back(base + 2);
		idx.push_back(base); idx.push_back(base + 2); idx.push_back(base + 3);
	};

	const float s = 0.5f;
	if (con_lados) {
		cara(Vector3(-s, h, s), Vector3(s, h, s), Vector3(s, 0, s), Vector3(-s, 0, s), Vector3(0, 0, 1), 0);
		cara(Vector3(s, h, -s), Vector3(-s, h, -s), Vector3(-s, 0, -s), Vector3(s, 0, -s), Vector3(0, 0, -1), 0);
		cara(Vector3(s, h, s), Vector3(s, h, -s), Vector3(s, 0, -s), Vector3(s, 0, s), Vector3(1, 0, 0), 0);
		cara(Vector3(-s, h, -s), Vector3(-s, h, s), Vector3(-s, 0, s), Vector3(-s, 0, -s), Vector3(-1, 0, 0), 0);
	}
	cara(Vector3(-s, h, -s), Vector3(s, h, -s), Vector3(s, h, s), Vector3(-s, h, s), Vector3(0, 1, 0), 1);

	Array arrays;
	arrays.resize(Mesh::ARRAY_MAX);
	arrays[Mesh::ARRAY_VERTEX] = v;
	arrays[Mesh::ARRAY_NORMAL] = n;
	arrays[Mesh::ARRAY_TEX_UV] = uv;
	arrays[Mesh::ARRAY_TEX_UV2] = uv2;
	arrays[Mesh::ARRAY_INDEX] = idx;

	Ref<ArrayMesh> mesh;
	mesh.instantiate();
	mesh->add_surface_from_arrays(Mesh::PRIMITIVE_TRIANGLES, arrays);
	return mesh;
}

inline Ref<StandardMaterial3D> CrearMaterialOverlay(Color color, bool color_instancia = false) {
	Ref<StandardMaterial3D> mat;
	mat.instantiate();
	mat->set_shading_mode(BaseMaterial3D::SHADING_MODE_UNSHADED);
	mat->set_transparency(BaseMaterial3D::TRANSPARENCY_ALPHA);
	mat->set_cull_mode(BaseMaterial3D::CULL_DISABLED);
	mat->set_flag(BaseMaterial3D::FLAG_DISABLE_FOG, true);
	mat->set_flag(BaseMaterial3D::FLAG_ALBEDO_FROM_VERTEX_COLOR, color_instancia);
	mat->set_albedo(color);
	return mat;
}

inline MultiMeshInstance3D* CrearMultiMesh(const String& nombre, Ref<Mesh> mesh, Ref<Material> mat, bool custom_data, bool colores = false) {
	Ref<MultiMesh> mm;
	mm.instantiate();
	mm->set_transform_format(MultiMesh::TRANSFORM_3D);
	mm->set_use_custom_data(custom_data);
	mm->set_use_colors(colores);
	mm->set_mesh(mesh);
	MultiMeshInstance3D* inst = memnew(MultiMeshInstance3D);
	inst->set_name(nombre);
	inst->set_multimesh(mm);
	if (mat.is_valid()) inst->set_material_override(mat);
	return inst;
}

inline String NombreProp(const String& tipo, int x, int y) {
	return tipo + String("_") + String::num_int64(x) + String("_") + String::num_int64(y);
}

inline MeshInstance3D* CrearPieza(Node* padre, const String& nombre, Ref<Mesh> mesh, Ref<Material> mat, Vector3 pos, Vector3 escala) {
	MeshInstance3D* m = memnew(MeshInstance3D);
	m->set_name(nombre);
	m->set_mesh(mesh);
	m->set_material_override(mat);
	m->set_position(pos);
	m->set_scale(escala);
	padre->add_child(m);
	return m;
}

inline CPUParticles3D* CrearParticulas(Node* padre, const String& nombre, int cantidad, float vida, float tam, Color color, Vector3 caja, Vector3 gravedad, float vel_min, float vel_max, float dispersion) {
	CPUParticles3D* p = memnew(CPUParticles3D);
	p->set_name(nombre);
	Ref<QuadMesh> quad;
	quad.instantiate();
	quad->set_size(Vector2(tam, tam));
	Ref<StandardMaterial3D> mat;
	mat.instantiate();
	mat->set_shading_mode(BaseMaterial3D::SHADING_MODE_UNSHADED);
	mat->set_billboard_mode(BaseMaterial3D::BILLBOARD_ENABLED);
	mat->set_transparency(BaseMaterial3D::TRANSPARENCY_ALPHA);
	mat->set_flag(BaseMaterial3D::FLAG_ALBEDO_FROM_VERTEX_COLOR, true);
	mat->set_flag(BaseMaterial3D::FLAG_DISABLE_FOG, true);
	quad->set_material(mat);
	p->set_mesh(quad);
	p->set_amount(cantidad);
	p->set_lifetime(vida);
	p->set_emission_shape(CPUParticles3D::EMISSION_SHAPE_BOX);
	p->set_emission_box_extents(caja);
	p->set_direction(Vector3(0, 1, 0));
	p->set_spread(dispersion);
	p->set_gravity(gravedad);
	p->set_param_min(CPUParticles3D::PARAM_INITIAL_LINEAR_VELOCITY, vel_min);
	p->set_param_max(CPUParticles3D::PARAM_INITIAL_LINEAR_VELOCITY, vel_max);
	Ref<Gradient> rampa;
	rampa.instantiate();
	rampa->set_offset(0, 0.0);
	rampa->set_color(0, Color(color.r, color.g, color.b, 0.0));
	rampa->set_offset(1, 1.0);
	rampa->set_color(1, Color(color.r, color.g, color.b, 0.0));
	rampa->add_point(0.3, color);
	rampa->add_point(0.7, color);
	p->set_color_ramp(rampa);
	p->set_pre_process_time(vida);
	padre->add_child(p);
	return p;
}

inline Sprite3D* CrearSpriteTile(Ref<Texture2D> atlas, Node* padre, const String& nombre, Rect2 region, float pixel, Vector3 pos, bool sombreado) {
	Sprite3D* sp = memnew(Sprite3D);
	sp->set_name(nombre);
	sp->set_texture(atlas);
	sp->set_region_enabled(true);
	sp->set_region_rect(region);
	sp->set_pixel_size(pixel);
	sp->set_billboard_mode(BaseMaterial3D::BILLBOARD_FIXED_Y);
	sp->set_alpha_cut_mode(SpriteBase3D::ALPHA_CUT_DISCARD);
	sp->set_texture_filter(BaseMaterial3D::TEXTURE_FILTER_NEAREST);
	sp->set_draw_flag(SpriteBase3D::FLAG_SHADED, sombreado);
	sp->set_position(pos);
	padre->add_child(sp);
	return sp;
}

} // namespace render
