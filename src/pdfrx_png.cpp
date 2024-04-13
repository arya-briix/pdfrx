#define FPNG_NO_STDIO 1

#include "fpng.h"

#ifdef __ANDROID__
#undef __INTRODUCED_IN
#define __INTRODUCED_IN(__api_level)
#include <android/log.h>
#include <android/bitmap.h>
#include <android/data_space.h>
#include <dlfcn.h>
#endif

#if defined(_WIN32)
#define EXPORT __declspec(dllexport)
#define PDFRX_PNG_API __stdcall
#else
#define EXPORT __attribute__((visibility("default"))) __attribute__((used))
#define PDFRX_PNG_API
#endif

#ifdef __ANDROID__
typedef int (*AndroidBitmap_compress_func)(const AndroidBitmapInfo *info,
                                           int32_t dataspace,
                                           const void *pixels,
                                           int32_t format, int32_t quality,
                                           void *userContext,
                                           AndroidBitmap_CompressWriteFunc fn);
AndroidBitmap_compress_func pAndroidBitmap_compress = nullptr;
#endif

extern "C" EXPORT void PDFRX_PNG_API pdfrx_init_png()
{
    fpng::fpng_init();

#ifdef __ANDROID__
    auto lib = dlopen("libjnigraphics.so", RTLD_LAZY);
    if (lib)
    {
        pAndroidBitmap_compress = (AndroidBitmap_compress_func)dlsym(lib, "AndroidBitmap_compress");
        if (pAndroidBitmap_compress)
        {
            __android_log_print(ANDROID_LOG_INFO, "pdfrx_png", "pdfrx_png can use AndroidBitmap_compress");
        }
    }
#endif
}

extern "C" EXPORT void *PDFRX_PNG_API pdfrx_bgra_to_png(const void *image, uint32_t w, uint32_t h, size_t *pSize)
{
    size_t size = w * h;
    for (size_t i = 0; i < size; i++)
    {
        auto p = reinterpret_cast<uint32_t *>(const_cast<void *>(image));
        auto c = p[i];
        p[i] = (c & 0xFF00FF00) | ((c & 0xFF) << 16) | ((c >> 16) & 0xFF);
    }

    std::vector<uint8_t> buf;

#ifdef __ANDROID__
    if (pAndroidBitmap_compress)
    {
        AndroidBitmapInfo info{
            .width = w,
            .height = h,
            .stride = w * 4,
            .format = ANDROID_BITMAP_FORMAT_RGBA_8888,
            .flags = ANDROID_BITMAP_FLAGS_ALPHA_PREMUL,
        };
        pAndroidBitmap_compress(
            &info, ADATASPACE_SRGB, image, ANDROID_BITMAP_COMPRESS_FORMAT_PNG, 0, &buf,
            [](void *userContext, const void *data, size_t size) -> bool
            {
            auto &buf = *reinterpret_cast<std::vector<uint8_t> *>(userContext);
            auto ptr = reinterpret_cast<const uint8_t *>(data);
            buf.insert(buf.end(), ptr, ptr + size);
            return true; });
    }
    else
#endif
    {
        if (!fpng::fpng_encode_image_to_memory(image, w, h, 4, buf))
        {
            *pSize = 0;
            return nullptr;
        }
    }

#if defined(_WIN32)
    // NOTE: Dart FFI's malloc.free uses CoTaskMemFree on Windows
    auto p = CoTaskMemAlloc(buf.size());
#else
    auto p = malloc(buf.size());
#endif
    if (p != nullptr)
    {
        memcpy(p, buf.data(), buf.size());
    }
    *pSize = buf.size();
    return p;
}
