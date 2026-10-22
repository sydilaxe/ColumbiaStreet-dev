# Columbia Street — jungle break sequencer for Miyoo Mini Plus / OnionOS
#
#   make          cross-compile dist/ColumbiaStreet/columbia (ARMv7 hard-float, glibc 2.28)
#                 and fetch the curated samples into dist/ColumbiaStreet/breaks and bass
#   make test     build the host harness and render screenshots + audio into build/test
#   make zip      package dist/ColumbiaStreet as build/ColumbiaStreet.zip for copying to SD:/App/
#   make install SD=/run/media/$USER/<card>   copy the app onto a mounted SD card
#   make package CROSS=<prefix>   build with the OnionOS gcc toolchain (no zig) and zip it as
#                 build/ColumbiaStreet-miyoo.zip, laid out to unzip at the root of the SD card (CI does this)

ZIG    ?= $(HOME)/.local/opt/zig-aarch64-linux-0.14.1/zig
TARGET := arm-linux-gnueabihf.2.28
SDL_INC := third_party/SDL-1.2.15-headers
SDL_LIB := third_party/onion-lib/libSDL-1.2.so.0

SRC    := src/engine.c src/breaks.c src/ui.c src/gfx.c src/sync.c
HDR    := src/engine.h src/breaks.h src/ui.h src/gfx.h src/sync.h src/font_data.h
CFLAGS := -O2 -std=gnu11 -Wall -Wextra -fno-math-errno
APP    := dist/ColumbiaStreet
SAMPLES := $(APP)/breaks/AMEN_153bpm.wav

all: $(APP)/columbia $(APP)/icon.png samples

samples: $(SAMPLES)
$(SAMPLES): tools/fetch_samples.sh
	sh tools/fetch_samples.sh $(APP)
	@touch $@

src/font_data.h: tools/mkfont.py
	python3 tools/mkfont.py /usr/share/fonts/google-noto/NotoSansMono-Bold.ttf $@

$(APP)/icon.png: tools/mkicon.py
	python3 tools/mkicon.py $@

# Onion's MI_AO/MI_GFX-backed SDL 1.2 (LGPL), linked against but not committed
SDL_URL := https://raw.githubusercontent.com/OnionUI/Onion/main/static/build/.tmp_update/lib/parasyte/libSDL-1.2.so.0
$(SDL_LIB):
	mkdir -p $(dir $@)
	curl -sSL -o $@ $(SDL_URL) || wget -qO $@ $(SDL_URL) || (rm -f $@; false)

$(APP)/columbia: $(SRC) src/main_sdl.c $(HDR) $(SDL_LIB)
	$(ZIG) cc -target $(TARGET) -mcpu=cortex_a7 $(CFLAGS) -I$(SDL_INC) -o $@ \
		$(SRC) src/main_sdl.c $(SDL_LIB) -lm -lpthread -Wl,--allow-shlib-undefined -s

build/cs_test: $(SRC) src/main_test.c $(HDR)
	@mkdir -p build
	cc $(CFLAGS) -o $@ $(SRC) src/main_test.c -lm

test: build/cs_test $(SAMPLES)
	@mkdir -p build/test
	build/cs_test build/test $(APP)
	cd build/test && for f in *.ppm; do convert $$f $${f%.ppm}.png && rm $$f; done

zip: all
	@mkdir -p build
	rm -f build/ColumbiaStreet.zip
	cd dist && zip -r ../build/ColumbiaStreet.zip ColumbiaStreet -x "ColumbiaStreet/*.sav*" "ColumbiaStreet/log.txt"

install: all
	@test -n "$(SD)" || (echo "usage: make install SD=/path/to/mounted/sdcard" && exit 1)
	@test -d "$(SD)/App" || (echo "$(SD)/App not found — is this the OnionOS card?" && exit 1)
	mkdir -p "$(SD)/App/ColumbiaStreet"
	cp -r $(APP)/. "$(SD)/App/ColumbiaStreet/"
	sync

clean:
	rm -rf build $(APP)/columbia

.PHONY: all samples test zip install clean

# the OnionOS toolchain's gcc instead of zig, e.g. in the aemiii91/miyoomini-toolchain container:
#   make package CROSS=/opt/miyoomini-toolchain/usr/bin/arm-linux-gnueabihf-
CROSS ?= arm-linux-gnueabihf-
miyoo: $(SRC) src/main_sdl.c $(HDR) $(SDL_LIB)
	$(CROSS)gcc -marm -mcpu=cortex-a7 -mfpu=neon-vfpv4 -mfloat-abi=hard $(CFLAGS) -I$(SDL_INC) -o $(APP)/columbia \
		$(SRC) src/main_sdl.c $(SDL_LIB) -lm -lpthread -Wl,--allow-shlib-undefined -s

package: miyoo samples
	rm -rf build/sd build/ColumbiaStreet-miyoo.zip
	mkdir -p build/sd/App
	cp -r $(APP) build/sd/App/
	rm -f build/sd/App/ColumbiaStreet/*.sav* build/sd/App/ColumbiaStreet/log.txt
	chmod +x build/sd/App/ColumbiaStreet/launch.sh build/sd/App/ColumbiaStreet/columbia
	cd build/sd && zip -qr ../ColumbiaStreet-miyoo.zip App

.PHONY: miyoo package
