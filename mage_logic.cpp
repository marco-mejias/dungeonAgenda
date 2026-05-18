#include <Godot/godot.hpp>
#include <Godot/classes/node.hpp>

using namespace godot;
using namespace jenova::sdk;

JENOVA_SCRIPT_BEGIN

void OnReady(Caller* instance) {
	Node* self = GetSelf<Node>(instance);
	if (!self) return;

	// Guardamos los datos de forma segura en los metadatos
	self->set_meta("nombre_display", "Vivi Owl-nitier");
	self->set_meta("fuerza", 12);
	self->set_meta("hp", 10);
	self->set_meta("magia", 1);
	self->set_meta("suerte", 5);
	self->set_meta("velocidad", 2);

	// Estado dinámico
	self->set_meta("ha_movido", false);
	self->set_meta("grid_pos", Vector2i(0, 0));
	self->set_meta("retrato_path", "res://assets/vivi-ornitier.png");
}

JENOVA_SCRIPT_END
