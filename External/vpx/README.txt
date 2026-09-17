The public decoder headers of libvpx v1.14.0 (https://github.com/webmproject/libvpx), under the BSD
licence in LICENSE next to this file. Only the headers are here; the library itself is whatever
libvpx the machine has, found by CMake, see Graphics/VideoPlayer.cpp.

Keeping them here means a machine with libvpx installed but no development package (very common: a
media player or browser pulls the runtime in) still builds video prints. libvpx checks the ABI
version its caller was built against in vpx_codec_dec_init_ver, so a libvpx too different from these
headers refuses to start cleanly instead of misbehaving, and video prints turn themselves off.
The decoder's ABI version has been 12 since libvpx 1.11 (the encoder's is what keeps changing), so
these headers work against any libvpx from 1.11 up, which is every one a supported distro ships.

Updating them: copy vpx/*.h and LICENSE from the release you want. Nothing else in libvpx is needed.
