// Fuzz loading Drawpile project files (.dppr, SQLite-based): the input is written to a scratch
// file (DP_project_canvas_load only takes a path) and loaded the way the client opens a project:
// header check (DP_project_check, which tells a full project from a single-snapshot canvas
// file), then DP_project_canvas_load in the matching mode, which opens the database read-only,
// validates the schema and rebuilds the canvas from the latest snapshot and its recorded messages.
#define _GNU_SOURCE
#include <dpcommon/common.h>
#include <dpcommon/cpu.h>
#include <dpengine/canvas_state.h>
#include <dpengine/draw_context.h>
#include <dpengine/project.h>
#include <dpengine/view_state.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static char path[4096];

int LLVMFuzzerInitialize(int *argc, char ***argv)
{
    (void)argc;
    (void)argv;
    DP_cpu_support_init();
    const char *tmp = getenv("TMPDIR");
    snprintf(path, sizeof(path), "%s/fuzz_project_XXXXXX.dppr",
             tmp && *tmp ? tmp : "/tmp");
    int fd = mkstemps(path, 5);
    if (fd < 0) {
        perror("mkstemps");
        abort();
    }
    close(fd);
    return 0;
}

static void remove_scratch(void)
{
    static const char *suffixes[] = {"", "-journal", "-wal", "-shm"};
    char buf[4200];
    for (size_t i = 0; i < sizeof(suffixes) / sizeof(suffixes[0]); ++i) {
        snprintf(buf, sizeof(buf), "%s%s", path, suffixes[i]);
        unlink(buf);
    }
}

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size)
{
    DP_ProjectCheckResult check = DP_project_check(data, size);
    bool snapshot_only = check.result == DP_PROJECT_CHECK_CANVAS;

    remove_scratch();
    FILE *fp = fopen(path, "wb");
    if (!fp) {
        abort();
    }
    if (size != 0 && fwrite(data, 1, size, fp) != size) {
        abort();
    }
    fclose(fp);

    DP_DrawContext *dc = DP_draw_context_new();
    DP_ViewState vs;
    DP_ProjectCanvasLoad pcl = DP_project_canvas_load(dc, path, snapshot_only, &vs);
    DP_canvas_state_decref_nullable(pcl.cs);
    DP_free(pcl.session_source_param);
    DP_draw_context_free(dc);
    remove_scratch();
    return 0;
}
