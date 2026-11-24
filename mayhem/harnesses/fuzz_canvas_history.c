// Fuzz the drawdance network protocol as every peer executes it: the input is a stream of
// wire messages (2-byte big-endian body length, type, context id, body), each deserialized by
// DP_message_deserialize and executed on a fresh canvas history with DP_canvas_history_handle,
// the same pipeline upstream's handle_* tests use (libengine/test/handle_common.h).
#include <dpcommon/common.h>
#include <dpcommon/cpu.h>
#include <dpengine/canvas_history.h>
#include <dpengine/draw_context.h>
#include <dpmsg/message.h>
#include <stddef.h>
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
    DP_CanvasHistory *ch = DP_canvas_history_new(NULL, NULL, false, NULL);
    DP_DrawContext *dc = DP_draw_context_new();
    size_t pos = 0;
    while (size - pos >= DP_MESSAGE_HEADER_LENGTH) {
        size_t total = DP_MESSAGE_HEADER_LENGTH
                     + (((size_t)data[pos] << 8) | (size_t)data[pos + 1]);
        if (total > size - pos) {
            break;
        }
        DP_Message *msg = DP_message_deserialize(data + pos, total, true);
        pos += total;
        if (msg) {
            DP_canvas_history_handle(ch, dc, msg);
            DP_message_decref(msg);
        }
    }
    DP_draw_context_free(dc);
    DP_canvas_history_free(ch);
    return 0;
}
