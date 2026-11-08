# Sets virtual_display to the command that runs a windowed check in its own
# invisible display, or to nothing where there is none. Drawn there, the check
# needs no screen: it passes on a headless host or with the screen asleep, and
# its window stays off the desktop. cage, a small Wayland compositor, draws into
# memory with the graphics card and also starts Xwayland for X11 programs.
# Systems without it, such as the AlmaLinux 8 release container, use Xvfb, which
# draws in software at the game's window size.
set(virtual_display)
find_program(CAGE cage)
find_program(XVFB_RUN xvfb-run)
if(CAGE)
    set(ENV{WLR_BACKENDS} headless)
    set(virtual_display "${CAGE}" --)
elseif(XVFB_RUN)
    set(virtual_display "${XVFB_RUN}" --auto-servernum "--server-args=-screen 0 1920x1080x24")
endif()
