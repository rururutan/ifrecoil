/**
 * @file ifrecoil.c
 * @brief Susie I/F adapter for the Recoil image decoding library.
 */
#define WIN32_LEAN_AND_MEAN
#define _CRT_SECURE_NO_WARNINGS
#include <windows.h>
#include <stdint.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include "spibase.h"
#include "recoil.h"

LONG_PTR SpiGetFileSize(SPI_FILE *fp);

const int NumInfo = 4;
const LPCSTR PluginInfo[] = {
    "00IN", "Recoil image format filter (Recoil " RECOIL_VERSION ")",
    "*.*", "Recoil supported formats"
};

static __declspec(thread) char recoil_last_filename[MAX_PATH] = "image.bin";

static void normalize_filename(char destination[MAX_PATH], const char *source)
{
    size_t i;
    strncpy(destination, source, MAX_PATH - 1);
    destination[MAX_PATH - 1] = '\0';
    for (i = 0; destination[i] != '\0'; ++i) {
        if (destination[i] >= 'A' && destination[i] <= 'Z')
            destination[i] = (char)(destination[i] - 'A' + 'a');
    }
}

static int read_file(SPI_FILE *file, uint8_t **data, size_t *size)
{
    LONG_PTR length = SpiGetFileSize(file);
    if (length <= 0 || length > 0x7fffffff)
        return SPI_ERROR_BROKEN_DATA;
    *size = (size_t)length;
    *data = (uint8_t *)malloc(*size);
    if (*data == NULL) return SPI_ERROR_ALLOCATE_MEMORY;
    SpiSeek(file, 0, FILE_BEGIN);
    if (SpiRead(*data, (DWORD)*size, file) != *size) {
        free(*data); *data = NULL; return SPI_ERROR_FILE_READ;
    }
    return 0;
}

static const char *recoil_name(const SPI_FILE *file)
{
    static __declspec(thread) char normalized[MAX_PATH];
    if (SpiIoType(file) != SPI_IOTYPE_FILE)
        return recoil_last_filename;
    normalize_filename(normalized,
                       file->fname != NULL ? file->fname : "image.bin");
    return normalized;
}

/** Ask Recoil whether a filename extension is supported. */
int IsSupportedFormat(LPBYTE data, DWORD size, LPCSTR filename)
{
    char normalized[MAX_PATH];
    (void)data; (void)size;
    if (filename == NULL)
        return 0;
    normalize_filename(normalized, filename);
    if (!RECOIL_IsOurFile(normalized))
        return 0;
    memcpy(recoil_last_filename, normalized, sizeof(recoil_last_filename));
    return 1;
}

/** Decode once to obtain Recoil's dimensions. */
int GetImageInfo(SPI_FILE *file, PictureInfo *info)
{
    uint8_t *data; size_t size; RECOIL *decoder; int result;
    int width, height;
    result = read_file(file, &data, &size);
    if (result != 0) return result;
    decoder = RECOIL_New();
    if (decoder == NULL) { free(data); return SPI_ERROR_ALLOCATE_MEMORY; }
    result = RECOIL_Decode(decoder, recoil_name(file), data, (int)size);
    if (!result) { RECOIL_Delete(decoder); free(data); return SPI_ERROR_BROKEN_DATA; }
    width = RECOIL_GetWidth(decoder); height = RECOIL_GetHeight(decoder);
    RECOIL_Delete(decoder); free(data);
    if (width <= 0 || height <= 0) return SPI_ERROR_BROKEN_DATA;
    SpiSetPictureInfo(info, width, height, 24, 0, 0, 0, 0, NULL);
    return 0;
}

/** Decode Recoil's top-down RGB pixels into a bottom-up 24-bit DIB. */
int GetImage(SPI_FILE *file, HANDLE *info_handle, HANDLE *bitmap_handle,
             SPIPROC progress, LONG_PTR callback_data)
{
    uint8_t *data; size_t size; RECOIL *decoder; int result;
    int width, height, x, y; int const *pixels;
    LPBITMAPINFO info; LPBYTE bits; DWORD stride;
    result = read_file(file, &data, &size);
    if (result != 0) return result;
    decoder = RECOIL_New();
    if (decoder == NULL) { free(data); return SPI_ERROR_ALLOCATE_MEMORY; }
    result = RECOIL_Decode(decoder, recoil_name(file), data, (int)size);
    free(data);
    if (!result) { RECOIL_Delete(decoder); return SPI_ERROR_BROKEN_DATA; }
    width = RECOIL_GetWidth(decoder); height = RECOIL_GetHeight(decoder);
    result = SpiInitBitmap((HLOCAL *)info_handle, &info, (HLOCAL *)bitmap_handle,
                           &bits, &stride, width, height, 24, 0, 0, 0);
    if (result != 0) { RECOIL_Delete(decoder); return result; }
    pixels = RECOIL_GetPixels(decoder);
    for (y = 0; y < height; ++y) {
        uint8_t *row = bits + (size_t)(height - 1 - y) * stride;
        for (x = 0; x < width; ++x) {
            int rgb = pixels[y * width + x];
            row[x * 3 + 0] = (uint8_t)rgb;
            row[x * 3 + 1] = (uint8_t)(rgb >> 8);
            row[x * 3 + 2] = (uint8_t)(rgb >> 16);
        }
    }
    RECOIL_Delete(decoder);
    SpiUnlockBuffer(info_handle); SpiUnlockBuffer(bitmap_handle);
    if (progress != NULL) progress(100, 100, callback_data);
    return 0;
}

