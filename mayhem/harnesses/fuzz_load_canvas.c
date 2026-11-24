// Fuzz Drawpile's canvas file loader (libimpex DP_load_canvas), the code the client runs when a
// user opens a file. DP_load_canvas only takes a path, so the input is written to a scratch file
// with the extension of the format under test:
//   FUZZ_LOAD_TYPE=DP_SAVE_IMAGE_ORA     -> OpenRaster: libzip archive, stack.xml (Qt XML stream),
//                                           layer/group/track/annotation attributes, PNG layers
//   FUZZ_LOAD_TYPE=DP_SAVE_IMAGE_UNKNOWN -> content sniffing (DP_load_guess) and dispatch to every
//                                           loader: ORA, PSD, project, PNG/JPEG/WebP/QOI flat images
// Each input gets a fresh draw context and a fresh scratch file; nothing is kept across inputs.
#define _GNU_SOURCE
#include <dpcommon/common.h>
#include <dpcommon/cpu.h>
#include <dpengine/canvas_state.h>
#include <dpengine/draw_context.h>
#include <dpengine/save_enums.h>
#include <dpimpex/image_impex.h>
#include <dpimpex/load.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#ifndef FUZZ_LOAD_TYPE
#    error "define FUZZ_LOAD_TYPE"
#endif
#ifndef FUZZ_LOAD_SUFFIX
#    define FUZZ_LOAD_SUFFIX ""
#endif

static char path[4096];

int LLVMFuzzerInitialize(int *argc, char ***argv)
{
    (void)argc;
    (void)argv;
    DP_cpu_support_init();
    DP_image_impex_init();
    const char *tmp = getenv("TMPDIR");
    snprintf(path, sizeof(path), "%s/fuzz_load_XXXXXX" FUZZ_LOAD_SUFFIX,
             tmp && *tmp ? tmp : "/tmp");
    int fd = mkstemps(path, (int)strlen(FUZZ_LOAD_SUFFIX));
    if (fd < 0) {
        perror("mkstemps");
        abort();
    }
    close(fd);
    return 0;
}

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size)
{
    FILE *fp = fopen(path, "wb");
    if (!fp) {
        abort();
    }
    if (size != 0 && fwrite(data, 1, size, fp) != size) {
        abort();
    }
    fclose(fp);

    DP_DrawContext *dc = DP_draw_context_new();
    DP_LoadContext lc = DP_load_context_make(path, dc);
    lc.in.type = FUZZ_LOAD_TYPE;
    DP_load_canvas(&lc);
    DP_load_context_dispose(&lc);
    DP_draw_context_free(dc);
    return 0;
}
