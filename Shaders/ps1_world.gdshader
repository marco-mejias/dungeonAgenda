// PS1-style world shader: vertex snapping, affine texture mapping, per-vertex lighting, per-cell
// fog of war, per-pixel line of sight (cone-shaped black shadows behind walls, columns and closed
// doors) and screen-door cutaway for walls that hide the active hero.
shader_type spatial;
render_mode vertex_lighting, specular_disabled, cull_disabled, depth_draw_opaque;

// Atlas (Dungeon_Tileset.png) and the tile used by side and top faces: xy = uv offset, zw = uv size
uniform sampler2D atlas : source_color, filter_nearest, repeat_disable;
uniform vec4 tile_side = vec4(0.2, 0.0, 0.1, 0.1);
uniform vec4 tile_top = vec4(0.8, 0.7, 0.1, 0.1);
uniform bool use_instance_tile = false; // MultiMesh custom data overrides tile_top (floor variants)
uniform vec4 tint : source_color = vec4(1.0);

// Fog of war: R8 texture, one texel per cell (0 = unexplored, ~0.5 = explored, 1 = visible)
uniform bool use_fog = true;
uniform sampler2D fog_tex : filter_nearest, repeat_disable;
uniform float grid_size = 35.0;

// Per-pixel line of sight: whatever no hero sees stays black, with cone shadows behind the walls.
// occ_tex: 1 = blocks sight (wall, column, closed door). vistaN = (x, z, radius, active)
uniform sampler2D occ_tex : filter_nearest, repeat_disable;
uniform vec4 vista0 = vec4(0.0);
uniform vec4 vista1 = vec4(0.0);
uniform vec4 vista2 = vec4(0.0);
uniform vec4 vista3 = vec4(0.0);
uniform vec4 luces[8];  // Braziers in view (x, z, radius, active): they light like a hero
uniform bool ver_todo = false;   // Debug mode
uniform bool es_muro = false;    // Walls and columns: their own tile does block (back faces)

// Cutaway: walls between the camera and the focused hero are dithered out
uniform bool use_cutaway = false;
uniform vec3 focus_pos = vec3(0.0);
uniform vec3 cam_pos = vec3(0.0, 10.0, 10.0);

// Vertex jitter strength in low-res pixels (pause menu option)
global uniform float ps1_jitter;

varying vec3 aff_uv;
varying vec3 wpos;
varying flat float fog_state;
varying flat float cut;
varying flat float is_top;
varying flat vec2 inst_tile;

const float BAYER[16] = float[](0.0, 8.0, 2.0, 10.0, 12.0, 4.0, 14.0, 6.0, 3.0, 11.0, 1.0, 9.0, 15.0, 7.0, 13.0, 5.0);

void vertex() {
	vec3 origin = MODEL_MATRIX[3].xyz;
	vec2 cell = floor(origin.xz);

	fog_state = 1.0;
	if (use_fog) {
		ivec2 c = ivec2(clamp(cell, vec2(0.0), vec2(grid_size - 1.0)));
		fog_state = texelFetch(fog_tex, c, 0).r;
	}

	cut = 0.0;
	if (use_cutaway) {
		vec2 to_focus = normalize((focus_pos - cam_pos).xz);
		vec2 rel = origin.xz - focus_pos.xz;
		float along = dot(rel, -to_focus);
		float perp = length(rel + to_focus * along);
		if (along > 0.3 && along < 6.0 && perp < 1.7) {
			cut = 1.0;
		}
	}

	is_top = UV2.x;
	wpos = (MODEL_MATRIX * vec4(VERTEX, 1.0)).xyz;
	inst_tile = INSTANCE_CUSTOM.xy;

	vec4 clip = PROJECTION_MATRIX * (MODELVIEW_MATRIX * vec4(VERTEX, 1.0));
	vec2 snap = VIEWPORT_SIZE / (2.0 * max(ps1_jitter, 0.01));
	clip.xy = floor((clip.xy / clip.w) * snap + 0.5) / snap * clip.w;
	POSITION = clip;
	aff_uv = vec3(UV * clip.w, clip.w);
}

// Does tile c block sight? (outside the map counts as a wall)
bool tapa(ivec2 c) {
	if (c.x < 0 || c.y < 0 || float(c.x) >= grid_size || float(c.y) >= grid_size) return true;
	return texelFetch(occ_tex, c, 0).r > 0.5;
}

// Does a ray from 'o' (inside the hero's tile) reach point p? It walks exactly the tiles it crosses
// (DDA), with no fixed steps: the shadow edge comes out straight, without teeth.
bool rayo(vec2 o, vec2 p) {
	vec2 d = o - p;
	float largo = length(d);
	if (largo < 0.05) return true;
	vec2 dir = d / largo;
	ivec2 propia = ivec2(floor(p));
	ivec2 destino = ivec2(floor(o));
	vec2 q = p + dir * 0.03; // A hair towards the hero (wall faces sit right on the edge)
	ivec2 c = ivec2(floor(q));
	if (c != propia && c != destino && tapa(c)) return false;
	ivec2 paso = ivec2(sign(dir));
	vec2 inv = 1.0 / max(abs(dir), vec2(1e-5));
	vec2 t_max = vec2(
		(dir.x > 0.0 ? float(c.x + 1) - q.x : q.x - float(c.x)) * inv.x,
		(dir.y > 0.0 ? float(c.y + 1) - q.y : q.y - float(c.y)) * inv.y);
	for (int i = 0; i < 64; i++) {
		float t;
		if (t_max.x < t_max.y) { c.x += paso.x; t = t_max.x; t_max.x += inv.x; }
		else { c.y += paso.y; t = t_max.y; t_max.y += inv.y; }
		if (t > largo || c == destino) break;
		if (c != propia && tapa(c)) return false;
	}
	return true;
}

// How much hero 'v' sees point p (0..1). Several rays are cast from points of their tile around
// them and averaged: the shadow edge is a soft penumbra.
const int RAYOS = 6;
const float RADIO_PENUMBRA = 0.16;
// How much light source v (x, z, radius, active) sees point p
float ve(vec4 v, vec2 p) {
	if (v.w < 0.5) return 0.0;
	if (length(v.xy - p) > v.z) return 0.0;
	ivec2 propia = ivec2(floor(p));
	// On a wall, if we're still inside when moving closer to the hero, it's a back face: in shadow
	if (es_muro && tapa(propia) && ivec2(floor(p + normalize(v.xy - p) * 0.03)) == propia) return 0.0;
	vec2 celda = floor(v.xy);
	float suma = 0.0;
	for (int i = 0; i < RAYOS; i++) {
		float a = 6.2831853 * float(i) / float(RAYOS);
		vec2 o = clamp(v.xy + vec2(cos(a), sin(a)) * RADIO_PENUMBRA, celda + 0.02, celda + 0.98);
		suma += rayo(o, p) ? 1.0 : 0.0;
	}
	return suma / float(RAYOS);
}

void fragment() {
	ivec2 p = ivec2(FRAGCOORD.xy) % 4;
	float threshold = (BAYER[p.y * 4 + p.x] + 0.5) / 16.0;
	if (cut > 0.5 && threshold < 0.75) {
		discard;
	}

	vec2 uv = clamp(aff_uv.xy / aff_uv.z, vec2(0.001), vec2(0.999));
	vec4 tile = is_top > 0.5 ? tile_top : tile_side;
	if (use_instance_tile && is_top > 0.5) {
		tile.xy = inst_tile;
	}
	vec4 col = texture(atlas, tile.xy + uv * tile.zw) * tint;
	if (col.a < 0.5) {
		discard;
	}

	vec3 rgb = col.rgb;
	if (!use_fog || ver_todo) {
		ALBEDO = rgb;
	} else {
		// Whatever some hero sees right now, pixel by pixel (the shadows come out cone-shaped)
		float visto;
		if (es_muro && is_top > 0.5) {
			visto = fog_state > 0.99 ? 1.0 : 0.0; // The top of a wall: whatever its tile says
		} else {
			visto = max(max(ve(vista0, wpos.xz), ve(vista1, wpos.xz)), max(ve(vista2, wpos.xz), ve(vista3, wpos.xz)));
			for (int i = 0; i < 8 && visto < 1.0; i++) {
				visto = max(visto, ve(luces[i], wpos.xz));
			}
		}
		float g = dot(rgb, vec3(0.3, 0.59, 0.11));
		// Explored but out of view: almost black and desaturated (you can make out the shape);
		// unexplored: black
		vec3 apagado = mix(vec3(g), rgb, 0.3) * vec3(0.2, 0.2, 0.27) * clamp(fog_state * 2.0, 0.0, 1.0);
		ALBEDO = mix(apagado, rgb, visto);
	}
}
