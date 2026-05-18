#include <Godot/godot.hpp>
#include <Godot/classes/node2d.hpp>
#include <Godot/classes/tween.hpp>
#include <Godot/classes/property_tweener.hpp>
#include <Godot/variant/utility_functions.hpp>

using namespace godot;
using namespace jenova::sdk;

JENOVA_SCRIPT_BEGIN

void OnAwake(Caller* instance) {
	UtilityFunctions::print("[Ficha] Despierta y lista para recibir ordenes.");
}

void MoveToPixel(Caller* instance, double target_x, double target_y) {
	Node2D* me = GetSelf<Node2D>(instance);
	
	if (!me) {
		UtilityFunctions::print("[Ficha ERROR] No encuentro mi cuerpo.");
		return; 
	}
	
	// RECONSTRUIMOS EL VECTOR2 AL LLEGAR
	Vector2 target_pixel_pos = Vector2(target_x, target_y);
	
	UtilityFunctions::print("[Ficha] Recibido! Deslizandome a X: ", target_pixel_pos.x, " Y: ", target_pixel_pos.y);
	
	// Encendemos el motor de animación otra vez
	Ref<Tween> tween = me->create_tween();
	if (tween.is_valid()) {
		tween->tween_property(me, "position", target_pixel_pos, 0.3)->set_trans(Tween::TRANS_SINE)->set_ease(Tween::EASE_OUT);
	} else {
		me->set_position(target_pixel_pos);
	}
}

JENOVA_SCRIPT_END
