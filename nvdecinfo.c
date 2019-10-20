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

  const char *surface;
  switch (caps.nOutputFormatMask) {
  case 1 << cudaVideoSurfaceFormat_NV12:
      surface = "NV12";
      break;
  case 1 << cudaVideoSurfaceFormat_P016:
      surface = "P016";
      break;
  case 1 << cudaVideoSurfaceFormat_YUV444:
      surface = "YUV444P";
      break;
  case 1 << cudaVideoSurfaceFormat_YUV444_16Bit:
      surface = "YUV444P16";
      break;
  case (1 << cudaVideoSurfaceFormat_NV12) + (1 << cudaVideoSurfaceFormat_P016):
      surface = "P016, NV12";
      break;
  default:
      surface = "Unknown";
      break;
  }

  printf("%5s | %6s | %5d | %9d | %10d | %15s\n",
         codec, format, bit_depth, caps.nMaxWidth, caps.nMaxHeight, surface);

  return 0;
}

int main(int argc, char *argv[])
{
  CUcontext cuda_ctx;
  CUcontext dummy;

  cuda_load_functions(&cu, NULL);
  cuvid_load_functions(&cv, NULL);

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
    printf("-----------------------------------------------------------------\n");

    CHECK_CU(cu->cuCtxCreate(&cuda_ctx, CU_CTX_SCHED_BLOCKING_SYNC, dev));
    printf("Codec | Chroma | Depth | Max Width | Max Height | Surface Formats\n");
    printf("-----------------------------------------------------------------\n");
    for (int c = 0; c < cudaVideoCodec_NumCodecs; c++) {
      for (int f = 0; f < 4; f++) {
        for (int b = 8; b < 14; b += 2) {
          get_caps(c, f, b);
        }
      }
    }
    printf("-----------------------------------------------------------------\n\n");
    cu->cuCtxPopCurrent(&dummy);
  }

  return 0;
}
