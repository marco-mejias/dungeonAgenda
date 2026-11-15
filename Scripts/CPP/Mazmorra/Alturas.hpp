#pragma once
// Floor heights: every tile has a level (0 = lower floor, 1 = step, 2 = high room). Mazmorra
// publishes them when generating the floor in an Engine meta ("alturas"), so any script (render,
// movement, AI, mouse, combat) knows the height of a tile without having access to the Mazmorra.

#include <Godot/godot.hpp>
#include <Godot/classes/engine.hpp>
#include <Godot/variant/packed_byte_array.hpp>

namespace alturas {

using namespace godot;

const float PASO = 0.2f; // Metres per level: step 0.2, high room 0.4
const int ESCALON = 1;
const int ALTO = 2;

// Mazmorra publishes the heights of the new floor
inline void Publicar(const PackedByteArray& niveles, int lado) {
	Engine* e = Engine::get_singleton();
	if (!e) return;
	e->set_meta("alturas", niveles);
	e->set_meta("alturas_lado", lado);
}

// Height level of a tile (0 if there's no floor published yet)
inline int Nivel(int x, int y) {
	Engine* e = Engine::get_singleton();
	if (!e || !e->has_meta("alturas")) return 0;
	int lado = e->get_meta("alturas_lado", 0);
	if (x < 0 || y < 0 || x >= lado || y >= lado) return 0;
	PackedByteArray a = e->get_meta("alturas");
	int i = x * lado + y;
	return i < a.size() ? (int)a[i] : 0;
}

inline int Nivel(Vector2i c) { return Nivel(c.x, c.y); }

// World height of a tile's floor
inline float Y(int x, int y) { return Nivel(x, y) * PASO; }

inline bool EsAlto(Vector2i c) { return Nivel(c) == ALTO; }

} // namespace alturas
