# opencv-ffmpeg-nvjmi-mpp
FFmpeg calls Jetson/RK3588 MPP hardware decoding. The `open` and `read` functions of `VideoCapture` have been rewritten to support H.264/H.265.

Reference repository: https://github.com/fan-chao/nvjmi
The dynamic library files of `nvjmi` must be compiled first.

The `.so` files in this repository are based on JetPack 4.6.2.

RK3588-MPP is located in the `opencv-mpp` branch.

For RK3588, MPP and FFmpeg need to be compiled. Refer to the repository: https://github.com/nyanmisaka/ffmpeg-rockchip
