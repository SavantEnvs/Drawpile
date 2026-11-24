// Fuzz the drawdance message codec on its own: one wire message per input is deserialized
// (current and compat protocol), serialized back, re-deserialized and compared, and written
// out in the text recording format (DP_message_write_text).
#include <dpcommon/common.h>
#include <dpcommon/cpu.h>
#include <dpcommon/output.h>
#include <dpmsg/message.h>
#include <dpmsg/text_writer.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>

typedef struct Buf {
    unsigned char *data;
    size_t cap;
} Buf;

static unsigned char *get_buffer(void *user, size_t length)
{
    Buf *b = user;
    if (length > b->cap) {
        unsigned char *p = realloc(b->data, length);
        if (!p) {
            return NULL;
        }
        b->data = p;
        b->cap = length;
    }
    return b->data;
}

static void exercise(DP_Message *msg)
{
    Buf b = {NULL, 0};
    size_t len = DP_message_serialize(msg, true, get_buffer, &b);
    if (len != 0) {
        DP_Message *again = DP_message_deserialize(b.data, len, true);
        if (again) {
            (void)DP_message_equals(msg, again);
            DP_message_decref(again);
        }
    }
    free(b.data);

    DP_Output *output = DP_mem_output_new(256, true);
    if (output) {
        DP_TextWriter *writer = DP_text_writer_new(output);
        if (writer) {
            (void)DP_message_write_text(msg, writer);
            DP_text_writer_free(writer); // also frees the output it owns
        }
        else {
            DP_output_free(output);
        }
    }
}

int LLVMFuzzerInitialize(int *argc, char ***argv)
{
    (void)argc;
    (void)argv;
    DP_cpu_support_init();
    return 0;
}

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size)
{
    DP_Message *msg = DP_message_deserialize(data, size, true);
    if (msg) {
        exercise(msg);
        DP_message_decref(msg);
    }
#ifdef DP_PROTOCOL_COMPAT_VERSION
    DP_Message *compat = DP_message_deserialize_compat(data, size, true);
    if (compat) {
        exercise(compat);
        DP_message_decref(compat);
    }
#endif
    return 0;
}
