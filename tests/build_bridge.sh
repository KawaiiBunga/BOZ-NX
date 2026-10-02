#!/bin/bash
set -eu
mkdir -p test-artifacts
gcc -std=c11 -Wall -Wextra -Werror -Isource tests/test_split_snapshot.c source/split_snapshot.c -o test-artifacts/test_split_snapshot
test-artifacts/test_split_snapshot
ARM=/opt/devkitpro/devkitARM/bin/arm-none-eabi
"$ARM-gcc" -march=armv8-a -mfpu=neon-fp-armv8 -mfloat-abi=softfp -c source/native_bridge.S -o test-artifacts/bridge.o
"$ARM-gcc" -march=armv8-a -c tests/bridge_mock.S -o test-artifacts/mock.o
"$ARM-ld" -Ttext=0x100000 -Tdata=0x120000 -e native_veneers test-artifacts/bridge.o test-artifacts/mock.o -o test-artifacts/bridge.elf
"$ARM-nm" test-artifacts/bridge.elf > test-artifacts/bridge.nm
"$ARM-objcopy" -O binary test-artifacts/bridge.elf test-artifacts/bridge.bin
gcc -std=c11 -Wall -Wextra -Itests/shim -Isource tests/test_cursor.c source/cursor.c -lm -o test-artifacts/test_cursor
test-artifacts/test_cursor
gcc -std=c11 -Wall -Wextra -ffunction-sections -Iportlibs32/include -Isource tests/test_gl_cache.c source/gl_cache.c -Wl,--gc-sections -o test-artifacts/test_gl_cache
test-artifacts/test_gl_cache
gcc -std=c11 -Wall -Wextra -Isource tests/test_options.c source/input_tuning.c source/display_options.c -lm -o test-artifacts/test_options
test-artifacts/test_options
gcc -std=c11 -Wall -Wextra -DEGL_NO_X11 -Iportlibs32/include -Isource tests/test_upscale.c source/upscale.c source/display_options.c -o test-artifacts/test_upscale
test-artifacts/test_upscale
gcc -std=c11 -Wall -Wextra -Isource tests/test_camera_hud.c source/camera_fov.c source/camera_controller.c source/hud_controls.c source/guest.c -lm -o test-artifacts/test_camera_hud
test-artifacts/test_camera_hud
gcc -std=c11 -Wall -Wextra -Werror -Isource tests/test_split_probe.c source/split_probe.c source/net_trace.c source/guest.c -o test-artifacts/test_split_probe
test-artifacts/test_split_probe
gcc -std=c11 -D_DEFAULT_SOURCE -Wall -Wextra -Itests/net_shim -Isource tests/test_net_host.c tests/net_host_stubs.c source/net.c source/net_trace.c source/split_probe.c source/guest.c -o test-artifacts/test_net_host
test-artifacts/test_net_host
for file in tests/camera_mock.c source/camera_controller.c source/camera_fov.c source/guest.c; do
    name="$(basename "${file%.c}")"
    "$ARM-gcc" -O2 -ffp-contract=off -march=armv8-a -mfpu=neon-fp-armv8 -mfloat-abi=softfp -Isource -c "$file" -o "test-artifacts/$name.o"
done
"$ARM-gcc" -march=armv8-a -c tests/camera_math_mock.S -o test-artifacts/camera_math.o
"$ARM-ld" -Ttext=0x100000 -Tdata=0x140000 -e native_veneers test-artifacts/bridge.o test-artifacts/camera_mock.o test-artifacts/camera_controller.o test-artifacts/camera_fov.o test-artifacts/guest.o test-artifacts/camera_math.o -o test-artifacts/camera.elf
"$ARM-nm" test-artifacts/camera.elf > test-artifacts/camera.nm
"$ARM-objcopy" -O binary test-artifacts/camera.elf test-artifacts/camera.bin

for file in tests/render_mock.c source/split_probe.c; do
    name="$(basename "${file%.c}")"
    "$ARM-gcc" -O2 -ffunction-sections -fdata-sections -march=armv8-a -mfpu=neon-fp-armv8 -mfloat-abi=softfp -Isource -c "$file" -o "test-artifacts/$name.o"
done
"$ARM-gcc" -march=armv8-a -c tests/render_tail.S -o test-artifacts/render_tail.o
"$ARM-ld" --gc-sections --undefined=render_tail -Ttext=0x100000 -Tdata=0x140000 -e native_veneers test-artifacts/bridge.o test-artifacts/render_mock.o test-artifacts/split_probe.o test-artifacts/render_tail.o -o test-artifacts/render.elf
"$ARM-nm" test-artifacts/render.elf > test-artifacts/render.nm
"$ARM-objcopy" -O binary test-artifacts/render.elf test-artifacts/render.bin
gcc -Wall -Wextra -Isource tests/load_image.c source/s3e_loader.c -o test-artifacts/load_image
