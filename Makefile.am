bin_PROGRAMS = nvdecinfo nvencinfo

nvdecinfo_CFLAGS = $(NVVIDEOINFO_CFLAGS) -Werror -Wall
nvdecinfo_LDADD = $(NVVIDEOINFO_LIBS) $(LIBADD_DLOPEN)
nvdecinfo_SOURCES = nvdecinfo.c

nvencinfo_CFLAGS = $(NVVIDEOINFO_CFLAGS)
nvencinfo_LDADD = $(NVVIDEOINFO_LIBS) $(LIBADD_DLOPEN)
nvencinfo_SOURCES = nvencinfo.c
