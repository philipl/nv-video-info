#include <stdio.h>

#include <ffnvcodec/dynlink_loader.h>

static CudaFunctions *cu;
static CuvidFunctions *cv;

static int get_caps(cudaVideoCodec codec_type,
                    cudaVideoChromaFormat chroma_format,
                    unsigned int bit_depth)
{
  CUresult err;
  CUVIDDECODECAPS caps;
  caps.eCodecType = codec_type;
  caps.eChromaFormat = chroma_format;
  caps.nBitDepthMinus8 = bit_depth - 8;

  err = cv->cuvidGetDecoderCaps(&caps);
  if (err != 0) {
    return err;
  }

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

  printf("%5s | %6s | %5d | %9d | %10d\n",
         codec, format, bit_depth, caps.nMaxWidth, caps.nMaxHeight);

  return 0;
}

int main(void) {
  CUcontext cuda_ctx;
  CUcontext dummy;
  CUresult err;
  CUdevice dev;

  cuda_load_functions(&cu, NULL);
  cuvid_load_functions(&cv, NULL);

  err = cu->cuInit(0);
  err = cu->cuDeviceGet(&dev, 0);
  err = cu->cuCtxCreate(&cuda_ctx, CU_CTX_SCHED_BLOCKING_SYNC, dev);

  printf("Codec | Chroma | Depth | Max Width | Max Height\n");
  printf("-----------------------------------------------\n");
  for (int c = 0; c < cudaVideoCodec_NumCodecs; c++) {
    for (int f = 0; f < 4; f++) {
      for (int b = 8; b < 14; b+=2) {
        err = get_caps(c, f, b);
      }
    }
  }

  cu->cuCtxPopCurrent(&dummy);

  return 0;
}
