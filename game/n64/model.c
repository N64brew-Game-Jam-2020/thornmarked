#include "game/n64/model.h"

#include "assets/model.h"
#include "assets/pak.h"
#include "assets/texture.h"
#include "base/base.h"
#include "base/hash.h"
#include "base/mat4.h"
#include "base/n64/mat4.h"
#include "base/pak/pak.h"
#include "base/vec2.h"
#include "base/vec3.h"
#include "game/defs.h"
#include "game/graphics.h"
#include "game/model.h"
#include "game/n64/texture.h"
#include "game/physics.h"

enum {
    // Number of model assets which can be loaded at once.
    MODEL_SLOTS = 3,

    // Number of animation frames which can be loaded at once.
    FRAME_SLOTS = 8,

    // Number of buckets in frame hash table. Must be a power of two, must be
    // larger than FRAME_SLOTS by some margin.
    FRAME_BUCKETS = 16,
};

// =============================================================================
// Models
// =============================================================================

// A frame in a model animation.
struct model_frame {
    float time;
    float inv_dt;    // Inverse of time delta to next frame.
    unsigned vertex; // Cartridge address of vertex data.
};

// An animation in a model.
struct model_animation {
    float duration;
    int frame_count;
    struct model_frame *frame;
};

// Header for the model data.
struct model_header {
    Vtx *vertex_data;
    Gfx *display_list;
    int animation_count;
    unsigned frame_size;
    struct model_animation animation[];
};

// Model data, including buffer.
union model_data {
    struct model_header header;
    uint8_t data[10 * 1024];
};

// Loaded models.
static union model_data model_data[MODEL_SLOTS] ASSET;

// Map from model asset ID to slot number. If the slot maps back to the same
// model asset, then this texture is loaded, otherwise it is not loaded.
static int model_to_slot[PAK_MODEL_COUNT + 1];

// Map from model slot number to model asset ID.
static int model_from_slot[MODEL_SLOTS];

// Convert a relative offset to a pointer.
static void *pointer_fixup(void *ptr, uintptr_t base, size_t size) {
    uintptr_t value = (uintptr_t)ptr;
    if (value > size) {
        fatal_error("Bad pointer in asset\nPointer: %p\nBase: %p\nSize: %zu",
                    ptr, (void *)base, size);
    }
    return (void *)(value + base);
}

// Fix the internal pointers in a model after loading.
static void model_fixup(union model_data *p, pak_model asset) {
    const struct pak_object vtx_obj = pak_objects[pak_model_object(asset) + 1];
    const uintptr_t base = (uintptr_t)p;
    const size_t size = sizeof(union model_data);
    struct model_header *restrict hdr = &p->header;
    const unsigned max_vtx_offset =
        vtx_obj.size < hdr->frame_size ? 0 : vtx_obj.size - hdr->frame_size;
    hdr->vertex_data = pointer_fixup(hdr->vertex_data, base, size);
    hdr->display_list = pointer_fixup(hdr->display_list, base, size);
    for (int i = 0; i < hdr->animation_count; i++) {
        struct model_animation *restrict anim = &hdr->animation[i];
        if (anim->frame_count > 0) {
            struct model_frame *restrict frame =
                pointer_fixup(hdr->animation[i].frame, base, size);
            anim->frame = frame;
            for (int j = 0; j < anim->frame_count; j++) {
                unsigned vtx_offset = frame[j].vertex;
                if (vtx_offset > max_vtx_offset) {
                    fatal_error("Bad vertex offset\nOffset: $%x", vtx_offset);
                }
                frame[j].vertex = vtx_obj.offset + vtx_offset;
            }
        }
    }
}

// Load a model into the given slot.
static void model_load_slot(pak_model asset, int slot) {
    pak_load_asset_sync(&model_data[slot], sizeof(model_data[slot]),
                        pak_model_object(asset));
    model_fixup(&model_data[slot], asset);
    model_to_slot[asset.id] = slot;
    model_from_slot[slot] = asset.id;
}

// Load a model and return the slot.
static int model_load(pak_model asset) {
    if (asset.id < 1 || PAK_MODEL_COUNT < asset.id) {
        fatal_error("model_load: invalid model\nModel: %d", asset.id);
    }
    int slot = model_to_slot[asset.id];
    if (model_from_slot[slot] == asset.id) {
        return slot;
    }
    for (slot = 0; slot < MODEL_SLOTS; slot++) {
        if (model_from_slot[slot] == 0) {
            model_load_slot(asset, slot);
            return slot;
        }
    }
    fatal_error("model_load: no slots available");
}

// =============================================================================
// Animation Frames
// =============================================================================

// Must be a power of two.
static_assert((FRAME_BUCKETS & (FRAME_BUCKETS - 1)) == 0);
// Must have more hash buckets than possible entries in hash table.
static_assert(FRAME_BUCKETS > FRAME_SLOTS);

// Bucket in frame hash table.
struct frame_bucket {
    unsigned frame;
    int slot;
};

// Hash table mapping frame addresses to slots.
static struct frame_bucket frame_to_slot[FRAME_BUCKETS];

// Get the slot for a frame, or return -1 if the frame is not loaded.
static int frame_slot_get(struct frame_bucket *restrict table, unsigned hash,
                          unsigned frame_addr) {
    const unsigned mask = FRAME_BUCKETS - 1;
    for (unsigned i = 0; i < FRAME_BUCKETS; i++) {
        unsigned pos = (hash + i) & mask;
        if (table[pos].frame == frame_addr) {
            return table[pos].slot;
        }
        if (table[pos].frame == 0) {
            break;
        }
    }
    return -1;
}

// Set the slot that a frame maps to.
static void frame_slot_set(struct frame_bucket *restrict table, unsigned hash,
                           unsigned frame_addr, int slot) {
    const unsigned mask = FRAME_BUCKETS - 1;
    for (unsigned i = 0; i < FRAME_BUCKETS; i++) {
        unsigned pos = (hash + i) & mask;
        if (table[pos].frame == frame_addr) {
            table[pos].slot = slot;
            return;
        }
        if (table[pos].frame == 0) {
            table[pos] = (struct frame_bucket){frame_addr, slot};
            return;
        }
    }
    fatal_error("Frame table full");
}

// Erase an entry mapping a frame to a slot.
static void frame_slot_erase(struct frame_bucket *restrict table, unsigned hash,
                             unsigned frame_addr) {
    const unsigned mask = FRAME_BUCKETS - 1;
    // Find and remove this hash entry.
    bool found = false;
    unsigned pos;
    for (unsigned i = 0; i < FRAME_BUCKETS; i++) {
        pos = (hash + i) & mask;
        if (table[pos].frame == frame_addr) {
            found = true;
            table[pos] = (struct frame_bucket){0, 0};
            break;
        }
    }
    if (found) {
        // Move later hash entries back to fill in the gap.
        unsigned hole = pos;
        for (unsigned i = 1; i < FRAME_BUCKETS; i++) {
            unsigned npos = (pos + i) & mask;
            if (table[npos].frame == 0) {
                break;
            }
            unsigned nhash = hash32(table[npos].frame);
            unsigned hole_dist = (hole - nhash) & mask;
            unsigned cur_dist = (npos - nhash) & mask;
            if (hole_dist < cur_dist) {
                table[hole] = table[npos];
                table[npos] = (struct frame_bucket){0, 0};
                hole = npos;
            }
        }
    }
}

// Loaded animation frame data.
static uint8_t frame_data[FRAME_SLOTS][5 * 1024] ASSET;

// Map from slots to animation frame cartridge addresses.
static unsigned frame_from_slot[FRAME_SLOTS];

// Next slot to load into.
static int frame_next_slot;

// Load an animation frame. Return the slot index.
static int frame_load(unsigned frame_addr, unsigned size) {
    unsigned hash = hash32(frame_addr);

    // Find the frame if it is loaded.
    int slot = frame_slot_get(frame_to_slot, hash, frame_addr);

    // Find an empty slot, and load it there.
    slot = frame_next_slot++;
    if (frame_next_slot >= FRAME_SLOTS) {
        frame_next_slot = 0;
    }
    if (size > sizeof(frame_data[slot])) {
        fatal_error(
            "frame_load_slot: frame too large\n"
            "Frame size: %u\nSlot size: %zu\n",
            size, sizeof(frame_data[slot]));
    }
    pak_load_data_sync(&frame_data[slot], frame_addr, size);
    unsigned old_addr = frame_from_slot[slot];
    if (old_addr != 0) {
        frame_slot_erase(frame_to_slot, hash32(old_addr), old_addr);
    }
    frame_slot_set(frame_to_slot, hash, frame_addr, slot);
    frame_from_slot[slot] = frame_addr;
    return slot;
}

// =============================================================================
// Public
// =============================================================================

void model_render_init(void) {
    model_load(MODEL_FAIRY);
    model_load(MODEL_BLUEENEMY);
    model_load(MODEL_GREENENEMY);
}

static const Gfx fairy_setup_dl[] = {
    gsDPPipeSync(),
    gsSPGeometryMode(G_LIGHTING, G_CULL_BACK | G_SHADE | G_SHADING_SMOOTH),
    gsDPSetCombineMode(G_CC_TRILERP, G_CC_MODULATERGB2),
    gsSPEndDisplayList(),
};

static int anim_id, frame_id;

Gfx *model_render(Gfx *dl, struct graphics *restrict gr,
                  struct sys_model *restrict msys,
                  struct sys_phys *restrict psys) {
    int current_model = 0;
    float scale = 0.5f;
    for (unsigned i = 0; i < psys->count; i++) {
        struct cp_phys *restrict cp = &psys->entities[i];
        if (i >= msys->count) {
            continue;
        }
        struct cp_model *restrict mp = &msys->entities[i];
        int model = mp->model_id.id;
        if (model == 0) {
            continue;
        }
        int slot = model_to_slot[model];
        if (model_from_slot[slot] != model) {
            fatal_error("Model not loaded");
        }
        if (model != current_model) {
            const struct model_header *restrict mp = &model_data[slot].header;
            switch (model) {
            case ID_MODEL_FAIRY:
                gSPDisplayList(dl++, fairy_setup_dl);
                dl = texture_use(dl, IMG_FAIRY);
                unsigned frame_addr =
                    mp->animation[anim_id].frame[frame_id].vertex;
                int frame_slot = frame_load(frame_addr, mp->frame_size);
                gSPSegment(dl++, 1, K0_TO_PHYS(frame_data[frame_slot]));
                break;
            case ID_MODEL_BLUEENEMY:
                gSPDisplayList(dl++, fairy_setup_dl);
                dl = texture_use(dl, IMG_BLUEENEMY);
                gSPSegment(dl++, 1, K0_TO_PHYS(mp->vertex_data));
                break;
            case ID_MODEL_GREENENEMY:
                gSPDisplayList(dl++, fairy_setup_dl);
                dl = texture_use(dl, IMG_GREENENEMY);
                gSPSegment(dl++, 1, K0_TO_PHYS(mp->vertex_data));
                break;
            default:
                fatal_error("Cannot use model\nModel: %d", model);
            }
        }
        current_model = model;
        Mtx *mtx = gr->mtx_ptr++;
        {
            mat4 mat;
            mat4_translate_rotate_scale(
                &mat, vec3_vec2(vec2_scale(cp->pos, meter), meter),
                cp->orientation, scale);
            mat4_tofixed(mtx, &mat);
        }
        gSPMatrix(dl++, K0_TO_PHYS(mtx),
                  G_MTX_MODELVIEW | G_MTX_LOAD | G_MTX_NOPUSH);
        gSPDisplayList(dl++, K0_TO_PHYS(model_data[slot].header.display_list));
    }

    frame_id++;
    if (frame_id >= model_data[0].header.animation[anim_id].frame_count) {
        frame_id = 0;
        anim_id++;
        if (anim_id >= model_data[0].header.animation_count) {
            anim_id = 0;
        }
    }

    return dl;
}
