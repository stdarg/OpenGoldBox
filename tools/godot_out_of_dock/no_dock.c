// Loaded into the checks' copy of Godot (cmake/godot_out_of_dock.cmake), it
// keeps the checks out of the player's way:
// - out of the Dock: every activation policy Godot asks for becomes Accessory,
//   and Carbon's request to become a Dock app makes it a UI element;
// - out of sight and never focused: Godot never activates itself, and a window
//   it brings forward goes behind the player's windows with the app hidden, as
//   if minimized but with no thumbnail in the Dock. A hidden Godot still draws
//   every frame, so the checks that read the screen see real images.
#include <objc/message.h>
#include <objc/runtime.h>
#include <stdint.h>

typedef signed char BOOL_T;
static IMP original_policy;

static BOOL_T accessory(id self, SEL command, long policy)
{
    (void)policy;
    return ((BOOL_T(*)(id, SEL, long))original_policy)(self, command, 1);
}

static void stay_inactive(id self, SEL command, BOOL_T flag)
{
    (void)self;
    (void)command;
    (void)flag;
}

static void stay_inactive_plain(id self, SEL command)
{
    (void)self;
    (void)command;
}

static BOOL_T stay_inactive_with_options(id self, SEL command, unsigned long options)
{
    (void)self;
    (void)command;
    (void)options;
    return 1;
}

static id application(void)
{
    return ((id(*)(id, SEL))objc_msgSend)((id)objc_getClass("NSApplication"),
                                          sel_registerName("sharedApplication"));
}

// Behind every other window, with the app hidden.
static void out_of_sight(id window)
{
    ((void (*)(id, SEL, id))objc_msgSend)(window, sel_registerName("orderBack:"), (id)0);
    ((void (*)(id, SEL, id))objc_msgSend)(application(), sel_registerName("hide:"), (id)0);
}

static void quietly_front(id self, SEL command, id sender)
{
    (void)command;
    (void)sender;
    out_of_sight(self);
}

static void quietly_front_plain(id self, SEL command)
{
    (void)command;
    out_of_sight(self);
}

static void replace(const char *class_name, const char *selector, IMP replacement)
{
    Class owner = objc_getClass(class_name);
    Method method = owner ? class_getInstanceMethod(owner, sel_registerName(selector)) : 0;
    if (method)
    {
        method_setImplementation(method, replacement);
    }
}

__attribute__((constructor)) static void keep_out_of_the_way(void)
{
    Class owner = objc_getClass("NSApplication");
    Method policy = owner ? class_getInstanceMethod(owner, sel_registerName("setActivationPolicy:")) : 0;
    if (policy)
    {
        original_policy = method_setImplementation(policy, (IMP)accessory);
    }
    replace("NSApplication", "activateIgnoringOtherApps:", (IMP)stay_inactive);
    replace("NSApplication", "activate", (IMP)stay_inactive_plain);
    replace("NSApplication", "unhide:", (IMP)stay_inactive);
    replace("NSRunningApplication", "activateWithOptions:", (IMP)stay_inactive_with_options);
    replace("NSWindow", "makeKeyAndOrderFront:", (IMP)quietly_front);
    replace("NSWindow", "orderFront:", (IMP)quietly_front);
    replace("NSWindow", "orderFrontRegardless", (IMP)quietly_front_plain);
    replace("NSWindow", "makeKeyWindow", (IMP)stay_inactive_plain);
}

// Carbon's way to become a Dock app; asked for, it makes the app a UI element
// instead (kProcessTransformToUIElementApplication).
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
