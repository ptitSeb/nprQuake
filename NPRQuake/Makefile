#RELEASE_CFLAGS=-Dstricmp=strcasecmp -fPIC -O2 -fomit-frame-pointer -fexpensive-optimizations -march=native
ifeq ($(ODROID),1)
 RELEASE_CFLAGS=-Dstricmp=strcasecmp -mcpu=cortex-a9 -mfpu=neon -mfloat-abi=hard -mno-unaligned-access -O3 -ffast-math -ftree-vectorize -fsigned-char -fdiagnostics-color=auto -fPIC -DPIC -fsingle-precision-constant -funroll-loops -fomit-frame-pointer -fexpensive-optimizations -std=gnu99
 DEBUG_CFLAGS=-Dstricmp=strcasecmp -g  -mcpu=cortex-a9 -mfpu=neon -mfloat-abi=hard -mno-unaligned-access -fsigned-char -fdiagnostics-color=auto -fsingle-precision-constant -fPIC -DPIC -std=gnu99
else
 RELEASE_CFLAGS=-Dstricmp=strcasecmp -mcpu=cortex-a8 -mfpu=neon -mfloat-abi=softfp -mno-unaligned-access -O3 -ffast-math -ftree-vectorize -fsigned-char -fdiagnostics-color=auto -fPIC -DPIC -fsingle-precision-constant -funroll-loops -fomit-frame-pointer -fexpensive-optimizations -std=gnu99
 DEBUG_CFLAGS=-Dstricmp=strcasecmp -g  -mcpu=cortex-a8 -mfpu=neon -mfloat-abi=softfp -mno-unaligned-access -fsigned-char -fdiagnostics-color=auto -fsingle-precision-constant -fPIC -DPIC -std=gnu99
endif

CFLAGS=$(RELEASE_CFLAGS)

DR_FLAGS=-DGLQUAKE -D_DLL_BUILD -shared

renderers=build/dr_default.so build/brush.so build/bprint.so build/sketch.so build/ainpr.so

release:
	make all CFLAGS="$(RELEASE_CFLAGS)" ODROID=$(ODROID)

debug:
	make all CFLAGS="$(DEBUG_CFLAGS)" ODROID=$(ODROID)

all: quake dynamic_renderers

quake:
	make -C NPRQuakeSrc build_release CFLAGS="$(CFLAGS)" ODROID=$(ODROID)

dynamic_renderers: $(renderers)

$(renderers): build/%.so: dynamic_r/%.c dynamic_r/*.h
	cd dynamic_r
	gcc $(CFLAGS) $(DR_FLAGS) -o $@ $<

clean:
	make -C NPRQuakeSrc clean
