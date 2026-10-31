#!/usr/bin/env python3
"""Names the functions behind a Windows crash report's opengoldbox_game.dll+<offset> lines.

Usage: decode_windows_crash.py opengoldbox_game.pdb 22996e 95826 ...

The PDB comes from the release's -windows-symbols.zip. Uses Windows' own dbghelp,
so no debugger needs to be installed. See docs/CONFIGURATION.md#crash-reports.
"""
import ctypes
import ctypes.wintypes as wintypes
import sys

# Any address will do: a report's offsets are relative to the module's start.
MODULE_BASE = 0x10000000
MODULE_SIZE = 0x4000000
NAME_CAPACITY = 500


class Symbol(ctypes.Structure):
    """dbghelp's SYMBOL_INFO, with room for the name after it."""

    _fields_ = [("SizeOfStruct", wintypes.ULONG), ("TypeIndex", wintypes.ULONG),
                ("Reserved", ctypes.c_ulonglong * 2), ("Index", wintypes.ULONG),
                ("Size", wintypes.ULONG), ("ModBase", ctypes.c_ulonglong), ("Flags", wintypes.ULONG),
                ("Value", ctypes.c_ulonglong), ("Address", ctypes.c_ulonglong),
                ("Register", wintypes.ULONG), ("Scope", wintypes.ULONG), ("Tag", wintypes.ULONG),
                ("NameLen", wintypes.ULONG), ("MaxNameLen", wintypes.ULONG),
                ("Name", ctypes.c_char * (NAME_CAPACITY + 1))]


def main():
    if len(sys.argv) < 3:
        raise SystemExit(__doc__)
    dbghelp = ctypes.WinDLL("dbghelp")
    dbghelp.SymLoadModuleEx.restype = ctypes.c_ulonglong
    dbghelp.SymLoadModuleEx.argtypes = [wintypes.HANDLE, wintypes.HANDLE, ctypes.c_char_p,
                                        ctypes.c_char_p, ctypes.c_ulonglong, wintypes.DWORD,
                                        ctypes.c_void_p, wintypes.DWORD]
    dbghelp.SymFromAddr.argtypes = [wintypes.HANDLE, ctypes.c_ulonglong,
                                    ctypes.POINTER(ctypes.c_ulonglong), ctypes.POINTER(Symbol)]
    # dbghelp only needs a value that identifies this session, not a real process.
    session = wintypes.HANDLE(0x1234)
    if not dbghelp.SymInitialize(session, None, False):
        raise SystemExit("Cannot start dbghelp")
    if not dbghelp.SymLoadModuleEx(session, None, sys.argv[1].encode(), None, MODULE_BASE,
                                   MODULE_SIZE, None, 0):
        raise SystemExit("Cannot load " + sys.argv[1])
    for offset in sys.argv[2:]:
        # 88 is the size of SYMBOL_INFO itself, without the name's room.
        symbol = Symbol(SizeOfStruct=88, MaxNameLen=NAME_CAPACITY)
        displacement = ctypes.c_ulonglong()
        if dbghelp.SymFromAddr(session, MODULE_BASE + int(offset, 16), ctypes.byref(displacement),
                               ctypes.byref(symbol)):
            print(f"{offset}: {symbol.Name.decode(errors='replace')} +{displacement.value:#x}")
        else:
            print(f"{offset}: no symbol")


if __name__ == "__main__":
    main()
