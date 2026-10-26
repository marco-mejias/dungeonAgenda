/* Jenova C++ Node Base Script (Meteora) - DungeonRenderer (3D) */

// La "mano visual": no tiene lógica de juego. El DungeonManager le pasa la cuadrícula
// y le da órdenes (construir el mundo, pintar alcance/ruta, actualizar niebla, abrir cofres...).

#include <Godot/godot.hpp>
#include <Godot/classes/node3d.hpp>
#include <Godot/classes/mesh_instance3d.hpp>
#include <Godot/classes/multi_mesh_instance3d.hpp>
#include <Godot/classes/multi_mesh.hpp>
#include <Godot/classes/array_mesh.hpp>
#include <Godot/classes/prism_mesh.hpp>
#include <Godot/classes/mesh.hpp>
#include <Godot/classes/shader.hpp>
#include <Godot/classes/shader_material.hpp>
#include <Godot/classes/standard_material3d.hpp>
#include <Godot/classes/base_material3d.hpp>
#include <Godot/classes/texture2d.hpp>
#include <Godot/classes/image.hpp>
#include <Godot/classes/image_texture.hpp>
#include <Godot/classes/omni_light3d.hpp>
#include <Godot/classes/sprite3d.hpp>
#include <Godot/classes/camera3d.hpp>
#include <Godot/classes/resource_loader.hpp>
#include <Godot/classes/tween.hpp>
#include <Godot/classes/property_tweener.hpp>
#include <Godot/classes/callback_tweener.hpp>
#include <Godot/classes/scene_tree.hpp>
#include <Godot/classes/time.hpp>
#include <Godot/variant/utility_functions.hpp>
#include <vector>
#include <cmath>

using namespace godot;
using namespace jenova::sdk;

namespace {
	// Tipos de casilla que nos manda el DungeonManager
	const uint8_t T_MURO = 0;
	const uint8_t T_SUELO = 1;
	const uint8_t T_PUERTA = 2;
	const uint8_t T_PUERTA_ABIERTA = 3;

	const float WALL_H = 1.0f;
	const float TILE = 0.1f; // 16px de 160px en el atlas

	Node3D* self = nullptr;
	int grid = 0;
	std::vector<uint8_t> tipos;
	std::vector<uint8_t> niebla;
	bool niebla_debug = false;

	Node3D* m_mundo = nullptr;
	Node3D* m_props = nullptr;
	MultiMeshInstance3D* m_alcance = nullptr;
	MultiMeshInstance3D* m_ruta = nullptr;
	MeshInstance3D* m_cursor = nullptr;

	Ref<Shader> m_shader;
	Ref<Texture2D> m_atlas;
	Ref<Image> m_fog_img;
	Ref<ImageTexture> m_fog_tex;
	Ref<ShaderMaterial> m_mat_suelo, m_mat_muro, m_mat_puerta, m_mat_cofre, m_mat_piedra;
	std::vector<Ref<ShaderMaterial>> m_mats_mundo;

	Vector4 Tile(int col, int fila) { return Vector4(col * TILE, fila * TILE, TILE, TILE); }

	Vector3 GridToWorld(int x, int y) { return Vector3(x + 0.5f, 0.0f, y + 0.5f); }

	bool EsMuro(int x, int y) {
		if (x < 0 || y < 0 || x >= grid || y >= grid) return true;
		return tipos[x * grid + y] == T_MURO;
	}

	// Bloque de base 1x1 centrado en el origen, desde y=0 hasta y=h. UV2.x = 1 en la tapa superior.
	Ref<ArrayMesh> CrearBloque(float h, bool con_lados = true) {
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

	Ref<ShaderMaterial> CrearMaterialMundo(Vector4 lado, Vector4 tapa, bool cutaway, bool tile_instancia) {
		Ref<ShaderMaterial> mat;
		mat.instantiate();
		mat->set_shader(m_shader);
		mat->set_shader_parameter("atlas", m_atlas);
		mat->set_shader_parameter("tile_side", lado);
		mat->set_shader_parameter("tile_top", tapa);
		mat->set_shader_parameter("use_cutaway", cutaway);
		mat->set_shader_parameter("use_instance_tile", tile_instancia);
		mat->set_shader_parameter("fog_tex", m_fog_tex);
		mat->set_shader_parameter("grid_size", (float)grid);
		m_mats_mundo.push_back(mat);
		return mat;
	}

	Ref<StandardMaterial3D> CrearMaterialOverlay(Color color) {
		Ref<StandardMaterial3D> mat;
		mat.instantiate();
		mat->set_shading_mode(BaseMaterial3D::SHADING_MODE_UNSHADED);
		mat->set_transparency(BaseMaterial3D::TRANSPARENCY_ALPHA);
		mat->set_cull_mode(BaseMaterial3D::CULL_DISABLED);
		mat->set_flag(BaseMaterial3D::FLAG_DISABLE_FOG, true);
		mat->set_albedo(color);
		return mat;
	}

	MultiMeshInstance3D* CrearMultiMesh(const String& nombre, Ref<Mesh> mesh, Ref<Material> mat, bool custom_data) {
		Ref<MultiMesh> mm;
		mm.instantiate();
		mm->set_transform_format(MultiMesh::TRANSFORM_3D);
		mm->set_use_custom_data(custom_data);
		mm->set_mesh(mesh);
		MultiMeshInstance3D* inst = memnew(MultiMeshInstance3D);
		inst->set_name(nombre);
		inst->set_multimesh(mm);
		if (mat.is_valid()) inst->set_material_override(mat);
		return inst;
	}

	void RellenarOverlay(MultiMeshInstance3D* inst, const PackedInt32Array& celdas, float altura) {
		if (!inst) return;
		Ref<MultiMesh> mm = inst->get_multimesh();
		int count = celdas.size() / 2;
		mm->set_instance_count(count);
		for (int i = 0; i < count; i++) {
			Vector3 pos = GridToWorld(celdas[i * 2], celdas[i * 2 + 1]);
			pos.y = altura;
			mm->set_instance_transform(i, Transform3D(Basis(), pos));
		}
	}

	String NombreProp(const String& tipo, int x, int y) {
		return tipo + String("_") + String::num_int64(x) + String("_") + String::num_int64(y);
	}

	MeshInstance3D* CrearPieza(Node* padre, const String& nombre, Ref<Mesh> mesh, Ref<Material> mat, Vector3 pos, Vector3 escala) {
		MeshInstance3D* m = memnew(MeshInstance3D);
		m->set_name(nombre);
		m->set_mesh(mesh);
		m->set_material_override(mat);
		m->set_position(pos);
		m->set_scale(escala);
		padre->add_child(m);
		return m;
	}

	Node3D* CrearProp(const String& nombre, int x, int y, bool solo_si_visible) {
		Node3D* prop = memnew(Node3D);
		prop->set_name(nombre);
		prop->set_position(GridToWorld(x, y));
		prop->set_meta("celda", Vector2i(x, y));
		prop->set_meta("solo_si_visible", solo_si_visible);
		prop->set_visible(false);
		m_props->add_child(prop);
		return prop;
	}

	void CrearAntorcha(int x, int y, Vector2i hacia) {
		Node3D* prop = CrearProp(NombreProp("Antorcha", x, y), x, y, false);

		Sprite3D* fuego = memnew(Sprite3D);
		fuego->set_name("Fuego");
		fuego->set_texture(m_atlas);
		fuego->set_region_enabled(true);
		fuego->set_region_rect(Rect2(0, 144, 16, 16));
		fuego->set_pixel_size(0.03f);
		fuego->set_billboard_mode(BaseMaterial3D::BILLBOARD_FIXED_Y);
		fuego->set_alpha_cut_mode(SpriteBase3D::ALPHA_CUT_DISCARD);
		fuego->set_texture_filter(BaseMaterial3D::TEXTURE_FILTER_NEAREST);
		fuego->set_draw_flag(SpriteBase3D::FLAG_SHADED, false);
		fuego->set_position(Vector3(hacia.x * 0.56f, 0.62f, hacia.y * 0.56f));
		prop->add_child(fuego);

		OmniLight3D* luz = memnew(OmniLight3D);
		luz->set_name("Luz");
		luz->set_color(Color(1.0, 0.55, 0.25));
		luz->set_param(Light3D::PARAM_ENERGY, 1.1);
		luz->set_param(Light3D::PARAM_RANGE, 3.2);
		luz->set_position(Vector3(hacia.x * 0.9f, 0.8f, hacia.y * 0.9f));
		luz->set_meta("energia_base", 1.1);
		luz->add_to_group("antorchas");
		prop->add_child(luz);
	}

	void ColocarAntorchas() {
		std::vector<Vector2i> puestas;
		const Vector2i dirs[4] = { Vector2i(0, 1), Vector2i(1, 0), Vector2i(0, -1), Vector2i(-1, 0) };
		int intentos = 0;
		while (puestas.size() < 10 && intentos < 4000) {
			intentos++;
			int x = UtilityFunctions::randi_range(1, grid - 2);
			int y = UtilityFunctions::randi_range(1, grid - 2);
			if (!EsMuro(x, y)) continue;

			Vector2i hacia(0, 0);
			int libres = 0;
			for (const Vector2i& d : dirs) {
				int nx = x + d.x, ny = y + d.y;
				if (nx >= 0 && ny >= 0 && nx < grid && ny < grid && tipos[nx * grid + ny] == T_SUELO) { hacia = d; libres++; }
			}
			if (libres != 1) continue;

			bool cerca = false;
			for (const Vector2i& p : puestas) {
				if (std::abs(p.x - x) + std::abs(p.y - y) < 7) { cerca = true; break; }
			}
			if (cerca) continue;

			puestas.push_back(Vector2i(x, y));
			CrearAntorcha(x, y, hacia);
		}
	}
}

JENOVA_SCRIPT_BEGIN

// ==========================================
// CONSTRUCCIÓN DEL MUNDO
// ==========================================

void Limpiar(Caller* instance) {
	// Los sacamos del árbol YA para que los nodos nuevos puedan reutilizar los mismos nombres
	Node* nodos[] = { m_mundo, m_props, m_alcance, m_ruta, m_cursor };
	for (Node* n : nodos) {
		if (!n) continue;
		if (n->get_parent()) n->get_parent()->remove_child(n);
		n->queue_free();
	}
	m_mundo = nullptr; m_props = nullptr; m_alcance = nullptr; m_ruta = nullptr; m_cursor = nullptr;
	m_mats_mundo.clear();
}

void ConstruirMundo(Caller* instance, PackedByteArray tipos_celdas, int grid_size) {
	self = GetSelf<Node3D>(instance);
	Limpiar(instance);

	grid = grid_size;
	tipos.assign(grid * grid, T_MURO);
	for (int i = 0; i < grid * grid && i < tipos_celdas.size(); i++) tipos[i] = tipos_celdas[i];
	niebla.assign(grid * grid, 0);

	if (m_shader.is_null()) m_shader = ResourceLoader::get_singleton()->load("res://Shaders/ps1_world.gdshader");
	if (m_atlas.is_null()) m_atlas = ResourceLoader::get_singleton()->load("res://Assets/2D Pixel Dungeon Asset Pack/character and tileset/Dungeon_Tileset.png");

	m_fog_img = Image::create_empty(grid, grid, false, Image::FORMAT_R8);
	m_fog_img->fill(Color(0, 0, 0));
	m_fog_tex = ImageTexture::create_from_image(m_fog_img);

	m_mat_suelo = CrearMaterialMundo(Tile(7, 1), Tile(7, 1), false, true);
	m_mat_muro = CrearMaterialMundo(Tile(2, 0), Tile(8, 7), true, false);
	m_mat_puerta = CrearMaterialMundo(Tile(8, 3), Tile(8, 7), false, false);
	m_mat_cofre = CrearMaterialMundo(Tile(3, 8), Tile(3, 8), false, false);
	m_mat_piedra = CrearMaterialMundo(Tile(2, 4), Tile(7, 2), false, false);

	m_mundo = memnew(Node3D);
	m_mundo->set_name("Mundo");
	self->add_child(m_mundo);

	m_props = memnew(Node3D);
	m_props->set_name("Props");
	self->add_child(m_props);

	// --- SUELOS (incluye la casilla bajo las puertas) ---
	const Vector2i variantes[] = {
		Vector2i(6, 0), Vector2i(7, 0), Vector2i(8, 0), Vector2i(9, 0),
		Vector2i(6, 1), Vector2i(7, 1), Vector2i(8, 1), Vector2i(9, 1),
		Vector2i(6, 2), Vector2i(7, 2), Vector2i(8, 2), Vector2i(9, 2),
		Vector2i(2, 2), Vector2i(3, 2)
	};
	std::vector<Vector2i> suelos, muros;
	for (int x = 0; x < grid; x++) {
		for (int y = 0; y < grid; y++) {
			if (!EsMuro(x, y)) { suelos.push_back(Vector2i(x, y)); continue; }
			// Solo dibujamos los muros que tocan suelo (el resto es vacío negro)
			bool toca_suelo = false;
			for (int dx = -1; dx <= 1 && !toca_suelo; dx++)
				for (int dy = -1; dy <= 1; dy++)
					if (!EsMuro(x + dx, y + dy)) { toca_suelo = true; break; }
			if (toca_suelo) muros.push_back(Vector2i(x, y));
		}
	}

	MultiMeshInstance3D* mm_suelo = CrearMultiMesh("Suelos", CrearBloque(0.0f, false), m_mat_suelo, true);
	Ref<MultiMesh> mms = mm_suelo->get_multimesh();
	mms->set_instance_count(suelos.size());
	for (size_t i = 0; i < suelos.size(); i++) {
		mms->set_instance_transform(i, Transform3D(Basis(), GridToWorld(suelos[i].x, suelos[i].y)));
		const Vector2i& var = variantes[UtilityFunctions::randi_range(0, 13)];
		mms->set_instance_custom_data(i, Color(var.x * TILE, var.y * TILE, 0, 0));
	}
	m_mundo->add_child(mm_suelo);

	MultiMeshInstance3D* mm_muro = CrearMultiMesh("Muros", CrearBloque(WALL_H), m_mat_muro, false);
	Ref<MultiMesh> mmm = mm_muro->get_multimesh();
	mmm->set_instance_count(muros.size());
	for (size_t i = 0; i < muros.size(); i++) {
		mmm->set_instance_transform(i, Transform3D(Basis(), GridToWorld(muros[i].x, muros[i].y)));
	}
	m_mundo->add_child(mm_muro);

	// --- PUERTAS ---
	Ref<ArrayMesh> bloque = CrearBloque(1.0f);
	for (int x = 0; x < grid; x++) {
		for (int y = 0; y < grid; y++) {
			uint8_t t = tipos[x * grid + y];
			if (t != T_PUERTA && t != T_PUERTA_ABIERTA) continue;
			Node3D* prop = CrearProp(NombreProp("Puerta", x, y), x, y, false);
			bool corta_en_x = EsMuro(x - 1, y) && EsMuro(x + 1, y); // pasillo vertical: la puerta se extiende en X
			Vector3 escala = corta_en_x ? Vector3(1.0, WALL_H * 0.96, 0.16) : Vector3(0.16, WALL_H * 0.96, 1.0);
			MeshInstance3D* hoja = CrearPieza(prop, "Hoja", bloque, m_mat_puerta, Vector3(), escala);
			if (t == T_PUERTA_ABIERTA) hoja->set_visible(false);
		}
	}

	// --- OVERLAYS (alcance, ruta y cursor) ---
	m_alcance = CrearMultiMesh("Alcance", CrearBloque(0.0f, false), CrearMaterialOverlay(Color(1.0, 0.72, 0.3, 0.13)), false);
	m_alcance->set_scale(Vector3(1, 1, 1));
	self->add_child(m_alcance);

	m_ruta = CrearMultiMesh("Ruta", CrearBloque(0.0f, false), CrearMaterialOverlay(Color(0.25, 1.0, 0.85, 0.85)), false);
	self->add_child(m_ruta);

	m_cursor = memnew(MeshInstance3D);
	m_cursor->set_name("Cursor");
	m_cursor->set_mesh(CrearBloque(0.0f, false));
	m_cursor->set_material_override(CrearMaterialOverlay(Color(1, 1, 1, 0.28)));
	m_cursor->set_visible(false);
	self->add_child(m_cursor);

	ColocarAntorchas();

	UtilityFunctions::print("[DungeonRenderer] Mundo 3D construido: ", (int)suelos.size(), " suelos, ", (int)muros.size(), " muros.");
}

// ==========================================
// PROPS INTERACTIVOS
// ==========================================

void CrearCofre(Caller* instance, int x, int y) {
	if (!m_props) return;
	Node3D* prop = CrearProp(NombreProp("Cofre", x, y), x, y, false);
	Ref<ArrayMesh> bloque = CrearBloque(1.0f);
	CrearPieza(prop, "Caja", bloque, m_mat_cofre, Vector3(0, 0, 0), Vector3(0.6, 0.36, 0.44));

	Node3D* bisagra = memnew(Node3D);
	bisagra->set_name("Bisagra");
	bisagra->set_position(Vector3(0, 0.36, -0.22));
	prop->add_child(bisagra);
	CrearPieza(bisagra, "Tapa", bloque, m_mat_cofre, Vector3(0, 0, 0.22), Vector3(0.64, 0.12, 0.48));
}

void AbrirCofre(Caller* instance, int x, int y) {
	if (!m_props) return;
	Node3D* bisagra = Object::cast_to<Node3D>(m_props->get_node_or_null(NodePath(NombreProp("Cofre", x, y) + "/Bisagra")));
	if (!bisagra) return;
	Ref<Tween> tw = bisagra->create_tween();
	tw->tween_property(bisagra, "rotation:x", -1.9, 0.35)->set_trans(Tween::TRANS_BACK)->set_ease(Tween::EASE_OUT);
}

void AbrirPuerta(Caller* instance, int x, int y) {
	if (!m_props) return;
	if (x >= 0 && y >= 0 && x < grid && y < grid) tipos[x * grid + y] = T_PUERTA_ABIERTA;
	Node3D* hoja = Object::cast_to<Node3D>(m_props->get_node_or_null(NodePath(NombreProp("Puerta", x, y) + "/Hoja")));
	if (!hoja) return;
	// Estilo rastrillo: la puerta se hunde en el suelo
	Ref<Tween> tw = hoja->create_tween();
	tw->tween_property(hoja, "position:y", -WALL_H * 1.05, 0.45)->set_trans(Tween::TRANS_QUAD)->set_ease(Tween::EASE_IN);
	tw->tween_callback(Callable(hoja, StringName("hide")));
}

void CrearSalida(Caller* instance, int x, int y) {
	if (!m_props) return;
	Node3D* prop = CrearProp("Salida", x, y, false);
	Ref<ArrayMesh> bloque = CrearBloque(1.0f);
	CrearPieza(prop, "Escalon1", bloque, m_mat_piedra, Vector3(0, 0, 0.3), Vector3(0.9, 0.15, 0.3));
	CrearPieza(prop, "Escalon2", bloque, m_mat_piedra, Vector3(0, 0, 0.0), Vector3(0.9, 0.3, 0.3));
	CrearPieza(prop, "Escalon3", bloque, m_mat_piedra, Vector3(0, 0, -0.3), Vector3(0.9, 0.45, 0.3));

	OmniLight3D* luz = memnew(OmniLight3D);
	luz->set_name("Luz");
	luz->set_color(Color(0.3, 0.55, 1.0));
	luz->set_param(Light3D::PARAM_ENERGY, 1.4);
	luz->set_param(Light3D::PARAM_RANGE, 2.8);
	luz->set_position(Vector3(0, 1.0, 0));
	prop->add_child(luz);
}

void CrearMarcadorEvento(Caller* instance, int x, int y) {
	if (!m_props) return;
	Node3D* prop = CrearProp(NombreProp("Evento", x, y), x, y, true);
	Ref<PrismMesh> gema;
	gema.instantiate();
	gema->set_size(Vector3(0.22, 0.3, 0.22));
	Ref<StandardMaterial3D> mat;
	mat.instantiate();
	mat->set_shading_mode(BaseMaterial3D::SHADING_MODE_UNSHADED);
	mat->set_albedo(Color(1.0, 0.2, 0.9));
	MeshInstance3D* m = CrearPieza(prop, "Gema", gema, mat, Vector3(0, 0.55, 0), Vector3(1, 1, 1));
	m->add_to_group("gemas_evento");
}

void QuitarMarcadorEvento(Caller* instance, int x, int y) {
	if (!m_props) return;
	Node* prop = m_props->get_node_or_null(NodePath(NombreProp("Evento", x, y)));
	if (prop) prop->queue_free();
}

// ==========================================
// NIEBLA Y OVERLAYS
// ==========================================

// estados: 0 = sin explorar, 1 = explorado, 2 = visible
void ActualizarNiebla(Caller* instance, PackedByteArray estados, int modo_debug) {
	if (m_fog_img.is_null() || grid == 0) return;
	niebla_debug = modo_debug != 0;
	for (int x = 0; x < grid; x++) {
		for (int y = 0; y < grid; y++) {
			int i = x * grid + y;
			uint8_t e = (i < estados.size()) ? estados[i] : 0;
			niebla[i] = e;
			float v = (e == 2) ? 1.0f : (e == 1 ? 0.5f : 0.0f);
			m_fog_img->set_pixel(x, y, Color(v, 0, 0));
		}
	}
	m_fog_tex->update(m_fog_img);

	if (!m_props) return;
	TypedArray<Node> props = m_props->get_children();
	for (int i = 0; i < props.size(); i++) {
		Node3D* prop = Object::cast_to<Node3D>(props[i]);
		if (!prop || !prop->has_meta("celda")) continue;
		Vector2i c = prop->get_meta("celda");
		uint8_t e = niebla[c.x * grid + c.y];
		bool solo_visible = (bool)prop->get_meta("solo_si_visible");
		prop->set_visible(solo_visible ? (e == 2 || niebla_debug) : (e >= 1));
	}
}

void PintarAlcance(Caller* instance, PackedInt32Array celdas) {
	RellenarOverlay(m_alcance, celdas, 0.012f);
	if (m_alcance) m_alcance->set_scale(Vector3(1, 1, 1));
}

void PintarRuta(Caller* instance, PackedInt32Array celdas) {
	RellenarOverlay(m_ruta, celdas, 0.025f);
	if (!m_ruta) return;
	// Puntos pequeños: escalamos cada instancia
	Ref<MultiMesh> mm = m_ruta->get_multimesh();
	for (int i = 0; i < mm->get_instance_count(); i++) {
		Transform3D t = mm->get_instance_transform(i);
		t.basis = Basis().scaled(Vector3(0.24, 1, 0.24));
		mm->set_instance_transform(i, t);
	}
}

void PintarCursor(Caller* instance, int x, int y) {
	if (!m_cursor) return;
	if (x < 0 || y < 0 || x >= grid || y >= grid) { m_cursor->set_visible(false); return; }
	Vector3 pos = GridToWorld(x, y);
	pos.y = 0.018f;
	m_cursor->set_position(pos);
	m_cursor->set_visible(true);
}

void LimpiarOverlay(Caller* instance) {
	PackedInt32Array vacio;
	RellenarOverlay(m_alcance, vacio, 0);
	RellenarOverlay(m_ruta, vacio, 0);
	if (m_cursor) m_cursor->set_visible(false);
}

// ==========================================
// CICLO DE VIDA
// ==========================================

void OnAwake(Caller* instance) {
	self = GetSelf<Node3D>(instance);
}

void OnProcess(Caller* instance, double delta) {
	if (!self) return;
	float t = Time::get_singleton()->get_ticks_msec() / 1000.0f;

	// Cutaway: los muros entre la cámara y el héroe activo se tramean
	Node* dungeon = self->get_parent();
	Camera3D* cam = dungeon ? Object::cast_to<Camera3D>(dungeon->get_node_or_null("Camera3D")) : nullptr;
	if (cam && m_mat_muro.is_valid()) {
		Vector3 foco = cam->has_meta("pivote") ? (Vector3)cam->get_meta("pivote") : Vector3();
		Node* pm = self->get_node_or_null("/root/Main/PartyManager");
		if (pm) {
			Node* act = Object::cast_to<Node>(pm->call("GetActiveCharacter"));
			if (act) {
				Node3D* token = Object::cast_to<Node3D>(dungeon->get_node_or_null(NodePath(String(act->get_name()))));
				if (token && token->is_visible()) foco = token->get_global_position();
			}
		}
		m_mat_muro->set_shader_parameter("focus_pos", foco);
		m_mat_muro->set_shader_parameter("cam_pos", cam->get_global_position());
	}

	// Parpadeo de antorchas (incluye la luz de los héroes)
	TypedArray<Node> luces = self->get_tree()->get_nodes_in_group("antorchas");
	for (int i = 0; i < luces.size(); i++) {
		OmniLight3D* luz = Object::cast_to<OmniLight3D>(luces[i]);
		if (!luz) continue;
		double base = luz->has_meta("energia_base") ? (double)luz->get_meta("energia_base") : 1.0;
		double f = 1.0 + 0.07 * std::sin(t * 11.0 + i * 1.7) + 0.05 * std::sin(t * 23.0 + i * 3.1);
		luz->set_param(Light3D::PARAM_ENERGY, base * f);
	}

	// Gemas de evento flotando
	TypedArray<Node> gemas = self->get_tree()->get_nodes_in_group("gemas_evento");
	for (int i = 0; i < gemas.size(); i++) {
		Node3D* g = Object::cast_to<Node3D>(gemas[i]);
		if (!g) continue;
		g->set_rotation(Vector3(0, t * 2.0f, 0));
		g->set_position(Vector3(0, 0.55f + 0.06f * std::sin(t * 3.0f + i), 0));
	}
}

void OnDestroy(Caller* instance) {
	self = nullptr;
	m_mundo = nullptr;
	m_props = nullptr;
	m_alcance = nullptr;
	m_ruta = nullptr;
	m_cursor = nullptr;
	m_mats_mundo.clear();
	m_mat_suelo.unref(); m_mat_muro.unref(); m_mat_puerta.unref(); m_mat_cofre.unref(); m_mat_piedra.unref();
	m_fog_tex.unref(); m_fog_img.unref(); m_shader.unref(); m_atlas.unref();
}

JENOVA_SCRIPT_END
