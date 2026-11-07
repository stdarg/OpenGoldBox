// Loaded into the checks' copy of Godot (cmake/godot_out_of_dock.cmake), it
// keeps Godot out of the Dock: every activation policy Godot asks for becomes
// Accessory, and Carbon's request to become a Dock app makes it a UI element.
// Its windows still open; only the Dock icon is gone.
#include <objc/message.h>
#include <objc/runtime.h>

typedef signed char BOOL_T;
static IMP original;

static BOOL_T accessory(id self, SEL command, long policy)
{
    (void)policy;
    return ((BOOL_T(*)(id, SEL, long))original)(self, command, 1);
}

__attribute__((constructor)) static void keep_out_of_dock(void)
{
    Class application = objc_getClass("NSApplication");
    if (!application)
    {
        return;
    }
    Method method = class_getInstanceMethod(application, sel_registerName("setActivationPolicy:"));
    if (method)
    {
        original = method_setImplementation(method, (IMP)accessory);
    }
}

// Carbon's way to become a Dock app; asked for, it makes the app a UI element
// instead (kProcessTransformToUIElementApplication).
#include <stdint.h>
typedef struct ProcessSerialNumber
{
    uint32_t high, low;
} ProcessSerialNumber;
extern int32_t TransformProcessType(const ProcessSerialNumber *psn, uint32_t type);

static int32_t keep_ui_element(const ProcessSerialNumber *psn, uint32_t type)
{
    (void)type;
    return TransformProcessType(psn, 4);
}

__attribute__((used, section("__DATA,__interpose"))) static struct
{
    const void *replacement, *replaced;
} interpose_transform = {(const void *)keep_ui_element, (const void *)TransformProcessType};
