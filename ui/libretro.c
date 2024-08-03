#include <libretro.h>

#include <stdio.h>
#include <string.h>
#include <libgen.h>

#include "qemu/datadir.h"
#include "qemu/osdep.h"
#include "qemu/module.h"
#include "qemu/main-loop.h"
#include "ui/console.h"
#include "ui/kbd-state.h"

int main(int argc, const char *argv[]);
void rcu_init(void);

static const char *game_path;
static const char *system_dir;
static pthread_t emu_thread;
static DisplaySurface *surface;
static QKbdState *kbd;

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
	info->valid_extensions = "txt|iso|img|qcow|qcow2";
	info->library_version = "0.1.0";
	info->library_name = "qemu";
}

void retro_get_system_av_info(struct retro_system_av_info *info)
{
	info->geometry.base_width = 720;
	info->geometry.base_height = 400;
	info->geometry.max_width = 720;
	info->geometry.max_height = 400;
	info->geometry.aspect_ratio = 0.0;
}

static void keyboard_event(bool down, unsigned keycode, uint32_t character,
			   uint16_t key_modifiers)
{
	QKeyCode qc;

	if (keycode >= RETROK_LAST) {
		return;
	}

	qc = key_map[keycode];

	if (!qc) {
		return;
	}

	printf("KEY %d %d %d\n", down, keycode, character);

	bql_lock();
	qkbd_state_key_event(kbd, qc, down);
	bql_unlock();
}

static retro_environment_t cb_env;

void retro_set_environment(retro_environment_t cb)
{
	cb_env = cb;

	cb_env(RETRO_ENVIRONMENT_SET_PIXEL_FORMAT,
	       (enum retro_pixel_format[]){ RETRO_PIXEL_FORMAT_XRGB8888 });
	cb_env(RETRO_ENVIRONMENT_SET_KEYBOARD_CALLBACK,
	       (struct retro_keyboard_callback[]){ {
		       keyboard_event,
	       } });
	cb_env(RETRO_ENVIRONMENT_SET_CONTROLLER_INFO,
	       (struct retro_controller_info[]){
		       {
			       .types =
				       (struct retro_controller_description[]){ {
					       .desc = "Keyboard",
					       .id = RETRO_DEVICE_KEYBOARD,
				       } },
			       .num_types = 1,
		       },
		       { 0 } });
	cb_env(RETRO_ENVIRONMENT_GET_SYSTEM_DIRECTORY, &system_dir);
}

static retro_video_refresh_t cb_video_refresh;

void retro_set_video_refresh(retro_video_refresh_t cb)
{
	cb_video_refresh = cb;
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
}

static void start_qemu_with_args(int argc, const char *argv[])
{
	rcu_init();
	main(argc, argv);
}

static void *emu_thread_fn(void *arg)
{
	char *game_dir = g_path_get_dirname(game_path);
	g_chdir(game_dir);
	g_free(game_dir);

	if (!system_dir) {
		return;
	}
	qemu_add_data_dir(g_build_filename(system_dir, "qemu", NULL));

	if (g_str_has_suffix(game_path, ".qemu_cmd_line")) {
		char *cmd_line = NULL;
		bool success =
			g_file_get_contents(game_path, &cmd_line, NULL, NULL);
		if (!success) {
			return NULL;
		}
		int argc;
		char **argv;
		success = g_shell_parse_argv(cmd_line, &argc, &argv, NULL);
		g_free(cmd_line);
		if (!success) {
			return NULL;
		}
		start_qemu_with_args(argc, (const char **)argv);
	} else if (g_str_has_suffix(game_path, ".iso")) {
		start_qemu_with_args(4, (const char *[]){ "libretro-qemu",
							  "-libretro", "-cdrom",
							  game_path, NULL });
	} else if (g_str_has_suffix(game_path, ".img") ||
		   g_str_has_suffix(game_path, ".qcow") ||
		   g_str_has_suffix(game_path, ".qcow2")) {
		start_qemu_with_args(3, (const char *[]){ "libretro-qemu",
							  "-libretro",
							  game_path, NULL });
	}
	return NULL;
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
	pthread_kill(emu_thread, SIGKILL);
	pthread_join(emu_thread, NULL);
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

static void gfx_update(DisplayChangeListener *dcl, int x, int y, int w, int h)
{
}

static void gfx_switch(DisplayChangeListener *dcl, DisplaySurface *new_surface)
{
	surface = new_surface;
}

static bool gfx_check_format(DisplayChangeListener *dcl,
			     pixman_format_code_t format)
{
	return format == PIXMAN_x8r8g8b8;
}

static void refresh(DisplayChangeListener *dcl)
{
	graphic_hw_update(dcl->con);
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
	dcl.con = qemu_console_lookup_by_index(0);
	kbd = qkbd_state_init(dcl.con);
	register_displaychangelistener(&dcl);
}

void retro_run(void)
{
	cb_input_poll();

	bql_lock();

	qemu_input_queue_rel(dcl.con, INPUT_AXIS_X,
			     cb_input_state(0, RETRO_DEVICE_MOUSE, 0,
					    RETRO_DEVICE_ID_MOUSE_X));
	qemu_input_queue_rel(dcl.con, INPUT_AXIS_Y,
			     cb_input_state(0, RETRO_DEVICE_MOUSE, 0,
					    RETRO_DEVICE_ID_MOUSE_Y));

#define BTN(q, r)                                                              \
	qemu_input_queue_btn(dcl.con, INPUT_BUTTON_##q,                        \
			     cb_input_state(0, RETRO_DEVICE_MOUSE, 0,          \
					    RETRO_DEVICE_ID_MOUSE_##r))
	BTN(LEFT, LEFT);
	BTN(RIGHT, RIGHT);
	BTN(MIDDLE, MIDDLE);
	BTN(WHEEL_UP, WHEELUP);
	BTN(WHEEL_DOWN, WHEELDOWN);
#undef BTN

	qemu_input_event_sync();

	bql_unlock();

	cb_video_refresh(surface_data(surface), surface_width(surface),
			 surface_height(surface), surface_stride(surface));
}

static QemuDisplay display = {
	.type = DISPLAY_TYPE_LIBRETRO,
	.init = display_init,
};

static void register_libretro(void)
{
	qemu_display_register(&display);
}

type_init(register_libretro);
