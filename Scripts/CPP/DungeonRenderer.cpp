/* Jenova C++ Node Base Script (Meteora) - DungeonRenderer (3D) */

// La "mano visual": no tiene lógica de juego. El DungeonManager le pasa la cuadrícula
// y le da órdenes (construir el mundo, pintar alcance/ruta, actualizar niebla, abrir cofres,
// mostrar bocadillos...).

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
#include <Godot/classes/canvas_layer.hpp>
#include <Godot/classes/control.hpp>
#include <Godot/classes/panel_container.hpp>
#include <Godot/classes/label.hpp>
#include <Godot/classes/progress_bar.hpp>
#include <Godot/classes/polygon2d.hpp>
#include <Godot/classes/style_box_flat.hpp>
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

	// Tipos de overlay de alcance
	const int O_ANDAR = 0;
	const int O_INTERACTUAR = 1;
	const int O_ENEMIGO = 2;

	// Estilos de bocadillo
	const int B_NORMAL = 0;
	const int B_SIN_PA = 1;
	const int B_HOVER = 2;

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
	MeshInstance3D* m_marcador = nullptr;

	// Capa 2D dentro del SubViewport: elementos anclados a posiciones 3D (se pixelan como el mundo)
	CanvasLayer* m_capa2d = nullptr;
	Control* m_raiz2d = nullptr;
	Control* m_coste = nullptr;

	Ref<Shader> m_shader;
	Ref<Shader> m_shader_overlay;
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

	Node* Dungeon() { return self ? self->get_parent() : nullptr; }

	Camera3D* Camara() {
		Node* d = Dungeon();
		return d ? Object::cast_to<Camera3D>(d->get_node_or_null("Camera3D")) : nullptr;
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

	Ref<StandardMaterial3D> CrearMaterialOverlay(Color color, bool color_instancia = false) {
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

	MultiMeshInstance3D* CrearMultiMesh(const String& nombre, Ref<Mesh> mesh, Ref<Material> mat, bool custom_data, bool colores = false) {
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

	// ==========================================
	// CAPA 2D ANCLADA AL MUNDO
	// ==========================================

	Ref<StyleBoxFlat> EstiloGlobo(int estilo) {
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

	Polygon2D* Triangulo(const String& nombre, Vector2 a, Vector2 b, Vector2 c, Color color) {
		Polygon2D* p = memnew(Polygon2D);
		p->set_name(nombre);
		PackedVector2Array pts;
		pts.push_back(a); pts.push_back(b); pts.push_back(c);
		p->set_polygon(pts);
		p->set_color(color);
		return p;
	}

	// wrapper (posición + zoom) > Escala (animación de aparición) > Cola + Globo > Texto
	Control* CrearBocadillo(const String& nombre, const String& texto, int estilo) {
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

	float EscalaZoom(Camera3D* cam) {
		float z = cam && cam->has_meta("zoom_factor") ? (float)cam->get_meta("zoom_factor") : 2.0f;
		return UtilityFunctions::clampf(z * 0.5f, 0.7f, 1.4f);
	}

	// Coloca un elemento 2D sobre un punto 3D; lo oculta si queda detrás de la cámara
	void Anclar(Control* c, Camera3D* cam, Vector3 mundo, float escala) {
		if (!c || !cam) return;
		if (cam->is_position_behind(mundo)) { c->set_visible(false); return; }
		c->set_position(cam->unproject_position(mundo).round());
		c->set_scale(Vector2(escala, escala));
	}

	void ColocarGlobo(Control* wrapper) {
		PanelContainer* globo = Object::cast_to<PanelContainer>(wrapper->get_node_or_null("Escala/Globo"));
		if (!globo) return;
		Vector2 sz = globo->get_combined_minimum_size();
		globo->set_size(sz);
		globo->set_position(Vector2(std::round(-sz.x * 0.5f), std::round(-sz.y - 5.0f)));
	}

	Control* CrearUIEnemigo(const String& nombre) {
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
	int Pareja(int f) {
		switch (f) {
			case 0: return 1; case 1: return 0; case 2: return 3; case 3: return 2;
			case 4: return 5; case 5: return 4; case 6: return 5;
			case 9: return 10; case 10: return 9; case 11: return 12; case 12: return 11; case 13: return 12;
			default: return f;
		}
	}

	void CrearCapa2D() {
		if (m_capa2d) return;
		m_capa2d = memnew(CanvasLayer);
		m_capa2d->set_name("Capa2D");
		m_capa2d->set_layer(2);
		self->add_child(m_capa2d);

		m_raiz2d = memnew(Control);
		m_raiz2d->set_name("Raiz");
		m_raiz2d->set_mouse_filter(Control::MOUSE_FILTER_IGNORE);
		m_capa2d->add_child(m_raiz2d);

		Label* coste = memnew(Label);
		coste->set_name("Coste");
		coste->add_theme_font_size_override("font_size", 8);
		coste->add_theme_color_override("font_outline_color", Color(0.02, 0.02, 0.05));
		coste->add_theme_constant_override("outline_size", 3);
		coste->set_mouse_filter(Control::MOUSE_FILTER_IGNORE);
		coste->set_visible(false);
		m_raiz2d->add_child(coste);
		m_coste = coste;
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

	if (m_raiz2d) {
		TypedArray<Node> hijos = m_raiz2d->get_children();
		for (int i = 0; i < hijos.size(); i++) {
			Node* h = Object::cast_to<Node>(hijos[i]);
			if (h && h != m_coste) { m_raiz2d->remove_child(h); h->queue_free(); }
		}
	}
	if (m_coste) m_coste->set_visible(false);
}

void ConstruirMundo(Caller* instance, PackedByteArray tipos_celdas, int grid_size) {
	self = GetSelf<Node3D>(instance);
	Limpiar(instance);
	CrearCapa2D();

	grid = grid_size;
	tipos.assign(grid * grid, T_MURO);
	for (int i = 0; i < grid * grid && i < tipos_celdas.size(); i++) tipos[i] = tipos_celdas[i];
	niebla.assign(grid * grid, 0);

	if (m_shader.is_null()) m_shader = ResourceLoader::get_singleton()->load("res://Shaders/ps1_world.gdshader");
	if (m_shader_overlay.is_null()) m_shader_overlay = ResourceLoader::get_singleton()->load("res://Shaders/ps1_overlay.gdshader");
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
	Ref<ShaderMaterial> mat_alcance;
	mat_alcance.instantiate();
	mat_alcance->set_shader(m_shader_overlay);
	m_alcance = CrearMultiMesh("Alcance", CrearBloque(0.0f, false), mat_alcance, true);
	self->add_child(m_alcance);

	m_ruta = CrearMultiMesh("Ruta", CrearBloque(0.0f, false), CrearMaterialOverlay(Color(1, 1, 1, 1), true), false, true);
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

// Ternas (x, y, tipo): 0 = andar, 1 = interactuable, 2 = enemigo.
// Las casillas de andar solo dibujan borde en los lados que dan fuera del área.
void PintarAlcance(Caller* instance, PackedInt32Array ternas) {
	if (!m_alcance || grid == 0) return;
	int count = ternas.size() / 3;
	std::vector<uint8_t> andar(grid * grid, 0);
	for (int i = 0; i < count; i++) {
		int x = ternas[i * 3], y = ternas[i * 3 + 1];
		if (ternas[i * 3 + 2] == O_ANDAR && x >= 0 && y >= 0 && x < grid && y < grid) andar[x * grid + y] = 1;
	}
	auto es_andar = [&](int x, int y) { return x >= 0 && y >= 0 && x < grid && y < grid && andar[x * grid + y]; };

	Ref<MultiMesh> mm = m_alcance->get_multimesh();
	mm->set_instance_count(count);
	for (int i = 0; i < count; i++) {
		int x = ternas[i * 3], y = ternas[i * 3 + 1], tipo = ternas[i * 3 + 2];
		Vector3 pos = GridToWorld(x, y);
		pos.y = 0.012f;
		mm->set_instance_transform(i, Transform3D(Basis(), pos));

		int mascara = 15;
		Color color(0.35, 0.72, 1.0);
		if (tipo == O_ANDAR) {
			mascara = (es_andar(x, y - 1) ? 0 : 1) | (es_andar(x, y + 1) ? 0 : 2) | (es_andar(x - 1, y) ? 0 : 4) | (es_andar(x + 1, y) ? 0 : 8);
		} else if (tipo == O_INTERACTUAR) {
			color = Color(1.0, 0.8, 0.25);
		} else {
			color = Color(1.0, 0.28, 0.22);
		}
		color.a = mascara / 15.0f;
		mm->set_instance_custom_data(i, color);
	}
}

// Ruta: las primeras 'alcanzables' casillas en color, el resto (sin PA) en gris
void PintarRuta(Caller* instance, PackedInt32Array celdas, int alcanzables) {
	if (!m_ruta) return;
	Ref<MultiMesh> mm = m_ruta->get_multimesh();
	int count = celdas.size() / 2;
	mm->set_instance_count(count);
	int ultima = std::min(alcanzables, count) - 1;
	for (int i = 0; i < count; i++) {
		Vector3 pos = GridToWorld(celdas[i * 2], celdas[i * 2 + 1]);
		pos.y = 0.025f;
		bool es_final = (i == ultima);
		float s = es_final ? 0.38f : 0.2f;
		Basis b = Basis().rotated(Vector3(0, 1, 0), es_final ? (float)Math_PI * 0.25f : 0.0f).scaled(Vector3(s, 1, s));
		mm->set_instance_transform(i, Transform3D(b, pos));
		Color c = i < alcanzables ? Color(0.75, 0.95, 1.0, 0.95) : Color(0.45, 0.45, 0.5, 0.7);
		mm->set_instance_color(i, c);
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
	if (m_alcance) m_alcance->get_multimesh()->set_instance_count(0);
	if (m_ruta) m_ruta->get_multimesh()->set_instance_count(0);
	if (m_cursor) m_cursor->set_visible(false);
	if (m_coste) m_coste->set_visible(false);
}

// ==========================================
// BOCADILLOS Y COSTE (capa 2D anclada al mundo)
// ==========================================

// canal 0 = interactuables adyacentes, canal 1 = hover. Solo se recrean los que cambian.
void FijarBocadillos(Caller* instance, int canal, PackedInt32Array celdas, PackedStringArray textos, PackedInt32Array estilos) {
	if (!m_raiz2d) return;
	String prefijo = "Bocadillo" + String::num_int64(canal);

	std::vector<String> deseados;
	for (int i = 0; i * 2 + 1 < celdas.size() && i < textos.size(); i++) {
		String nombre = NombreProp(prefijo, celdas[i * 2], celdas[i * 2 + 1]);
		deseados.push_back(nombre);
		int estilo = i < estilos.size() ? estilos[i] : B_NORMAL;

		Control* existente = Object::cast_to<Control>(m_raiz2d->get_node_or_null(NodePath(nombre)));
		if (existente && (String)existente->get_meta("texto") == textos[i] && (int)existente->get_meta("estilo") == estilo) continue;
		if (existente) { m_raiz2d->remove_child(existente); existente->queue_free(); }

		Control* b = CrearBocadillo(nombre, textos[i], estilo);
		b->set_meta("anclaje", GridToWorld(celdas[i * 2], celdas[i * 2 + 1]) + Vector3(0, 0.95f, 0));
		b->set_visible(false); // se muestra al anclarse en el siguiente frame
		m_raiz2d->add_child(b);
	}

	TypedArray<Node> hijos = m_raiz2d->get_children();
	for (int i = 0; i < hijos.size(); i++) {
		Node* h = Object::cast_to<Node>(hijos[i]);
		if (!h || !String(h->get_name()).begins_with(prefijo + "_")) continue;
		bool sigue = false;
		for (const String& d : deseados) if (d == String(h->get_name())) { sigue = true; break; }
		if (!sigue) { m_raiz2d->remove_child(h); h->queue_free(); }
	}
}

// Texto pequeño junto a la casilla bajo el cursor ("3 PA"). x < 0 lo oculta. estilo 1 = sin PA.
void MostrarCoste(Caller* instance, int x, int y, String texto, int estilo) {
	if (!m_coste) return;
	Label* l = Object::cast_to<Label>(m_coste);
	if (x < 0 || texto.is_empty()) { m_coste->set_visible(false); return; }
	l->set_text(texto);
	l->add_theme_color_override("font_color", estilo == B_SIN_PA ? Color(1.0, 0.45, 0.4) : Color(0.85, 0.97, 1.0));
	m_coste->set_meta("anclaje", GridToWorld(x, y) + Vector3(0.35f, 0.05f, 0.0f));
	m_coste->set_visible(true);
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
	Node* dungeon = Dungeon();
	Camera3D* cam = Camara();
	if (!dungeon) return;

	// --- Héroe activo ---
	Node3D* token_activo = nullptr;
	Node* pm = self->get_node_or_null("/root/Main/PartyManager");
	if (pm) {
		Node* act = Object::cast_to<Node>(pm->call("GetActiveCharacter"));
		if (act) token_activo = Object::cast_to<Node3D>(dungeon->get_node_or_null(NodePath(String(act->get_name()))));
	}

	// Cutaway: los muros entre la cámara y el héroe activo se tramean
	if (cam && m_mat_muro.is_valid()) {
		Vector3 foco = cam->has_meta("pivote") ? (Vector3)cam->get_meta("pivote") : Vector3();
		if (token_activo && token_activo->is_visible()) foco = token_activo->get_global_position();
		m_mat_muro->set_shader_parameter("focus_pos", foco);
		m_mat_muro->set_shader_parameter("cam_pos", cam->get_global_position());
	}

	// Marcador del héroe activo: rombo dorado girando sobre su cabeza
	if (!m_marcador) {
		Ref<PrismMesh> prisma;
		prisma.instantiate();
		prisma->set_size(Vector3(0.16, 0.2, 0.16));
		Ref<StandardMaterial3D> mat;
		mat.instantiate();
		mat->set_shading_mode(BaseMaterial3D::SHADING_MODE_UNSHADED);
		mat->set_albedo(Color(1.0, 0.82, 0.25));
		mat->set_flag(BaseMaterial3D::FLAG_DISABLE_FOG, true);
		m_marcador = memnew(MeshInstance3D);
		m_marcador->set_name("MarcadorActivo");
		m_marcador->set_mesh(prisma);
		m_marcador->set_material_override(mat);
		self->add_child(m_marcador);
	}
	bool bloqueado = dungeon->has_meta("bloqueo_evento") && (bool)dungeon->get_meta("bloqueo_evento");
	if (token_activo && token_activo->is_visible() && !bloqueado) {
		m_marcador->set_visible(true);
		m_marcador->set_position(token_activo->get_position() + Vector3(0, 1.18f + 0.05f * std::sin(t * 4.0f), 0));
		m_marcador->set_rotation(Vector3((float)Math_PI, t * 2.5f, 0));
	} else {
		m_marcador->set_visible(false);
	}

	// Idle: los sprites alternan sus dos frames; y UI 2D de los enemigos
	float escala = EscalaZoom(cam);
	TypedArray<Node> hijos = dungeon->get_children();
	std::vector<String> uis_vivas;
	for (int i = 0; i < hijos.size(); i++) {
		Node3D* ficha = Object::cast_to<Node3D>(hijos[i]);
		if (!ficha) continue;
		Sprite3D* cuerpo = Object::cast_to<Sprite3D>(ficha->get_node_or_null("Cuerpo"));
		if (!cuerpo) continue;

		if (!cuerpo->has_meta("frame_a")) cuerpo->set_meta("frame_a", cuerpo->get_frame());
		int fa = cuerpo->get_meta("frame_a");
		int fase = (int)(t * 1.8f + (ficha->get_instance_id() % 7) * 0.37f) % 2;
		cuerpo->set_frame(fase == 0 ? fa : Pareja(fa));

		String nombre = ficha->get_name();
		if (!m_raiz2d || !nombre.begins_with("Enemy_") || nombre.ends_with("_muerto")) continue;

		String nombre_ui = "UI_" + nombre;
		uis_vivas.push_back(nombre_ui);
		Control* ui = Object::cast_to<Control>(m_raiz2d->get_node_or_null(NodePath(nombre_ui)));
		if (!ui) { ui = CrearUIEnemigo(nombre_ui); m_raiz2d->add_child(ui); }

		bool visible = ficha->is_visible();
		ui->set_visible(visible);
		if (!visible || !cam) continue;
		Anclar(ui, cam, ficha->get_position() + Vector3(0, 0.95f, 0), escala);

		bool alerta = ficha->has_meta("in_combat") && (bool)ficha->get_meta("in_combat");
		Control* signo = Object::cast_to<Control>(ui->get_node_or_null("Alerta"));
		if (signo) {
			if (alerta && !signo->is_visible()) {
				signo->set_visible(true);
				signo->set_pivot_offset(Vector2(3, 10));
				signo->set_scale(Vector2(0.2, 2.0));
				Ref<Tween> tw = signo->create_tween();
				tw->tween_property(signo, "scale", Vector2(1, 1), 0.25)->set_trans(Tween::TRANS_ELASTIC)->set_ease(Tween::EASE_OUT);
			}
			if (!alerta) signo->set_visible(false);
			signo->set_position(Vector2(-3, -30 + 2.0f * std::sin(t * 6.0f)));
		}
		ProgressBar* vida = Object::cast_to<ProgressBar>(ui->get_node_or_null("Vida"));
		if (vida) {
			int hp = ficha->get_meta("hp_actual", 1);
			int hp_max = ficha->get_meta("hp_max", 1);
			vida->set_max(hp_max);
			vida->set_value(hp);
			vida->set_visible(hp < hp_max);
		}
	}

	// Bocadillos y coste: recolocar sobre su punto 3D; quitar la UI de enemigos que ya no existen
	if (m_raiz2d && cam) {
		TypedArray<Node> elems = m_raiz2d->get_children();
		for (int i = 0; i < elems.size(); i++) {
			Control* c = Object::cast_to<Control>(elems[i]);
			if (!c) continue;
			String n = c->get_name();
			if (n.begins_with("UI_")) {
				bool viva = false;
				for (const String& u : uis_vivas) if (u == n) { viva = true; break; }
				if (!viva) { m_raiz2d->remove_child(c); c->queue_free(); }
				continue;
			}
			if (!c->has_meta("anclaje")) continue;
			if (c == m_coste && !c->is_visible()) continue;
			c->set_visible(true);
			Anclar(c, cam, c->get_meta("anclaje"), escala);
			if (n.begins_with("Bocadillo")) ColocarGlobo(c);
		}
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
	m_marcador = nullptr;
	m_capa2d = nullptr;
	m_raiz2d = nullptr;
	m_coste = nullptr;
	m_mats_mundo.clear();
	m_mat_suelo.unref(); m_mat_muro.unref(); m_mat_puerta.unref(); m_mat_cofre.unref(); m_mat_piedra.unref();
	m_fog_tex.unref(); m_fog_img.unref(); m_shader.unref(); m_shader_overlay.unref(); m_atlas.unref();
}

JENOVA_SCRIPT_END
