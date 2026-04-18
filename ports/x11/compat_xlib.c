#define XLIB_ILLEGAL_ACCESS 1

#include <X11/Xatom.h>
#include <X11/Xlib.h>
#include <X11/keysym.h>

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
    Atom atom;
    const char *name;
} builtin_atom_t;

typedef struct {
    KeySym sym;
    const char *name;
} builtin_keysym_t;

#include "generated_atoms.h"
#include "generated_keysyms.h"

typedef struct atom_entry {
    Atom atom;
    char *name;
    struct atom_entry *next;
} atom_entry_t;

typedef struct property_entry {
    Window window;
    Atom property;
    Atom type;
    int format;
    unsigned long nitems;
    unsigned char *data;
    struct property_entry *next;
} property_entry_t;

typedef struct {
    Display display;
    Screen screen;
    char *display_name;
    char *vendor_name;
    property_entry_t *properties;
} ohos_display_t;

static atom_entry_t *dynamic_atoms;
static Atom next_dynamic_atom = XA_LAST_PREDEFINED + 1;

static char *x11_strdup(const char *value) {
    size_t length;
    char *copy;

    if (value == NULL) {
        return NULL;
    }

    length = strlen(value) + 1;
    copy = (char *)malloc(length);
    if (copy == NULL) {
        return NULL;
    }

    memcpy(copy, value, length);
    return copy;
}

static ohos_display_t *unwrap_display(Display *display) {
    if (display == NULL) {
        return NULL;
    }

    return (ohos_display_t *)display;
}

static const builtin_atom_t *find_builtin_atom_by_name(const char *name) {
    size_t i;

    for (i = 0; i < sizeof(kBuiltinAtoms) / sizeof(kBuiltinAtoms[0]); ++i) {
        if (strcmp(kBuiltinAtoms[i].name, name) == 0) {
            return &kBuiltinAtoms[i];
        }
    }

    return NULL;
}

static const builtin_atom_t *find_builtin_atom_by_id(Atom atom) {
    size_t i;

    for (i = 0; i < sizeof(kBuiltinAtoms) / sizeof(kBuiltinAtoms[0]); ++i) {
        if (kBuiltinAtoms[i].atom == atom) {
            return &kBuiltinAtoms[i];
        }
    }

    return NULL;
}

static atom_entry_t *find_dynamic_atom_by_name(const char *name) {
    atom_entry_t *entry = dynamic_atoms;

    while (entry != NULL) {
        if (strcmp(entry->name, name) == 0) {
            return entry;
        }
        entry = entry->next;
    }

    return NULL;
}

static atom_entry_t *find_dynamic_atom_by_id(Atom atom) {
    atom_entry_t *entry = dynamic_atoms;

    while (entry != NULL) {
        if (entry->atom == atom) {
            return entry;
        }
        entry = entry->next;
    }

    return NULL;
}

static property_entry_t *find_property(ohos_display_t *state, Window window, Atom property) {
    property_entry_t *entry = state->properties;

    while (entry != NULL) {
        if (entry->window == window && entry->property == property) {
            return entry;
        }
        entry = entry->next;
    }

    return NULL;
}

static void destroy_property(property_entry_t *entry) {
    if (entry == NULL) {
        return;
    }

    free(entry->data);
    free(entry);
}

static void remove_property(ohos_display_t *state, property_entry_t *target) {
    property_entry_t *prev = NULL;
    property_entry_t *entry = state->properties;

    while (entry != NULL) {
        if (entry == target) {
            if (prev == NULL) {
                state->properties = entry->next;
            } else {
                prev->next = entry->next;
            }
            destroy_property(entry);
            return;
        }

        prev = entry;
        entry = entry->next;
    }
}

static unsigned long item_size_for_format(int format) {
    switch (format) {
        case 8:
            return 1;
        case 16:
            return 2;
        case 32:
            return 4;
        default:
            return 0;
    }
}

Display *XOpenDisplay(_Xconst char *display_name) {
    ohos_display_t *state = (ohos_display_t *)calloc(1, sizeof(*state));

    if (state == NULL) {
        return NULL;
    }

    state->display_name = x11_strdup(display_name != NULL ? display_name : ":0");
    state->vendor_name = x11_strdup("OpenHobbyOS");
    if (state->display_name == NULL || state->vendor_name == NULL) {
        free(state->display_name);
        free(state->vendor_name);
        free(state);
        return NULL;
    }

    state->display.vendor = state->vendor_name;
    state->display.display_name = state->display_name;
    state->display.default_screen = 0;
    state->display.nscreens = 1;
    state->display.screens = &state->screen;
    state->display.byte_order = LSBFirst;
    state->display.bitmap_unit = 32;
    state->display.bitmap_pad = 32;
    state->display.bitmap_bit_order = LSBFirst;
    state->display.release = 1;
    state->display.proto_major_version = 11;
    state->display.proto_minor_version = 0;
    state->display.max_request_size = 65535;
    state->display.min_keycode = 8;
    state->display.max_keycode = 255;

    state->screen.display = &state->display;
    state->screen.root = 1;
    state->screen.width = 1024;
    state->screen.height = 768;
    state->screen.mwidth = 270;
    state->screen.mheight = 203;
    state->screen.ndepths = 1;
    state->screen.root_depth = 24;
    state->screen.white_pixel = 0x00ffffffUL;
    state->screen.black_pixel = 0x00000000UL;
    state->screen.max_maps = 1;
    state->screen.min_maps = 1;
    state->screen.backing_store = NotUseful;
    state->screen.save_unders = False;

    return &state->display;
}

int XCloseDisplay(Display *display) {
    ohos_display_t *state = unwrap_display(display);
    property_entry_t *property;
    property_entry_t *next_property;

    if (state == NULL) {
        return 0;
    }

    property = state->properties;
    while (property != NULL) {
        next_property = property->next;
        destroy_property(property);
        property = next_property;
    }

    free(state->display_name);
    free(state->vendor_name);
    free(state);
    return 0;
}

Atom XInternAtom(Display *display, _Xconst char *atom_name, Bool only_if_exists) {
    const builtin_atom_t *builtin;
    atom_entry_t *entry;
    (void)display;

    if (atom_name == NULL || atom_name[0] == '\0') {
        return None;
    }

    builtin = find_builtin_atom_by_name(atom_name);
    if (builtin != NULL) {
        return builtin->atom;
    }

    entry = find_dynamic_atom_by_name(atom_name);
    if (entry != NULL) {
        return entry->atom;
    }

    if (only_if_exists) {
        return None;
    }

    entry = (atom_entry_t *)calloc(1, sizeof(*entry));
    if (entry == NULL) {
        return None;
    }

    entry->name = x11_strdup(atom_name);
    if (entry->name == NULL) {
        free(entry);
        return None;
    }

    entry->atom = next_dynamic_atom++;
    entry->next = dynamic_atoms;
    dynamic_atoms = entry;
    return entry->atom;
}

char *XGetAtomName(Display *display, Atom atom) {
    const builtin_atom_t *builtin;
    atom_entry_t *entry;
    (void)display;

    builtin = find_builtin_atom_by_id(atom);
    if (builtin != NULL) {
        return x11_strdup(builtin->name);
    }

    entry = find_dynamic_atom_by_id(atom);
    if (entry != NULL) {
        return x11_strdup(entry->name);
    }

    return NULL;
}

char *XKeysymToString(KeySym keysym) {
    size_t i;

    for (i = 0; i < sizeof(kBuiltinKeysyms) / sizeof(kBuiltinKeysyms[0]); ++i) {
        if (kBuiltinKeysyms[i].sym == keysym) {
            return (char *)kBuiltinKeysyms[i].name;
        }
    }

    return NULL;
}

int XChangeProperty(Display *display, Window window, Atom property, Atom type,
                    int format, int mode, _Xconst unsigned char *data, int nelements) {
    ohos_display_t *state = unwrap_display(display);
    property_entry_t *entry;
    unsigned long item_size;
    unsigned long bytes;
    unsigned char *copy;

    if (state == NULL || nelements < 0) {
        return 0;
    }

    item_size = item_size_for_format(format);
    if (item_size == 0) {
        return 0;
    }

    bytes = (unsigned long)nelements * item_size;
    copy = NULL;
    if (bytes > 0) {
        copy = (unsigned char *)malloc(bytes);
        if (copy == NULL) {
            return 0;
        }
        memcpy(copy, data, bytes);
    }

    entry = find_property(state, window, property);
    if (entry == NULL) {
        entry = (property_entry_t *)calloc(1, sizeof(*entry));
        if (entry == NULL) {
            free(copy);
            return 0;
        }
        entry->window = window;
        entry->property = property;
        entry->next = state->properties;
        state->properties = entry;
    }

    if (mode != PropModeReplace && (entry->format != format || entry->type != type)) {
        free(entry->data);
        entry->data = NULL;
        entry->nitems = 0;
        mode = PropModeReplace;
    }

    if (mode == PropModeReplace) {
        free(entry->data);
        entry->data = copy;
        entry->type = type;
        entry->format = format;
        entry->nitems = (unsigned long)nelements;
        return 1;
    }

    {
        unsigned long old_size = entry->nitems * item_size;
        unsigned long new_size = old_size + bytes;
        unsigned char *merged = (unsigned char *)malloc(new_size);
        if (merged == NULL) {
            free(copy);
            return 0;
        }

        if (mode == PropModeAppend) {
            if (old_size > 0) {
                memcpy(merged, entry->data, old_size);
            }
            if (bytes > 0) {
                memcpy(merged + old_size, copy, bytes);
            }
        } else {
            if (bytes > 0) {
                memcpy(merged, copy, bytes);
            }
            if (old_size > 0) {
                memcpy(merged + bytes, entry->data, old_size);
            }
        }

        free(entry->data);
        free(copy);
        entry->data = merged;
        entry->type = type;
        entry->format = format;
        entry->nitems += (unsigned long)nelements;
    }

    return 1;
}

int XGetWindowProperty(Display *display, Window window, Atom property, long long_offset,
                       long long_length, Bool delete, Atom req_type, Atom *actual_type_return,
                       int *actual_format_return, unsigned long *nitems_return,
                       unsigned long *bytes_after_return, unsigned char **prop_return) {
    ohos_display_t *state = unwrap_display(display);
    property_entry_t *entry;
    unsigned long item_size;
    unsigned long total_bytes;
    unsigned long offset_bytes;
    unsigned long max_bytes;
    unsigned long copy_bytes;

    if (actual_type_return != NULL) {
        *actual_type_return = None;
    }
    if (actual_format_return != NULL) {
        *actual_format_return = 0;
    }
    if (nitems_return != NULL) {
        *nitems_return = 0;
    }
    if (bytes_after_return != NULL) {
        *bytes_after_return = 0;
    }
    if (prop_return != NULL) {
        *prop_return = NULL;
    }

    if (state == NULL || long_offset < 0 || long_length < 0) {
        return BadValue;
    }

    entry = find_property(state, window, property);
    if (entry == NULL) {
        return Success;
    }

    item_size = item_size_for_format(entry->format);
    if (item_size == 0) {
        return BadValue;
    }

    if (actual_type_return != NULL) {
        *actual_type_return = entry->type;
    }
    if (actual_format_return != NULL) {
        *actual_format_return = entry->format;
    }

    total_bytes = entry->nitems * item_size;
    offset_bytes = (unsigned long)long_offset * 4UL;
    if (offset_bytes > total_bytes) {
        return BadValue;
    }

    if (req_type != AnyPropertyType && req_type != entry->type) {
        if (bytes_after_return != NULL) {
            *bytes_after_return = total_bytes;
        }
        return Success;
    }

    max_bytes = (unsigned long)long_length * 4UL;
    if (offset_bytes == total_bytes || max_bytes == 0) {
        if (bytes_after_return != NULL) {
            *bytes_after_return = total_bytes - offset_bytes;
        }
        if (delete && bytes_after_return != NULL && *bytes_after_return == 0) {
            remove_property(state, entry);
        }
        return Success;
    }

    copy_bytes = total_bytes - offset_bytes;
    if (copy_bytes > max_bytes) {
        copy_bytes = max_bytes;
    }

    if (prop_return != NULL) {
        *prop_return = (unsigned char *)malloc(copy_bytes + 1);
        if (*prop_return == NULL) {
            return BadAlloc;
        }
        memcpy(*prop_return, entry->data + offset_bytes, copy_bytes);
        (*prop_return)[copy_bytes] = '\0';
    }

    if (nitems_return != NULL) {
        *nitems_return = copy_bytes / item_size;
    }
    if (bytes_after_return != NULL) {
        *bytes_after_return = total_bytes - offset_bytes - copy_bytes;
    }

    if (delete && bytes_after_return != NULL && *bytes_after_return == 0) {
        remove_property(state, entry);
    }

    return Success;
}

int XFree(void *data) {
    free(data);
    return 1;
}
