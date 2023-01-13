nv-video-info
=============

This project provides utilities to print out the capabilities of the NVDEC and
NVENC functions in nvidia GPUs.

NVDEC and NVENC
---------------

NVDEC and NVENC are nvidia's names for their GPUs dedicated video decoding and
encoding functions respectively. These allow, in theory, for video processing
to be done more quickly and efficiently than on a CPU.

More details about NVDEC and NVENC are can be found
[here](https://developer.nvidia.com/nvidia-video-codec-sdk).

`nvdecinfo` and `nvencinfo`
---------------------------

The two utilities provided by this project will print out the capabilities
reported by nvidia GPUs. Most, but not all, GPUs have both decoding and
encoding functionality.

Requirements
------------

* [nv-codec-headers](https://git.videolan.org/?p=ffmpeg/nv-codec-headers.git) >= 9.1.23.0
* nvidia GPU drivers installed on the system

Operating Systems
-----------------

I developed these utilities on Linux and have only tested them there. I believe
it is fairly simple to build them on Windows, and would happily accept any PRs
to make that work out-of-the-box.
