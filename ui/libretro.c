#include <libretro.h>

#include <stdio.h>
#include <string.h>
#include <libgen.h>
#include <pthread.h>
#include <glib/gstdio.h>

#include "retro-gen.h"
#include "qemu/datadir.h"
#include "qemu/osdep.h"
#include "qemu/module.h"
#include "qemu/main-loop.h"
#include "qemu/error-report.h"
#include "qemu-main.h"
#include "sysemu/sysemu.h"
#include "sysemu/runstate.h"
#include "ui/console.h"
#include "ui/kbd-state.h"
#include "audio/audio.h"
#include "audio/audio_int.h"

#define QEMU_CMD_PREFIX "qemu-system-"
#define DEFAULT_ARCH "x86_64"
#define QEMU_CMD                                                               \
	(QEMU_CMD_PREFIX DEFAULT_ARCH), "-libretro", "-audiodev",              \
		"libretro,id=snd0", "-machine", "pcspk-audiodev=snd0",         \
		"-device", "AC97,audiodev=snd0"

void rcu_init(void);

static const char *game_path;
static const char *system_dir;
static const char *target_arch;
static pthread_t emu_thread;
static DisplaySurface *surface;
static QKbdState *kbd;
static bool exited = false;

static HWVoiceOut *hw_voice_out = NULL;
static pthread_mutex_t hw_voice_out_mutex = PTHREAD_MUTEX_INITIALIZER;

// Input
#define KEY_EVENT_QUEUE_LEN 32
struct key_event {
	bool down;
	QKeyCode key;
};
static struct key_event key_event_queue[KEY_EVENT_QUEUE_LEN];
static size_t num_pending_keys = 0;
static int mouse_dx, mouse_dy;
static bool buttons_down[INPUT_BUTTON__MAX];

// Synchronization
static bool emu_waiting = false;
static bool main_waiting = true;
static pthread_cond_t emu_cv = PTHREAD_COND_INITIALIZER;
static pthread_cond_t main_cv = PTHREAD_COND_INITIALIZER;
static pthread_mutex_t emu_mutex = PTHREAD_MUTEX_INITIALIZER;
static pthread_mutex_t main_mutex = PTHREAD_MUTEX_INITIALIZER;

static const QKeyCode key_map[RETROK_LAST] = {
#define KEY(q, r) [RETROK_##r] = Q_KEY_CODE_##q
	KEY(SHIFT, LSHIFT),
	KEY(SHIFT_R, RSHIFT),
	KEY(ALT, LALT),
	KEY(ALT_R, RALT),
	KEY(CTRL, LCTRL),
	KEY(CTRL_R, RCTRL),
	KEY(MENU, MENU),
	KEY(ESC, ESCAPE),
	KEY(1, 1),
	KEY(2, 2),
	KEY(3, 3),
	KEY(4, 4),
	KEY(5, 5),
	KEY(6, 6),
	KEY(7, 7),
	KEY(8, 8),
	KEY(9, 9),
	KEY(0, 0),
	KEY(MINUS, MINUS),
	KEY(EQUAL, EQUALS),
	KEY(BACKSPACE, BACKSPACE),
	KEY(TAB, TAB),
	KEY(Q, q),
	KEY(W, w),
	KEY(E, e),
	KEY(R, r),
	KEY(T, t),
	KEY(Y, y),
	KEY(U, u),
	KEY(I, i),
	KEY(O, o),
	KEY(P, p),
	KEY(BRACKET_LEFT, LEFTBRACKET),
	KEY(BRACKET_RIGHT, RIGHTBRACKET),
	KEY(RET, RETURN),
	KEY(A, a),
	KEY(S, s),
	KEY(D, d),
	KEY(F, f),
	KEY(G, g),
	KEY(H, h),
	KEY(J, j),
	KEY(K, k),
	KEY(L, l),
	KEY(SEMICOLON, SEMICOLON),
	KEY(APOSTROPHE, QUOTE),
	KEY(GRAVE_ACCENT, BACKQUOTE),
	KEY(BACKSLASH, BACKSLASH),
	KEY(Z, z),
	KEY(X, x),
	KEY(C, c),
	KEY(V, v),
	KEY(B, b),
	KEY(N, n),
	KEY(M, m),
	KEY(COMMA, COMMA),
	KEY(DOT, PERIOD),
	KEY(SLASH, SLASH),
	KEY(ASTERISK, ASTERISK),
	KEY(SPC, SPACE),
	KEY(CAPS_LOCK, CAPSLOCK),
	KEY(F1, F1),
	KEY(F2, F2),
	KEY(F3, F3),
	KEY(F4, F4),
	KEY(F5, F5),
	KEY(F6, F6),
	KEY(F7, F7),
	KEY(F8, F8),
	KEY(F9, F9),
	KEY(F10, F10),
	KEY(NUM_LOCK, NUMLOCK),
	KEY(SCROLL_LOCK, SCROLLOCK),
	KEY(KP_DIVIDE, KP_DIVIDE),
	KEY(KP_MULTIPLY, KP_MULTIPLY),
	KEY(KP_SUBTRACT, KP_MINUS),
	KEY(KP_ADD, KP_PLUS),
	KEY(KP_ENTER, KP_ENTER),
	KEY(KP_DECIMAL, KP_PERIOD),
	KEY(SYSRQ, SYSREQ),
	KEY(KP_0, KP0),
	KEY(KP_1, KP1),
	KEY(KP_2, KP2),
	KEY(KP_3, KP3),
	KEY(KP_4, KP4),
	KEY(KP_5, KP5),
	KEY(KP_6, KP6),
	KEY(KP_7, KP7),
	KEY(KP_8, KP8),
	KEY(KP_9, KP9),
	KEY(LESS, LESS),
	KEY(F11, F11),
	KEY(F12, F12),
	KEY(PRINT, PRINT),
	KEY(HOME, HOME),
	KEY(PGUP, PAGEUP),
	KEY(PGDN, PAGEDOWN),
	KEY(END, END),
	KEY(LEFT, LEFT),
	KEY(UP, UP),
	KEY(DOWN, DOWN),
	KEY(RIGHT, RIGHT),
	KEY(INSERT, INSERT),
	KEY(DELETE, DELETE),
	KEY(UNDO, UNDO),
	KEY(HELP, HELP),
	KEY(META_L, LMETA),
	KEY(META_R, RMETA),
	KEY(COMPOSE, COMPOSE),
	KEY(PAUSE, PAUSE),
	KEY(KP_EQUALS, KP_EQUALS),
	KEY(POWER, POWER),
	KEY(AUDIONEXT, MEDIA_NEXT),
	KEY(AUDIOPREV, MEDIA_PREV),
	KEY(AUDIOSTOP, MEDIA_STOP),
	KEY(AUDIOPLAY, MEDIA_PLAY_PAUSE),
	KEY(AUDIOMUTE, VOLUME_MUTE),
	KEY(VOLUMEUP, VOLUME_UP),
	KEY(VOLUMEDOWN, VOLUME_DOWN),
	KEY(MAIL, LAUNCH_MAIL),
	KEY(AC_HOME, BROWSER_HOME),
	KEY(AC_BACK, BROWSER_BACK),
	KEY(AC_FORWARD, BROWSER_FORWARD),
	KEY(AC_REFRESH, BROWSER_REFRESH),
	KEY(AC_BOOKMARKS, BROWSER_FAVORITES),
	KEY(F13, F13),
	KEY(F14, F14),
	KEY(F15, F15)
#undef KEY
};

static void switch_to_emu_thread(void)
{
	pthread_mutex_lock(&emu_mutex);
	emu_waiting = false;
	pthread_mutex_unlock(&emu_mutex);

	pthread_mutex_lock(&main_mutex);
	pthread_cond_signal(&emu_cv);
	main_waiting = true;
	while (main_waiting) {
		pthread_cond_wait(&main_cv, &main_mutex);
	}
	pthread_mutex_unlock(&main_mutex);
}

static void switch_to_main_thread(void)
{
	pthread_mutex_lock(&main_mutex);
	main_waiting = false;
	pthread_mutex_unlock(&main_mutex);

	pthread_mutex_lock(&emu_mutex);
	pthread_cond_signal(&main_cv);
	emu_waiting = true;
	while (emu_waiting) {
		pthread_cond_wait(&emu_cv, &emu_mutex);
	}
	pthread_mutex_unlock(&emu_mutex);
}

// Wait for emu thread to exit
static void join_emu_thread(void)
{
	// Resume thread
	pthread_mutex_lock(&emu_mutex);
	emu_waiting = false;
	pthread_mutex_unlock(&emu_mutex);
	pthread_cond_signal(&emu_cv);

	// Wait for thread to exit
	pthread_join(emu_thread, NULL);
}

static void emu_thread_exit(void)
{
	exited = true;

	if (target_arch && arch_is_valid(target_arch)) {
		CALL_QEMU_FUNC(qemu_kill_threads);
	}

	pthread_mutex_lock(&main_mutex);
	main_waiting = false;
	pthread_mutex_unlock(&main_mutex);
	pthread_cond_signal(&main_cv);

	pthread_exit(NULL);
}

void retro_init(void)
{
}

void retro_deinit(void)
{
}

unsigned retro_api_version(void)
{
	return 1;
}

void retro_get_system_info(struct retro_system_info *info)
{
	memset(info, 0, sizeof(*info));
	info->need_fullpath = true;
	info->valid_extensions = "qemu_cmd_line|iso|img|qcow|qcow2";
	info->library_version = "0.1.0";
	info->library_name = "qemu";
}

static pthread_mutex_t av_info_lock = PTHREAD_MUTEX_INITIALIZER;
static bool changed_av_info = false;
static struct retro_system_av_info av_info = {
	.geometry = {
		.base_width = 100,
		.base_height = 100,
		.max_width = 100,
		.max_height = 100,
		.aspect_ratio = 0.0,
	},
	.timing = {
		.fps = 0.0,
		.sample_rate = 0.0,
	},
};

void retro_get_system_av_info(struct retro_system_av_info *info)
{
	*info = av_info;
}

static void keyboard_event(bool down, unsigned keycode, uint32_t character,
			   uint16_t key_modifiers)
{
	g_assert(num_pending_keys <= KEY_EVENT_QUEUE_LEN);
	if (num_pending_keys == KEY_EVENT_QUEUE_LEN) {
		return;
	}
	if (keycode >= RETROK_LAST) {
		return;
	}
	QKeyCode qc = key_map[keycode];
	if (!qc) {
		return;
	}
	key_event_queue[num_pending_keys++] = (struct key_event){
		.down = down,
		.key = qc,
	};
}

static retro_audio_sample_t cb_audio_sample;

void retro_set_audio_sample(retro_audio_sample_t cb)
{
	cb_audio_sample = cb;
}

static retro_audio_sample_batch_t cb_audio_sample_batch;

void retro_set_audio_sample_batch(retro_audio_sample_batch_t cb)
{
	cb_audio_sample_batch = cb;
}

static void audio_callback(void)
{
	pthread_mutex_lock(&hw_voice_out_mutex);

	if (!hw_voice_out) {
		goto out;
	}

	while (hw_voice_out->pending_emul) {
		size_t start = audio_ring_posb(hw_voice_out->pos_emul,
					       hw_voice_out->pending_emul,
					       hw_voice_out->size_emul);
		g_assert(start < hw_voice_out->size_emul);
		size_t write_len = MIN(hw_voice_out->pending_emul,
				       hw_voice_out->size_emul - start);

		size_t written = cb_audio_sample_batch(
			hw_voice_out->buf_emul + start, write_len / 4);
		g_assert(hw_voice_out->pending_emul >= written * 4);
		hw_voice_out->pending_emul -= written * 4;
		g_assert(write_len == written * 4);
	}

out:
	pthread_mutex_unlock(&hw_voice_out_mutex);
}

static retro_environment_t cb_env;

void retro_set_environment(retro_environment_t cb)
{
	cb_env = cb;

	cb(RETRO_ENVIRONMENT_SET_PIXEL_FORMAT,
	   &(enum retro_pixel_format){ RETRO_PIXEL_FORMAT_XRGB8888 });
	cb(RETRO_ENVIRONMENT_SET_KEYBOARD_CALLBACK,
	   &(struct retro_keyboard_callback){ keyboard_event });
	cb(RETRO_ENVIRONMENT_SET_CONTROLLER_INFO,
	   (struct retro_controller_info[]){
		   {
			   .types = (struct retro_controller_description[]){ {
				   .desc = "Keyboard",
				   .id = RETRO_DEVICE_KEYBOARD,
			   } },
			   .num_types = 1,
		   },
		   { 0 } });
	cb(RETRO_ENVIRONMENT_GET_SYSTEM_DIRECTORY, &system_dir);

	cb(RETRO_ENVIRONMENT_SET_AUDIO_CALLBACK,
	   &(struct retro_audio_callback){
		   .callback = audio_callback,
		   .set_state = NULL,
	   });
}

static retro_video_refresh_t cb_video_refresh;

void retro_set_video_refresh(retro_video_refresh_t cb)
{
	cb_video_refresh = cb;
}

static retro_input_poll_t cb_input_poll;

void retro_set_input_poll(retro_input_poll_t cb)
{
	cb_input_poll = cb;
}

static retro_input_state_t cb_input_state;

void retro_set_input_state(retro_input_state_t cb)
{
	cb_input_state = cb;
}

void retro_set_controller_port_device(unsigned port, unsigned device)
{
}

void retro_reset(void)
{
	if (!exited) {
		CALL_QEMU_FUNC(qemu_system_reset_request,
			       SHUTDOWN_CAUSE_HOST_UI);
	}
}

static void *audio_init(Audiodev *dev, Error **errp)
{
	return dev;
}

static void audio_fini(void *opaque)
{
}

static int audio_init_out(HWVoiceOut *hw, struct audsettings *as,
			  void *drv_opaque)
{
	g_assert(as->fmt == AUDIO_FORMAT_S16);
	g_assert(as->nchannels == 2);

	pthread_mutex_lock(&av_info_lock);
	av_info.timing.sample_rate = as->freq;
	changed_av_info = true;
	pthread_mutex_unlock(&av_info_lock);

	CALL_QEMU_FUNC(audio_pcm_init_info, &hw->info, as);

	hw->samples = 1024;

	pthread_mutex_lock(&hw_voice_out_mutex);
	g_assert(!hw_voice_out);
	hw_voice_out = hw;
	pthread_mutex_unlock(&hw_voice_out_mutex);

	return 0;
}

static void audio_fini_out(HWVoiceOut *hw)
{
	pthread_mutex_lock(&hw_voice_out_mutex);
	g_assert(hw_voice_out == hw);
	hw_voice_out = NULL;
	pthread_mutex_unlock(&hw_voice_out_mutex);
}

static void audio_enable_out(HWVoiceOut *hw, bool enable)
{
}

static size_t audio_write(HWVoiceOut *hw, void *buf, size_t size)
{
	pthread_mutex_lock(&hw_voice_out_mutex);
	size_t ret = CALL_QEMU_FUNC(audio_generic_write, hw, buf, size);
	pthread_mutex_unlock(&hw_voice_out_mutex);
	return ret;
}

static size_t audio_buffer_get_free(HWVoiceOut *hw)
{
	pthread_mutex_lock(&hw_voice_out_mutex);
	size_t ret = CALL_QEMU_FUNC(audio_generic_buffer_get_free, hw);
	pthread_mutex_unlock(&hw_voice_out_mutex);
	return ret;
}

static void *audio_get_buffer_out(HWVoiceOut *hw, size_t *size)
{
	pthread_mutex_lock(&hw_voice_out_mutex);
	void *ret = CALL_QEMU_FUNC(audio_generic_get_buffer_out, hw, size);
	pthread_mutex_unlock(&hw_voice_out_mutex);
	return ret;
}

static size_t audio_put_buffer_out(HWVoiceOut *hw, void *buf, size_t size)
{
	pthread_mutex_lock(&hw_voice_out_mutex);
	size_t ret =
		CALL_QEMU_FUNC(audio_generic_put_buffer_out, hw, buf, size);
	pthread_mutex_unlock(&hw_voice_out_mutex);
	return ret;
}

static void gfx_update(DisplayChangeListener *dcl, int x, int y, int w, int h)
{
}

static void gfx_switch(DisplayChangeListener *dcl, DisplaySurface *new_surface)
{
	int w = surface_width(new_surface);
	int h = surface_height(new_surface);
	bool changed_resolution = !surface || !(surface_width(surface) == w &&
						surface_height(surface) == h);
	if (changed_resolution) {
		pthread_mutex_lock(&av_info_lock);
		av_info.geometry.base_width = av_info.geometry.max_width = w;
		av_info.geometry.base_height = av_info.geometry.max_height = h;
		changed_av_info = true;
		pthread_mutex_unlock(&av_info_lock);
	}
	surface = new_surface;
}

static bool gfx_check_format(DisplayChangeListener *dcl,
			     pixman_format_code_t format)
{
	return format == PIXMAN_x8r8g8b8;
}

static void refresh(DisplayChangeListener *dcl)
{
	CALL_QEMU_FUNC(graphic_hw_update, dcl->con);

	switch_to_main_thread();

	// Flush keyboard event queue
	for (size_t i = 0; i < num_pending_keys; i++) {
		CALL_QEMU_FUNC(qkbd_state_key_event, kbd,
			       key_event_queue[i].key, key_event_queue[i].down);
	}
	num_pending_keys = 0;

	// Update mouse
	CALL_QEMU_FUNC(qemu_input_queue_rel, dcl->con, INPUT_AXIS_X, mouse_dx);
	CALL_QEMU_FUNC(qemu_input_queue_rel, dcl->con, INPUT_AXIS_Y, mouse_dy);

	// Update buttons
	for (size_t i = 0; i < INPUT_BUTTON__MAX; i++) {
		CALL_QEMU_FUNC(qemu_input_queue_btn, dcl->con, i,
			       buttons_down[i]);
	}

	CALL_QEMU_FUNC(qemu_input_event_sync);
}

static DisplayChangeListener dcl = {
	.ops = (DisplayChangeListenerOps[]){ {
		.dpy_name = "libretro",
		.dpy_gfx_update = gfx_update,
		.dpy_gfx_switch = gfx_switch,
		.dpy_gfx_check_format = gfx_check_format,
		.dpy_refresh = refresh,
	} },
};

static void display_init(DisplayState *ds, DisplayOptions *o)
{
	dcl.con = CALL_QEMU_FUNC(qemu_console_lookup_by_index, 0);
	kbd = CALL_QEMU_FUNC(qkbd_state_init, dcl.con);
	CALL_QEMU_FUNC(register_displaychangelistener, &dcl);
}

static QemuDisplay display = {
	.type = DISPLAY_TYPE_LIBRETRO,
	.init = display_init,
};

static struct audio_pcm_ops pcm_ops = {
	.init_out = audio_init_out,
	.fini_out = audio_fini_out,
	.enable_out = audio_enable_out,
	.write = audio_write,
	.buffer_get_free = audio_buffer_get_free,
	.get_buffer_out = audio_get_buffer_out,
	.put_buffer_out = audio_put_buffer_out,
};

static struct audio_driver libretro_audio_driver = {
	.name = "libretro",
	.descr = "libretro https://www.libretro.com/",
	.init = audio_init,
	.fini = audio_fini,
	.pcm_ops = &pcm_ops,
	.max_voices_out = 1,
	.max_voices_in = 0,
	.voice_size_out = sizeof(HWVoiceOut),
	.voice_size_in = 0,
};

static void register_libretro(void)
{
	CALL_QEMU_FUNC(qemu_display_register, &display);
	CALL_QEMU_FUNC(audio_driver_register, &libretro_audio_driver);
}

void vreport(report_type type, const char *fmt, va_list ap)
{
	unsigned duration = 5000; // 5 seconds
	enum retro_log_level level;
	switch (type) {
	case REPORT_TYPE_WARNING:
		level = RETRO_LOG_WARN;
		break;
	case REPORT_TYPE_INFO:
		level = RETRO_LOG_INFO;
		break;
	default:
		level = RETRO_LOG_ERROR;
		// I can't find a way to make it permanent, so instead display for a full day
		duration = 1000 * 60 * 60 * 24;
		break;
	}

	char *msg = g_strdup_vprintf(fmt, ap);

	fprintf(stderr, "%s\n", msg);

	cb_env(RETRO_ENVIRONMENT_SET_MESSAGE_EXT,
	       &(struct retro_message_ext){
		       .msg = msg,
		       .duration = duration,
		       .priority = 0,
		       .level = level,
		       .target = RETRO_MESSAGE_TARGET_ALL,
		       .type = RETRO_MESSAGE_TYPE_NOTIFICATION,
	       });

	g_free(msg);

	if (level == RETRO_LOG_ERROR) {
		emu_thread_exit();
	}
}

G_GNUC_PRINTF(1, 2)
static void early_error_report(const char *fmt, ...)
{
	va_list ap;

	va_start(ap, fmt);
	vreport(REPORT_TYPE_ERROR, fmt, ap);
	va_end(ap);
}

static void start_qemu_with_args(const char *argv[])
{
	int argc = 0;
	while (argv[argc]) {
		argc++;
	}

	if (!system_dir) {
		early_error_report("failed finding libretro system directory");
		return;
	}
	CALL_QEMU_FUNC(qemu_add_data_dir,
		       g_build_filename(system_dir, "qemu", NULL));

	CALL_QEMU_FUNC(register_module_init, register_libretro,
		       MODULE_INIT_QOM);
	CALL_QEMU_FUNC(rcu_init);
	CALL_QEMU_FUNC(qemu_init, argc, (char **)argv);
	CALL_QEMU_FUNC(qemu_default_main);

	emu_thread_exit();
}

size_t retro_serialize_size(void)
{
	return 0;
}

bool retro_serialize(void *data, size_t size)
{
	return false;
}

bool retro_unserialize(const void *data, size_t size)
{
	return false;
}

void retro_cheat_reset(void)
{
}

void retro_cheat_set(unsigned index, bool enabled, const char *code)
{
}

static void *emu_thread_fn(void *arg)
{
	char *game_dir = g_path_get_dirname(game_path);
	g_chdir(game_dir);
	g_free(game_dir);

	if (g_str_has_suffix(game_path, ".qemu_cmd_line")) {
		char *cmd_line = NULL;
		GError *error;
		bool success =
			g_file_get_contents(game_path, &cmd_line, NULL, &error);
		if (!success) {
			early_error_report("failed reading file '%s':\n%s",
					   game_path, error->message);
			return NULL;
		}
		char **argv;
		success = g_shell_parse_argv(cmd_line, NULL, &argv, &error);
		g_free(cmd_line);
		if (!success) {
			early_error_report("failed parsing file '%s':\n%s",
					   game_path, error->message);
			return NULL;
		}
		if (!argv[0]) {
			early_error_report("empty command in file '%s'",
					   game_path);
			return NULL;
		}
		if (!g_str_has_prefix(argv[0], QEMU_CMD_PREFIX)) {
			early_error_report(
				"command must be of the form " QEMU_CMD_PREFIX
				"ARCH, not '%s': '%s'",
				argv[0], game_path);
			return NULL;
		}
		target_arch = &argv[0][strlen(QEMU_CMD_PREFIX)];
		start_qemu_with_args((const char **)argv);
	} else if (g_str_has_suffix(game_path, ".iso")) {
		target_arch = DEFAULT_ARCH;
		start_qemu_with_args((const char *[]){ QEMU_CMD, "-cdrom",
						       game_path, NULL });
	} else {
		target_arch = DEFAULT_ARCH;
		start_qemu_with_args(
			(const char *[]){ QEMU_CMD, game_path, NULL });
	}
	return NULL;
}

bool retro_load_game(const struct retro_game_info *game)
{
	if (!game) {
		return false;
	}

	game_path = game->path;
	pthread_create(&emu_thread, NULL, emu_thread_fn, NULL);
	return true;
}

bool retro_load_game_special(unsigned game_type,
			     const struct retro_game_info *info,
			     size_t num_info)
{
	return false;
}

void retro_unload_game(void)
{
	if (!exited) {
		// Request shutdown
		CALL_QEMU_FUNC(qemu_system_shutdown_request,
			       SHUTDOWN_CAUSE_HOST_UI);
	}
	join_emu_thread();
}

unsigned retro_get_region(void)
{
	return RETRO_REGION_NTSC;
}

void *retro_get_memory_data(unsigned id)
{
	return NULL;
}

size_t retro_get_memory_size(unsigned id)
{
	return 0;
}

void retro_run(void)
{
	cb_input_poll();

	mouse_dx = cb_input_state(0, RETRO_DEVICE_MOUSE, 0,
				  RETRO_DEVICE_ID_MOUSE_X);
	mouse_dy = cb_input_state(0, RETRO_DEVICE_MOUSE, 0,
				  RETRO_DEVICE_ID_MOUSE_Y);

#define BTN(q, r)                                                              \
	buttons_down[INPUT_BUTTON_##q] = cb_input_state(                       \
		0, RETRO_DEVICE_MOUSE, 0, RETRO_DEVICE_ID_MOUSE_##r)
	BTN(LEFT, LEFT);
	BTN(RIGHT, RIGHT);
	BTN(MIDDLE, MIDDLE);
	BTN(WHEEL_UP, WHEELUP);
	BTN(WHEEL_DOWN, WHEELDOWN);
#undef BTN

	if (exited) {
		join_emu_thread();
		cb_env(RETRO_ENVIRONMENT_SHUTDOWN, NULL);
		return;
	}

	switch_to_emu_thread();

	if (exited) {
		join_emu_thread();
		cb_env(RETRO_ENVIRONMENT_SHUTDOWN, NULL);
		return;
	}

	int w = surface_width(surface);
	int h = surface_height(surface);

	pthread_mutex_lock(&av_info_lock);
	if (changed_av_info) {
		changed_av_info = false;
		cb_env(RETRO_ENVIRONMENT_SET_SYSTEM_AV_INFO, &av_info);
	}
	pthread_mutex_unlock(&av_info_lock);

	cb_video_refresh(surface_data(surface), w, h, surface_stride(surface));
}
