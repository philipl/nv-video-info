/*
 * nvencinfo - enumerate nvenc capabilities of nvidia video devices
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
#include <sys/param.h>

#include <ffnvcodec/dynlink_loader.h>

static CudaFunctions *cu;
static NvencFunctions *nv;
static NV_ENCODE_API_FUNCTION_LIST nv_funcs;

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

static const struct {
    NVENCSTATUS nverr;
    int         averr;
    const char *desc;
} nvenc_errors[] = {
    { NV_ENC_SUCCESS,                       0, "success"                  },
    { NV_ENC_ERR_NO_ENCODE_DEVICE,         -1, "no encode device"         },
    { NV_ENC_ERR_UNSUPPORTED_DEVICE,       -1, "unsupported device"       },
    { NV_ENC_ERR_INVALID_ENCODERDEVICE,    -1, "invalid encoder device"   },
    { NV_ENC_ERR_INVALID_DEVICE,           -1, "invalid device"           },
    { NV_ENC_ERR_DEVICE_NOT_EXIST,         -1, "device does not exist"    },
    { NV_ENC_ERR_INVALID_PTR,              -1, "invalid ptr"              },
    { NV_ENC_ERR_INVALID_EVENT,            -1, "invalid event"            },
    { NV_ENC_ERR_INVALID_PARAM,            -1, "invalid param"            },
    { NV_ENC_ERR_INVALID_CALL,             -1, "invalid call"             },
    { NV_ENC_ERR_OUT_OF_MEMORY,            -1, "out of memory"            },
    { NV_ENC_ERR_ENCODER_NOT_INITIALIZED,  -1, "encoder not initialized"  },
    { NV_ENC_ERR_UNSUPPORTED_PARAM,        -1, "unsupported param"        },
    { NV_ENC_ERR_LOCK_BUSY,                -1, "lock busy"                },
    { NV_ENC_ERR_NOT_ENOUGH_BUFFER,        -1, "not enough buffer"        },
    { NV_ENC_ERR_INVALID_VERSION,          -1, "invalid version"          },
    { NV_ENC_ERR_MAP_FAILED,               -1, "map failed"               },
    { NV_ENC_ERR_NEED_MORE_INPUT,          -1, "need more input"          },
    { NV_ENC_ERR_ENCODER_BUSY,             -1, "encoder busy"             },
    { NV_ENC_ERR_EVENT_NOT_REGISTERD,      -1, "event not registered"     },
    { NV_ENC_ERR_GENERIC,                  -1, "generic error"            },
    { NV_ENC_ERR_INCOMPATIBLE_CLIENT_KEY,  -1, "incompatible client key"  },
    { NV_ENC_ERR_UNIMPLEMENTED,            -1, "unimplemented"            },
    { NV_ENC_ERR_RESOURCE_REGISTER_FAILED, -1, "resource register failed" },
    { NV_ENC_ERR_RESOURCE_NOT_REGISTERED,  -1, "resource not registered"  },
    { NV_ENC_ERR_RESOURCE_NOT_MAPPED,      -1, "resource not mapped"      },
};

#define FF_ARRAY_ELEMS(a) (sizeof(a) / sizeof((a)[0]))
static int nvenc_map_error(NVENCSTATUS err, const char **desc)
{
    int i;
    for (i = 0; i < FF_ARRAY_ELEMS(nvenc_errors); i++) {
        if (nvenc_errors[i].nverr == err) {
            if (desc)
                *desc = nvenc_errors[i].desc;
            return nvenc_errors[i].averr;
        }
    }
    if (desc)
        *desc = "unknown error";
    return -1;
}

static int nvenc_print_error(NVENCSTATUS err,
                             const char *error_string)
{
  const char *desc;
  int ret;
  ret = nvenc_map_error(err, &desc);
  printf("%s: %s (%d)\n", error_string, desc, err);
  return ret;
}

static int check_nv(NVENCSTATUS err, const char *func)
{
  const char *err_string;

  if (err == NV_ENC_SUCCESS) {
    return 0;
  }

  nvenc_map_error(err, &err_string);

  fprintf(stderr, "%s failed", func);
  if (err_string) {
    fprintf(stderr, " -> %s", err_string);
  }
  fprintf(stderr, "\n");

  return -1;
}

#define CHECK_NV(x) { int ret = check_nv((x), #x); if (ret != 0) { return ret; } }

typedef struct {
  NV_ENC_CAPS cap;
  const char *desc;
} cap_t;


static const cap_t nvenc_limits[] = {
  { NV_ENC_CAPS_WIDTH_MAX,                      "Maximum Width" },
  { NV_ENC_CAPS_HEIGHT_MAX,                     "Maximum Hight" },
  { NV_ENC_CAPS_MB_NUM_MAX,                     "Maximum Macroblocks/frame" },
  { NV_ENC_CAPS_MB_PER_SEC_MAX,                 "Maximum Macroblocks/second" },
  { NV_ENC_CAPS_LEVEL_MAX,                      "Max Encoding Level" },
  { NV_ENC_CAPS_LEVEL_MIN,                      "Min Encoding Level" },
  { NV_ENC_CAPS_NUM_MAX_BFRAMES,                "Max No. of B-Frames" },
  { NV_ENC_CAPS_NUM_MAX_LTR_FRAMES,             "Maxmimum LT Reference Frames" },
};

static const cap_t nvenc_caps[] = {
  { NV_ENC_CAPS_SUPPORTED_RATECONTROL_MODES,    "Supported Rate-Control Modes" },
  { NV_ENC_CAPS_SUPPORT_FIELD_ENCODING,         "Supports Field-Encoding" },
  { NV_ENC_CAPS_SUPPORT_MONOCHROME,             "Supports Monochrome" },
  { NV_ENC_CAPS_SUPPORT_FMO,                    "Supports FMO" },
  { NV_ENC_CAPS_SUPPORT_QPELMV,                 "Supports QPEL Motion Estimation" },
  { NV_ENC_CAPS_SUPPORT_BDIRECT_MODE,           "Supports BDirect Mode" },
  { NV_ENC_CAPS_SUPPORT_CABAC,                  "Supports CABAC" },
  { NV_ENC_CAPS_SUPPORT_ADAPTIVE_TRANSFORM,     "Supports Adaptive Transform" },
  { NV_ENC_CAPS_NUM_MAX_TEMPORAL_LAYERS,        "Supports Temporal Layers" },
  { NV_ENC_CAPS_SUPPORT_HIERARCHICAL_PFRAMES,   "Supports Hierarchical P-Frames" },
  { NV_ENC_CAPS_SUPPORT_HIERARCHICAL_BFRAMES,   "Supports Hierarchical B-Frames" },
  { NV_ENC_CAPS_SEPARATE_COLOUR_PLANE,          "Supports Separate Colour Planes" },
  { NV_ENC_CAPS_SUPPORT_TEMPORAL_SVC,           "Supports Temporal SVC" },
  { NV_ENC_CAPS_SUPPORT_DYN_RES_CHANGE,         "Supports Dynamic Resolution Change" },
  { NV_ENC_CAPS_SUPPORT_DYN_BITRATE_CHANGE,     "Supports Dynamic Bitrate Change" },
  { NV_ENC_CAPS_SUPPORT_DYN_FORCE_CONSTQP,      "Supports Dynamic Force Const-QP" },
  { NV_ENC_CAPS_SUPPORT_DYN_RCMODE_CHANGE,      "Supports Dynamic RC-Mode Change" },
  { NV_ENC_CAPS_SUPPORT_SUBFRAME_READBACK,      "Supports Sub-Frame Read-back" },
  { NV_ENC_CAPS_SUPPORT_CONSTRAINED_ENCODING,   "Supports Constrained Encoding" },
  { NV_ENC_CAPS_SUPPORT_INTRA_REFRESH,          "Supports Intra Refresh" },
  { NV_ENC_CAPS_SUPPORT_CUSTOM_VBV_BUF_SIZE,    "Supports Custom VBV Buffer Size" },
  { NV_ENC_CAPS_SUPPORT_DYNAMIC_SLICE_MODE,     "Supports Dynamic Slice Mode" },
  { NV_ENC_CAPS_SUPPORT_REF_PIC_INVALIDATION,   "Supports Ref Pic Invalidation" },
  { NV_ENC_CAPS_PREPROC_SUPPORT,                "Supports PreProcessing" },
  { NV_ENC_CAPS_ASYNC_ENCODE_SUPPORT,           "Supports Async Encoding" },
  { NV_ENC_CAPS_SUPPORT_YUV444_ENCODE,          "Supports YUV444 Encoding" },
  { NV_ENC_CAPS_SUPPORT_LOSSLESS_ENCODE,        "Supports Lossless Encoding" },
  { NV_ENC_CAPS_SUPPORT_SAO,                    "Supports SAO" },
  { NV_ENC_CAPS_SUPPORT_MEONLY_MODE,            "Supports ME-Only Mode" },
  { NV_ENC_CAPS_SUPPORT_LOOKAHEAD,              "Supports Lookahead Encoding" },
  { NV_ENC_CAPS_SUPPORT_TEMPORAL_AQ,            "Supports Temporal AQ" },
  { NV_ENC_CAPS_SUPPORT_10BIT_ENCODE,           "Supports 10-bit Encoding" },
  { NV_ENC_CAPS_SUPPORT_WEIGHTED_PREDICTION,    "Supports Weighted Prediction" },
#if 0
  /* This isn't really a capability. It's a runtime measurement. */
  { NV_ENC_CAPS_DYNAMIC_QUERY_ENCODER_CAPACITY, "Remaining Encoder Capacity" },
#endif
  { NV_ENC_CAPS_SUPPORT_BFRAME_REF_MODE,        "Supports B-Frames as References" },
  { NV_ENC_CAPS_SUPPORT_EMPHASIS_LEVEL_MAP,     "Supports Emphasis Level Map" },
};

static const struct {
  NV_ENC_BUFFER_FORMAT fmt;
  const char *desc;
} nvenc_formats[] = {
  { NV_ENC_BUFFER_FORMAT_NV12,         "NV12" },
  { NV_ENC_BUFFER_FORMAT_YV12,         "YV12" },
  { NV_ENC_BUFFER_FORMAT_IYUV,         "IYUV" },
  { NV_ENC_BUFFER_FORMAT_YUV444,       "YUV444" },
  { NV_ENC_BUFFER_FORMAT_YUV420_10BIT, "P010" },
  { NV_ENC_BUFFER_FORMAT_YUV444_10BIT, "YUV444P10" },
  { NV_ENC_BUFFER_FORMAT_ARGB,         "ARGB" },
  { NV_ENC_BUFFER_FORMAT_ARGB10,       "ARGB10" },
  { NV_ENC_BUFFER_FORMAT_AYUV,         "AYUV" },
  { NV_ENC_BUFFER_FORMAT_ABGR,         "ABGR" },
  { NV_ENC_BUFFER_FORMAT_ABGR10,       "ABGR10" },
};

static const struct {
  const GUID *guid;
  const char *desc;
} nvenc_profiles[] = {
  { &NV_ENC_CODEC_PROFILE_AUTOSELECT_GUID,        "Auto" },
  { &NV_ENC_H264_PROFILE_BASELINE_GUID,           "Baseline" },
  { &NV_ENC_H264_PROFILE_MAIN_GUID,               "Main" },
  { &NV_ENC_H264_PROFILE_HIGH_GUID,               "High" },
  { &NV_ENC_H264_PROFILE_HIGH_444_GUID,           "High444" },
  { &NV_ENC_H264_PROFILE_STEREO_GUID,             "MVC" },
  { &NV_ENC_H264_PROFILE_SVC_TEMPORAL_SCALABILTY, "SVC" },
  { &NV_ENC_H264_PROFILE_PROGRESSIVE_HIGH_GUID,   "Progressive High" },
  { &NV_ENC_H264_PROFILE_CONSTRAINED_HIGH_GUID,   "Constrained High" },
  { &NV_ENC_HEVC_PROFILE_MAIN_GUID,               "Main" },
  { &NV_ENC_HEVC_PROFILE_MAIN10_GUID,             "Main10" },
  { &NV_ENC_HEVC_PROFILE_FREXT_GUID,              "Main444" },
};


static const struct {
  const GUID *guid;
  const char *desc;
} nvenc_presets[] = {
  { &NV_ENC_PRESET_DEFAULT_GUID,             "default" },
  { &NV_ENC_PRESET_HP_GUID,                  "hp"},
  { &NV_ENC_PRESET_HQ_GUID,                  "hq"},
  { &NV_ENC_PRESET_BD_GUID,                  "bluray"},
  { &NV_ENC_PRESET_LOW_LATENCY_DEFAULT_GUID, "ll" },
  { &NV_ENC_PRESET_LOW_LATENCY_HQ_GUID,      "llhq" },
  { &NV_ENC_PRESET_LOW_LATENCY_HP_GUID,      "llhp" },
  { &NV_ENC_PRESET_LOSSLESS_DEFAULT_GUID,    "lossless" },
  { &NV_ENC_PRESET_LOSSLESS_HP_GUID,         "losslesshp" },
};


#define NVENCAPI_CHECK_VERSION(major, minor) \
    ((major) < NVENCAPI_MAJOR_VERSION || ((major) == NVENCAPI_MAJOR_VERSION && (minor) <= NVENCAPI_MINOR_VERSION))

static void nvenc_print_driver_requirement()
{
#if NVENCAPI_CHECK_VERSION(8, 1)
# if defined(_WIN32) || defined(__CYGWIN__)
    const char *minver = "390.77";
# else
    const char *minver = "390.25";
# endif
#else
# if defined(_WIN32) || defined(__CYGWIN__)
    const char *minver = "378.66";
# else
    const char *minver = "378.13";
# endif
#endif
    printf("The minimum required Nvidia driver for nvenc is %s or newer\n", minver);
}

static int nvenc_load_libraries()
{
  NVENCSTATUS err;
  uint32_t nvenc_max_ver;
  int ret;

  ret = cuda_load_functions(&cu, NULL);
  if (ret < 0)
    return ret;

  ret = nvenc_load_functions(&nv, NULL);
  if (ret < 0) {
    nvenc_print_driver_requirement();
    return ret;
  }

  CHECK_NV(nv->NvEncodeAPIGetMaxSupportedVersion(&nvenc_max_ver));

  printf("Loaded Nvenc version %d.%d\n", nvenc_max_ver >> 4, nvenc_max_ver & 0xf);

  if ((NVENCAPI_MAJOR_VERSION << 4 | NVENCAPI_MINOR_VERSION) > nvenc_max_ver) {
    printf("Driver does not support the required nvenc API version. "
           "Required: %d.%d Found: %d.%d\n",
           NVENCAPI_MAJOR_VERSION, NVENCAPI_MINOR_VERSION,
           nvenc_max_ver >> 4, nvenc_max_ver & 0xf);
    nvenc_print_driver_requirement();
    return -1;
  }

  nv_funcs.version = NV_ENCODE_API_FUNCTION_LIST_VER;

  CHECK_NV(nv->NvEncodeAPICreateInstance(&nv_funcs));

  printf("Nvenc initialized successfully\n");

  return 0;
}


static int print_formats(void *encoder, GUID *guids, int guid_count)
{
  NV_ENC_BUFFER_FORMAT *formats_for_guid = calloc(guid_count, sizeof(NV_ENC_BUFFER_FORMAT));
  if (!formats_for_guid) {
    return -1;
  }

  for (int i = 0; i < guid_count; i++) {
    int count = 0;
    CHECK_NV(nv_funcs.nvEncGetInputFormatCount(encoder, guids[i], &count));

    NV_ENC_BUFFER_FORMAT *formats = malloc(count * sizeof(NV_ENC_BUFFER_FORMAT));
    if (!formats) {
      return -1;
    }

    CHECK_NV(nv_funcs.nvEncGetInputFormats(encoder, guids[i], formats, count, &count));
    for (int j = 0; j < count; j++) {
      formats_for_guid[i] |= formats[j];
    }

    free(formats);
  }

  printf("        Input Buffer Formats       |          |          |\n");
  printf("----------------------------------------------------------\n");
  for (int i = 0; i < FF_ARRAY_ELEMS(nvenc_formats); i++) {
    printf("%34s |", nvenc_formats[i].desc);
    for (int j = 0; j < guid_count; j++) {
      printf("%9s |", (formats_for_guid[j] & nvenc_formats[i].fmt) ? "x" : ".");
    }
    printf("\n");
  }
  printf("----------------------------------------------------------\n");


  free(formats_for_guid);

  return 0;
}


static int get_profiles(void *encoder, GUID encodeGUID, const char **profiles, uint32_t *count)
{
  GUID guids[FF_ARRAY_ELEMS(nvenc_profiles)];
  CHECK_NV(nv_funcs.nvEncGetEncodeProfileGUIDs(encoder, encodeGUID, guids,
                                               FF_ARRAY_ELEMS(nvenc_profiles), count));

  for (int i = 0; i < *count; i++) {
    for (int j = 0; j < FF_ARRAY_ELEMS(nvenc_profiles); j++) {
      if (memcmp(&guids[i], nvenc_profiles[j].guid, sizeof (GUID)) == 0) {
        profiles[i] = nvenc_profiles[j].desc;
      }
    }
  }
  return 0;
}


static int print_profiles(void *encoder, GUID *guids, int count)
{
  printf("----------------------------------------------------------\n");
  printf("              Profiles             |          |          |\n");
  printf("----------------------------------------------------------\n");

  const char *profileGuids[count][FF_ARRAY_ELEMS(nvenc_profiles)];
  uint32_t profileCount[count];

  for (int i = 0; i < count; i++) {
    get_profiles(encoder, guids[i], profileGuids[i], &profileCount[i]);
  }

  uint32_t max = MAX(profileCount[0], profileCount[1]);
  for (int i = 0; i < max; i++) {
    printf("%34s |", "");
    for (int j = 0; j < count; j++) {
      if (i < profileCount[j]) {
        printf("%9s |", profileGuids[j][i]);
      } else {
        printf("%9s |", "");
      }
    }
    printf("\n");
  }

  return 0;
}


static int get_presets(void *encoder, GUID encodeGUID, const char **presets, uint32_t *count)
{
  GUID guids[FF_ARRAY_ELEMS(nvenc_presets)];
  CHECK_NV(nv_funcs.nvEncGetEncodePresetGUIDs(encoder, encodeGUID, guids,
                                               FF_ARRAY_ELEMS(nvenc_presets), count));

  for (int i = 0; i < *count; i++) {
    int matched = 0;
    for (int j = 0; j < FF_ARRAY_ELEMS(nvenc_presets); j++) {
      if (memcmp(&guids[i], nvenc_presets[j].guid, sizeof (GUID)) == 0) {
        presets[i] = nvenc_presets[j].desc;
        matched = 1;
      }
    }
    if (!matched) {
      presets[i] = "Unknown";
    }
  }
  return 0;
}


static int print_presets(void *encoder, GUID *guids, int count)
{
  printf("----------------------------------------------------------\n");
  printf("               Presets             |          |          |\n");
  printf("----------------------------------------------------------\n");

  const char *presetGuids[count][FF_ARRAY_ELEMS(nvenc_presets)];
  uint32_t presetCount[count];

  for (int i = 0; i < count; i++) {
    get_presets(encoder, guids[i], presetGuids[i], &presetCount[i]);
  }

  uint32_t max = MAX(presetCount[0], presetCount[1]);
  for (int i = 0; i < max; i++) {
    printf("%34s |", "");
    for (int j = 0; j < count; j++) {
      if (i < presetCount[j]) {
        printf("%9s |", presetGuids[j][i]);
      } else {
        printf("%9s |", "");
      }
    }
    printf("\n");
  }

  return 0;
}


static int get_cap(void *encoder, GUID *guid, NV_ENC_CAPS cap)
{
  NV_ENC_CAPS_PARAM params = { 0 };

  int val = 0;
  params.version = NV_ENC_CAPS_PARAM_VER;
  params.capsToQuery = cap;
  CHECK_NV(nv_funcs.nvEncGetEncodeCaps(encoder, *guid, &params, &val));

  return val;
}


static int print_caps(void *encoder, GUID *guids, int count)
{
  int ret;

  printf("               Limits              |          |          |\n");
  printf("----------------------------------------------------------\n");
  for (int i = 0; i < FF_ARRAY_ELEMS(nvenc_limits); i++) {
    printf("%34s |", nvenc_limits[i].desc);
    for (int j = 0; j < count; j++) {
      ret = get_cap(encoder, &guids[j], nvenc_limits[i].cap);
      printf("%9d |", ret);
    }
    printf("\n");
  }

  printf("----------------------------------------------------------\n");
  printf("            Capabilities           |          |          |\n");
  printf("----------------------------------------------------------\n");
  for (int i = 0; i < FF_ARRAY_ELEMS(nvenc_caps); i++) {
    printf("%34s |", nvenc_caps[i].desc);
    for (int j = 0; j < count; j++) {
      ret = get_cap(encoder, &guids[j], nvenc_caps[i].cap);
      printf("%9d |", ret);
    }
    printf("\n");
  }
}


static int print_codecs(void *encoder)
{
  int count = 0;
  CHECK_NV(nv_funcs.nvEncGetEncodeGUIDCount(encoder, &count));

  GUID *guids = malloc(count * sizeof(GUID));
  if (!guids) {
    return -1;
  }

  CHECK_NV(nv_funcs.nvEncGetEncodeGUIDs(encoder, guids, count, &count));

  printf("==========================================================\n");
  printf("                             Codec |");
  for (int i = 0; i < count; i++) {
    if (memcmp(&guids[i], &NV_ENC_CODEC_H264_GUID, 16) == 0) {
      printf("   H264   |");
    } else if (memcmp(&guids[i], &NV_ENC_CODEC_HEVC_GUID, 16) == 0) {
      printf("   HEVC   |");
    } else {
      printf(" Unknown  |");
    }
  }
  printf("\n==========================================================\n");
  CHECK_NV(print_formats(encoder, guids, count));
  CHECK_NV(print_caps(encoder, guids, count));
  CHECK_NV(print_profiles(encoder, guids, count));
  CHECK_NV(print_presets(encoder, guids, count));

  free(guids);

  return 0;
}


static int print_nvenc_capabilities(CUcontext *cuda_ctx)
{
  NV_ENC_OPEN_ENCODE_SESSION_EX_PARAMS params = { 0 };
  NVENCSTATUS ret;
  void *nvencoder;

  params.version    = NV_ENC_OPEN_ENCODE_SESSION_EX_PARAMS_VER;
  params.apiVersion = NVENCAPI_VERSION;
  params.device     = cuda_ctx;
  params.deviceType = NV_ENC_DEVICE_TYPE_CUDA;

  CHECK_NV(nv_funcs.nvEncOpenEncodeSessionEx(&params, &nvencoder));

  CHECK_NV(print_codecs(nvencoder));

  CHECK_NV(nv_funcs.nvEncDestroyEncoder(nvencoder));

  return 0;
}


int main(int argc, char *argv[])
{
  CUcontext cuda_ctx;
  CUcontext dummy;

  nvenc_load_libraries();

  CHECK_CU(cu->cuInit(0));
  int count;
  CHECK_CU(cu->cuDeviceGetCount(&count));

  for (int i = 0; i < count; i++) {
    CUdevice dev;
    CHECK_CU(cu->cuDeviceGet(&dev, i));

    char name[255];
    CHECK_CU(cu->cuDeviceGetName(name, 255, dev));
    printf("Device %d: %s\n", i, name);

    CHECK_CU(cu->cuCtxCreate(&cuda_ctx, CU_CTX_SCHED_BLOCKING_SYNC, dev));
    print_nvenc_capabilities(cuda_ctx);
    printf("==========================================================\n\n");
    cu->cuCtxPopCurrent(&dummy);
  }

  nvenc_free_functions(&nv);
  cuda_free_functions(&cu);

  return 0;
}
