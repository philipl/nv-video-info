/*
 * nvdecinfo - enumerate nvdec capabilities of nvidia video devices
 * Copyright (c) 2018 Philip Langdale <philipl@overt.org>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 */

#include <stdio.h>
#include <string.h>

#include <ffnvcodec/dynlink_loader.h>

static CudaFunctions *cu;
static CuvidFunctions *cv;

static int check_cu(CUresult err, const char *func)
{
  const char *err_name;
  const char *err_string;

  if (err == CUDA_SUCCESS) {
    return 0;
  }

  cu->cuGetErrorName(err, &err_name);
  cu->cuGetErrorString(err, &err_string);

  fprintf(stderr, "%s failed", func);
  if (err_name && err_string) {
    fprintf(stderr, " -> %s: %s", err_name, err_string);
  }
  fprintf(stderr, "\n");

  return -1;
}

#define CHECK_CU(x) { int ret = check_cu((x), #x); if (ret != 0) { return ret; } }

static int get_caps(cudaVideoCodec codec_type,
                    cudaVideoChromaFormat chroma_format,
                    unsigned int bit_depth)
{
  CUVIDDECODECAPS caps;
  caps.eCodecType = codec_type;
  caps.eChromaFormat = chroma_format;
  caps.nBitDepthMinus8 = bit_depth - 8;

  CHECK_CU(cv->cuvidGetDecoderCaps(&caps));

  if (!caps.bIsSupported) {
    return 0;
  }

  const char *codec;
  switch (codec_type) {
  case cudaVideoCodec_MPEG1:
    codec = "MPEG1";
    break;
  case cudaVideoCodec_MPEG2:
    codec = "MPEG2";
    break;
  case cudaVideoCodec_MPEG4:
    codec = "MPEG4";
    break;
  case cudaVideoCodec_VC1:
    codec = "VC1";
    break;
  case cudaVideoCodec_H264:
    codec = "H264";
    break;
  case cudaVideoCodec_JPEG:
    codec = "MJPEG";
    break;
  case cudaVideoCodec_H264_SVC:
    codec = "H264 SVC";
    break;
  case cudaVideoCodec_H264_MVC:
    codec = "H264 MVC";
    break;
  case cudaVideoCodec_HEVC:
    codec = "HEVC";
    break;
  case cudaVideoCodec_VP8:
    codec = "VP8";
    break;
  case cudaVideoCodec_VP9:
    codec = "VP9";
    break;
#if NVDECAPI_MAJOR_VERSION > 10
  case cudaVideoCodec_AV1:
    codec = "AV1";
    break;
#endif
  default:
    codec = "Unknown";
    break;
  }

  const char *format;
  switch (chroma_format) {
  case cudaVideoChromaFormat_Monochrome:
    format = "400";
    break;
  case cudaVideoChromaFormat_420:
    format = "420";
    break;
  case cudaVideoChromaFormat_422:
    format = "422";
    break;
  case cudaVideoChromaFormat_444:
    format = "444";
    break;
  }

   char surface[256];
   char opaque[256];
   surface[0] = '\0';
   opaque[0] = '\0';

   if (caps.nOutputFormatMask & (1 << cudaVideoSurfaceFormat_NV12)) {
       strcat(surface, ", NV12");
   }
   if (caps.nOutputFormatMask & (1 << cudaVideoSurfaceFormat_P016)) {
       strcat(surface, ", P016");
   }
   if (caps.nOutputFormatMask & (1 << cudaVideoSurfaceFormat_YUV444)) {
       strcat(surface, ", YUV444P");
   }
   if (caps.nOutputFormatMask & (1 << cudaVideoSurfaceFormat_YUV444_16Bit)) {
       strcat(surface, ", YUV444P16");
   }
#if NVDECAPI_MAJOR_VERSION > 12
   if (caps.nOutputFormatMask & (1 << cudaVideoSurfaceFormat_NV16)) {
       strcat(surface, ", NV16");
   }
   if (caps.nOutputFormatMask & (1 << cudaVideoSurfaceFormat_P216)) {
       strcat(surface, ", P216");
   }
#if NVDECAPI_MAJOR_VERSION > 13 || \
    (NVDECAPI_MAJOR_VERSION == 13 && NVDECAPI_MINOR_VERSION > 0)
   if (caps.nOutputFormatMask & (1 << cudaVideoSurfaceFormat_NV12_Opaque)) {
       strcat(opaque, ", NV12");
   }
   if (caps.nOutputFormatMask & (1 << cudaVideoSurfaceFormat_P016_Opaque)) {
       strcat(opaque, ", P016");
   }
   if (caps.nOutputFormatMask & (1 << cudaVideoSurfaceFormat_YUV444_Opaque)) {
       strcat(opaque, ", YUV444P");
   }
   if (caps.nOutputFormatMask & (1 << cudaVideoSurfaceFormat_YUV444_16Bit_Opaque)) {
       strcat(opaque, ", YUV444P16");
   }
   if (caps.nOutputFormatMask & (1 << cudaVideoSurfaceFormat_NV16_Opaque)) {
       strcat(opaque, ", NV16");
   }
   if (caps.nOutputFormatMask & (1 << cudaVideoSurfaceFormat_P216_Opaque)) {
       strcat(opaque, ", P216");
   }
#endif
#endif

   if (surface[0] == '\0') {
       strcpy(surface, "Unknown");
   } else {
       /* Skip leading ", " separator */
       memmove(surface, surface + 2, strlen(surface + 2) + 1);
   }

   if (opaque[0] == '\0') {
       strcpy(opaque, "-");
   } else {
       /* Skip leading ", " separator */
       memmove(opaque, opaque + 2, strlen(opaque + 2) + 1);
   }

   printf("%5s | %6s | %5d | %9d | %10d | %9d | %10d | %8d | %15s | %14s\n",
          codec, format, bit_depth, caps.nMinWidth, caps.nMinHeight,
          caps.nMaxWidth, caps.nMaxHeight, caps.nMaxMBCount, surface, opaque);

  return 0;
}

int main(int argc, char *argv[])
{
  CUcontext cuda_ctx;
  CUcontext dummy;
  int ret;

  ret = cuda_load_functions(&cu, NULL);
  if (ret != 0) {
    fprintf(stderr, "Failed to load CUDA functions.\n");
    return -1;
  }
  ret = cuvid_load_functions(&cv, NULL);
  if (ret != 0) {
    fprintf(stderr, "Failed to load NVDEC functions.\n");
    return -1;
  }

  if (!cv->cuvidGetDecoderCaps) {
    fprintf(stderr,
            "The current nvidia driver is too old to perform a capability check.\n"
            "The minimum required driver version is %s\n",
#if defined(_WIN32) || defined(__CYGWIN__)
            "378.66");
#else
            "378.13");
#endif
    return -1;
  }

  CHECK_CU(cu->cuInit(0));
  int count;
  CHECK_CU(cu->cuDeviceGetCount(&count));

  for (int i = 0; i < count; i++) {
    CUdevice dev;
    CHECK_CU(cu->cuDeviceGet(&dev, i));

    char name[255];
    CHECK_CU(cu->cuDeviceGetName(name, 255, dev));
    printf("Device %d: %s\n", i, name);
    printf("----------------------------------------------------------------------------------------------------------------------\n");

    CHECK_CU(cu->cuCtxCreate(&cuda_ctx, CU_CTX_SCHED_BLOCKING_SYNC, dev));
    printf("Codec | Chroma | Depth | Min Width | Min Height | Max Width | Max Height |  Max MBs | Surface Formats | Opaque Formats\n");
    printf("----------------------------------------------------------------------------------------------------------------------\n");
    for (int c = 0; c < cudaVideoCodec_NumCodecs; c++) {
      for (int f = 0; f < 4; f++) {
        for (int b = 8; b < 14; b += 2) {
          get_caps(c, f, b);
        }
      }
    }
    printf("----------------------------------------------------------------------------------------------------------------------\n\n");
    cu->cuCtxPopCurrent(&dummy);
  }

  return 0;
}
