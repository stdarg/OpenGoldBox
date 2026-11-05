#include "game_classes.h"
#include "party_test_bridge.h"
#include <godot_cpp/core/class_db.hpp>
#include <godot_cpp/core/defs.hpp>
#include <godot_cpp/godot.hpp>

namespace
{
void initialize(godot::ModuleInitializationLevel level)
{
    if (level != godot::MODULE_INITIALIZATION_LEVEL_SCENE)
    {
        return;
    }
    register_game_classes();
    godot::ClassDB::register_class<PartyTestBridge>();
}

void terminate(godot::ModuleInitializationLevel)
{
}
} // namespace

extern "C" GDExtensionBool GDE_EXPORT party_tester_library_init(
    GDExtensionInterfaceGetProcAddress get_proc_address, GDExtensionClassLibraryPtr library,
    GDExtensionInitialization *initialization)
{
    godot::GDExtensionBinding::InitObject init(get_proc_address, library, initialization);
    init.register_initializer(initialize);
    init.register_terminator(terminate);
    init.set_minimum_library_initialization_level(godot::MODULE_INITIALIZATION_LEVEL_SCENE);
    return init.init();
}
