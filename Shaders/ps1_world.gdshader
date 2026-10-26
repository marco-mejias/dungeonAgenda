// PS1-style world shader: vertex snapping, affine texture mapping, per-vertex lighting,
// per-cell fog of war and screen-door cutaway for walls that hide the active hero.
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

// Cutaway: walls between the camera and the focused hero are dithered out
uniform bool use_cutaway = false;
uniform vec3 focus_pos = vec3(0.0);
uniform vec3 cam_pos = vec3(0.0, 10.0, 10.0);

// Vertex snapping strength, in low-res pixels
uniform float jitter = 1.5;

varying vec3 aff_uv;
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
	inst_tile = INSTANCE_CUSTOM.xy;

	vec4 clip = PROJECTION_MATRIX * (MODELVIEW_MATRIX * vec4(VERTEX, 1.0));
	vec2 snap = VIEWPORT_SIZE / (2.0 * max(jitter, 0.01));
	clip.xy = floor((clip.xy / clip.w) * snap + 0.5) / snap * clip.w;
	POSITION = clip;
	aff_uv = vec3(UV * clip.w, clip.w);
}

void fragment() {
	if (fog_state < 0.2) {
		discard;
	}
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
	if (fog_state < 0.75) {
		// Explored but not currently seen: dark and desaturated
		float g = dot(rgb, vec3(0.3, 0.59, 0.11));
		rgb = mix(vec3(g), rgb, 0.35) * vec3(0.32, 0.32, 0.42);
	}
	ALBEDO = rgb;
}
