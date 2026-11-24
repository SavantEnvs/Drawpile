// Fuzz Drawpile's Photoshop importer (libimpex DP_load_psd -> bundled psd_sdk parser: header,
// image resources, layer and mask info, channel data incl. RLE, then conversion to a canvas).
// The input is read from memory; a fresh draw context per input, nothing kept across inputs.
#include <dpcommon/common.h>
#include <dpcommon/cpu.h>
#include <dpcommon/input.h>
#include <dpengine/canvas_state.h>
#include <dpengine/draw_context.h>
#include <dpimpex/load.h>
#include <stdint.h>

int LLVMFuzzerInitialize(int *argc, char ***argv)
{
    (void)argc;
    (void)argv;
    DP_cpu_support_init();
    return 0;
}

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size)
{
    DP_Input *input = DP_mem_input_new_keep_on_close(data, size);
    if (!input) {
        return 0;
    }
    DP_DrawContext *dc = DP_draw_context_new();
    DP_LoadResult result;
    DP_CanvasState *cs = DP_load_psd(dc, input, &result);
    // DP_load_psd takes ownership of the input (DP_PsdFile frees it).
    DP_canvas_state_decref_nullable(cs);
    DP_draw_context_free(dc);
    return 0;
}
