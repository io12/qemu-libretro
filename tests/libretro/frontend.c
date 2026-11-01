/*
 * Minimal libretro frontend for testing the QEMU core headlessly.
 *
 * usage: frontend CORE CONTENT SYSTEM_DIR [options]
 *   --frames N         give up after N frames (default 3000)
 *   --until WxH        stop once the core has shown a WxH frame for 60 frames
 *   --pixel X,Y=RRGGBB require this pixel in the last frame (repeatable)
 *   --out FILE.ppm     save the last frame
 *
 * It behaves like RetroArch where that has caught bugs before: when the core
 * calls SET_SYSTEM_AV_INFO it redraws the last frame it was given, and at the
 * end it unloads the core and keeps running for a while, so threads the core
 * left behind crash the test instead of a user's frontend.
 *
 * Prints MODE/MESSAGE/FAIL lines and a final RESULT line; exits 0 on success.
 */
#include <stdarg.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "libretro.h"

#ifdef _WIN32
#include <windows.h>
#define lib_open(p) ((void *)LoadLibraryA(p))
#define lib_sym(h, s) ((void *)GetProcAddress((HMODULE)(h), s))
#define lib_close(h) FreeLibrary((HMODULE)(h))
#define sleep_ms(ms) Sleep(ms)
#else
#include <dlfcn.h>
#include <unistd.h>
#define lib_open(p) dlopen(p, RTLD_NOW | RTLD_LOCAL)
#define lib_sym(h, s) dlsym(h, s)
#define lib_close(h) dlclose(h)
#define sleep_ms(ms) usleep((ms) * 1000)
#endif

static const char *system_dir;
static bool shutdown_requested;
static int failures;
static struct retro_system_av_info av_info;

// The last frame the core gave us, like RetroArch's cached frame
static const void *last_data;
static unsigned last_w, last_h;
static size_t last_pitch;

static unsigned frame_no;
static unsigned mode_w, mode_h, mode_frames;

static void fail(const char *fmt, ...)
{
	va_list ap;
	va_start(ap, fmt);
	printf("FAIL: ");
	vprintf(fmt, ap);
	printf("\n");
	fflush(stdout);
	va_end(ap);
	failures++;
}

static void log_cb(enum retro_log_level level, const char *fmt, ...)
{
	va_list ap;
	va_start(ap, fmt);
	printf("LOG %d: ", level);
	vprintf(fmt, ap);
	fflush(stdout);
	va_end(ap);
}

// Read every byte of the frame, so a dangling or short buffer faults here
static uint8_t touch_frame(const void *data, unsigned h, size_t pitch)
{
	const volatile uint8_t *p = data;
	size_t size = (size_t)h * pitch;
	uint8_t sum = 0;
	for (size_t i = 0; i < size; i += 64) {
		sum += p[i];
	}
	return sum + p[size - 1];
}

static bool environment(unsigned cmd, void *data)
{
	switch (cmd) {
	case RETRO_ENVIRONMENT_GET_SYSTEM_DIRECTORY:
		*(const char **)data = system_dir;
		return true;
	case RETRO_ENVIRONMENT_GET_LOG_INTERFACE:
		((struct retro_log_callback *)data)->log = log_cb;
		return true;
	case RETRO_ENVIRONMENT_GET_CAN_DUPE:
		*(bool *)data = true;
		return true;
	case RETRO_ENVIRONMENT_SET_MESSAGE:
		printf("MESSAGE: %s\n", ((struct retro_message *)data)->msg);
		fflush(stdout);
		return true;
	case RETRO_ENVIRONMENT_SET_MESSAGE_EXT:
		printf("MESSAGE: %s\n", ((struct retro_message_ext *)data)->msg);
		fflush(stdout);
		return true;
	case RETRO_ENVIRONMENT_SET_PIXEL_FORMAT:
		if (*(enum retro_pixel_format *)data != RETRO_PIXEL_FORMAT_XRGB8888) {
			fail("unsupported pixel format %d",
			     *(enum retro_pixel_format *)data);
			return false;
		}
		return true;
	case RETRO_ENVIRONMENT_SET_SYSTEM_AV_INFO:
		av_info = *(struct retro_system_av_info *)data;
		// RetroArch reinitializes video here and redraws its cached frame
		if (last_data) {
			touch_frame(last_data, last_h, last_pitch);
		}
		return true;
	case RETRO_ENVIRONMENT_SET_GEOMETRY:
		av_info.geometry = *(struct retro_game_geometry *)data;
		return true;
	case RETRO_ENVIRONMENT_SHUTDOWN:
		shutdown_requested = true;
		return true;
	default:
		return false;
	}
}

static void video_refresh(const void *data, unsigned w, unsigned h, size_t pitch)
{
	frame_no++;
	if (!data) {
		return; // dupe
	}
	if (w > av_info.geometry.max_width || h > av_info.geometry.max_height) {
		fail("frame %ux%u exceeds max geometry %ux%u", w, h,
		     av_info.geometry.max_width, av_info.geometry.max_height);
	}
	if (pitch < (size_t)w * 4) {
		fail("pitch %zu too small for width %u", pitch, w);
	}
	touch_frame(data, h, pitch);
	if (w != mode_w || h != mode_h) {
		printf("MODE %ux%u at frame %u\n", w, h, frame_no);
		fflush(stdout);
		mode_w = w;
		mode_h = h;
		mode_frames = 0;
	}
	mode_frames++;
	last_data = data;
	last_w = w;
	last_h = h;
	last_pitch = pitch;
}

static void input_poll(void)
{
}

static int16_t input_state(unsigned port, unsigned device, unsigned index,
			   unsigned id)
{
	return 0;
}

static void audio_sample(int16_t left, int16_t right)
{
}

static size_t audio_sample_batch(const int16_t *data, size_t frames)
{
	return frames;
}

static void write_ppm(const char *path, const uint32_t *px, unsigned w,
		      unsigned h)
{
	FILE *f = fopen(path, "wb");
	if (!f) {
		fail("can't write %s", path);
		return;
	}
	fprintf(f, "P6\n%u %u\n255\n", w, h);
	for (size_t i = 0; i < (size_t)w * h; i++) {
		uint8_t rgb[3] = { px[i] >> 16, px[i] >> 8, px[i] };
		fwrite(rgb, 1, 3, f);
	}
	fclose(f);
}

#define LOAD(name)                                                    \
	name##_t name = (name##_t)lib_sym(core, #name);               \
	if (!name) {                                                  \
		printf("RESULT error: core lacks %s\n", #name);      \
		return 2;                                             \
	}

typedef void (*retro_set_environment_t)(retro_environment_t);
typedef void (*retro_set_video_refresh_t)(retro_video_refresh_t);
typedef void (*retro_set_input_poll_t)(retro_input_poll_t);
typedef void (*retro_set_input_state_t)(retro_input_state_t);
typedef void (*retro_set_audio_sample_t)(retro_audio_sample_t);
typedef void (*retro_set_audio_sample_batch_t)(retro_audio_sample_batch_t);
typedef void (*retro_init_t)(void);
typedef void (*retro_deinit_t)(void);
typedef void (*retro_get_system_av_info_t)(struct retro_system_av_info *);
typedef bool (*retro_load_game_t)(const struct retro_game_info *);
typedef void (*retro_unload_game_t)(void);
typedef void (*retro_run_t)(void);

int main(int argc, char **argv)
{
	if (argc < 4) {
		fprintf(stderr, "usage: %s CORE CONTENT SYSTEM_DIR [options]\n",
			argv[0]);
		return 2;
	}
	const char *core_path = argv[1];
	const char *content = argv[2];
	system_dir = argv[3];
	unsigned max_frames = 3000, until_w = 0, until_h = 0;
	struct {
		unsigned x, y, rgb;
	} pixels[16];
	unsigned num_pixels = 0;
	const char *out = NULL;
	for (int i = 4; i < argc; i++) {
		if (!strcmp(argv[i], "--frames") && i + 1 < argc) {
			max_frames = strtoul(argv[++i], NULL, 0);
		} else if (!strcmp(argv[i], "--until") && i + 1 < argc &&
			   sscanf(argv[++i], "%ux%u", &until_w, &until_h) == 2) {
		} else if (!strcmp(argv[i], "--pixel") && i + 1 < argc &&
			   num_pixels < 16 &&
			   sscanf(argv[++i], "%u,%u=%x", &pixels[num_pixels].x,
				  &pixels[num_pixels].y,
				  &pixels[num_pixels].rgb) == 3) {
			num_pixels++;
		} else if (!strcmp(argv[i], "--out") && i + 1 < argc) {
			out = argv[++i];
		} else {
			fprintf(stderr, "bad option %s\n", argv[i]);
			return 2;
		}
	}

	void *core = lib_open(core_path);
	if (!core) {
		printf("RESULT error: can't load %s\n", core_path);
		return 2;
	}
	LOAD(retro_set_environment)
	LOAD(retro_set_video_refresh)
	LOAD(retro_set_input_poll)
	LOAD(retro_set_input_state)
	LOAD(retro_set_audio_sample)
	LOAD(retro_set_audio_sample_batch)
	LOAD(retro_init)
	LOAD(retro_deinit)
	LOAD(retro_get_system_av_info)
	LOAD(retro_load_game)
	LOAD(retro_unload_game)
	LOAD(retro_run)

	retro_set_environment(environment);
	retro_set_video_refresh(video_refresh);
	retro_set_input_poll(input_poll);
	retro_set_input_state(input_state);
	retro_set_audio_sample(audio_sample);
	retro_set_audio_sample_batch(audio_sample_batch);
	retro_init();

	struct retro_game_info game = { .path = content };
	if (!retro_load_game(&game)) {
		printf("RESULT error: retro_load_game failed\n");
		return 2;
	}
	retro_get_system_av_info(&av_info);

	bool reached = false;
	while (frame_no < max_frames && !shutdown_requested) {
		retro_run();
		if (until_w && mode_w == until_w && mode_h == until_h &&
		    mode_frames >= 60) {
			reached = true;
			break;
		}
	}

	// Keep our own copy; the core's buffer goes away when it's unloaded
	uint32_t *last = NULL;
	if (last_data) {
		last = malloc((size_t)last_w * last_h * 4);
		for (unsigned y = 0; y < last_h; y++) {
			memcpy(&last[(size_t)y * last_w],
			       (const uint8_t *)last_data + y * last_pitch,
			       (size_t)last_w * 4);
		}
	}

	retro_unload_game();
	retro_deinit();
	lib_close(core);
	printf("UNLOADED\n");
	fflush(stdout);
	// Anything the core left running now runs unmapped code
	sleep_ms(3000);

	if (until_w && !reached) {
		fail("never showed %ux%u for 60 frames (last mode %ux%u)",
		     until_w, until_h, mode_w, mode_h);
	}
	for (unsigned i = 0; i < num_pixels; i++) {
		unsigned x = pixels[i].x, y = pixels[i].y;
		if (!last || x >= last_w || y >= last_h) {
			fail("no pixel at %u,%u", x, y);
		} else if ((last[(size_t)y * last_w + x] & 0xffffff) !=
			   pixels[i].rgb) {
			fail("pixel %u,%u is %06x, expected %06x", x, y,
			     last[(size_t)y * last_w + x] & 0xffffff,
			     pixels[i].rgb);
		}
	}
	if (out && last) {
		write_ppm(out, last, last_w, last_h);
	}
	printf("RESULT %s frames=%u mode=%ux%u shutdown=%d\n",
	       failures ? "fail" : "ok", frame_no, mode_w, mode_h,
	       shutdown_requested);
	fflush(stdout);
	return failures ? 1 : 0;
}
