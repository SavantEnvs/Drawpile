// Fuzz Drawpile's flat image reader (libimpex DP_image_new_from_file with type GUESS: magic
// sniffing, then the PNG/JPEG reader (Qt QImage), the drawdance WebP reader (libwebp) or the
// bundled QOI decoder). The input is read from memory; nothing is kept across inputs.
#include <dpcommon/common.h>
#include <dpcommon/cpu.h>
#include <dpcommon/input.h>
#include <dpengine/image.h>
#include <dpimpex/image_impex.h>
#include <stdint.h>

int LLVMFuzzerInitialize(int *argc, char ***argv)
{
    (void)argc;
    (void)argv;
    DP_cpu_support_init();
    DP_image_impex_init();
    return 0;
}

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size)
{
    DP_Input *input = DP_mem_input_new_keep_on_close(data, size);
    if (!input) {
        return 0;
    }
    DP_ImageFileType out_type;
    DP_Image *img = DP_image_new_from_file(input, DP_IMAGE_FILE_TYPE_GUESS, &out_type);
    if (img) {
        DP_image_free(img);
    }
    DP_input_free(input);
    return 0;
}
