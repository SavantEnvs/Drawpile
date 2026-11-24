// Fuzz the drawdance recording readers through the player API: the input is a whole recording
// (.dprec binary, .dptxt text, or a debug dump, picked by FUZZ_PLAYER_TYPE at build time) read
// from memory by DP_player_new and stepped to the end. Binary and text recordings are played onto
// a canvas history, as recording playback does.
#include <dpcommon/common.h>
#include <dpcommon/cpu.h>
#include <dpcommon/input.h>
#include <dpengine/canvas_history.h>
#include <dpengine/draw_context.h>
#include <dpengine/player.h>
#include <dpmsg/message.h>
#include <stddef.h>
#include <stdint.h>

#ifndef FUZZ_PLAYER_TYPE
#    error "define FUZZ_PLAYER_TYPE"
#endif

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
    DP_LoadResult result;
    DP_Player *player = DP_player_new(FUZZ_PLAYER_TYPE, NULL, input, &result);
    if (!player) {
        return 0;
    }
    if (FUZZ_PLAYER_TYPE == DP_PLAYER_TYPE_DEBUG_DUMP) {
        // Debug dumps have no header; DP_player_compatibility asserts on them.
        for (;;) {
            DP_DumpType type;
            int count;
            DP_Message **msgs;
            if (DP_player_step_dump(player, &type, &count, &msgs)
                != DP_PLAYER_SUCCESS) {
                break;
            }
        }
    }
    else {
        (void)DP_player_header(player);
        (void)DP_player_compatibility(player);
        DP_CanvasHistory *ch = DP_canvas_history_new(NULL, NULL, false, NULL);
        DP_DrawContext *dc = DP_draw_context_new();
        for (;;) {
            DP_Message *msg;
            DP_PlayerResult r = DP_player_step(player, true, &msg);
            if (r == DP_PLAYER_SUCCESS) {
                DP_canvas_history_handle(ch, dc, msg);
                DP_message_decref(msg);
            }
            else if (r != DP_PLAYER_ERROR_PARSE) {
                break;
            }
        }
        DP_draw_context_free(dc);
        DP_canvas_history_free(ch);
    }
    DP_player_free(player);
    return 0;
}
