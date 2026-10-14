/* Here be a small dragon.
 * Not as large as the monster 
 * in src/swindle.c
 * but you get the idea */

#include "parser.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <time.h>
#include <unistd.h>
#include <signal.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <sys/inotify.h>
#include <linux/input-event-codes.h>
#include <xkbcommon/xkbcommon.h>
#include <wayland-server-core.h>

/* Lua */
#include <lua.h>
#include <lauxlib.h>
#include <lualib.h>

/* wlroots */
#include <wlr/util/log.h>
#include <wlr/types/wlr_keyboard.h>
#include <libinput.h>

/* Forward declarations from src/swindle.c  */
extern struct wl_list mons;   
extern struct wl_list clients; 
void arrange(void *m);       

/* Internal watcher state */

typedef struct {
	int                  inotify_fd;
	int                  watch_fd;
	char                 path[1024];
	config_reload_cb     cb;
	void                *userdata;
	struct wl_event_source *event_src;
} WatchState;

/* Helpers */

static void
lua_log_error(lua_State *L, const char *context)
{
	fprintf(stderr, "swindle config [%s]: %s\n",
	        context, lua_tostring(L, -1));
	lua_pop(L, 1);
}

/* Get a string field from the table at the top of the stack
 * Writes into buf and returns false and leaves buf untouched on failure
 */

static bool
lua_get_string(lua_State *L, const char *key, char *buf, size_t bufsz)
{
	lua_getfield(L, -1, key);
	bool ok = lua_isstring(L, -1);
	if (ok)
		strncpy(buf, lua_tostring(L, -1), bufsz - 1);
	lua_pop(L, 1);
	return ok;
}

/* Get an integer field. It falls back to `def` if missing or wrong type */

static int
lua_get_int(lua_State *L, const char *key, int def)
{
	lua_getfield(L, -1, key);
	int val = lua_isnumber(L, -1) ? (int)lua_tointeger(L, -1) : def;
	lua_pop(L, 1);
	return val;
}

static double
lua_get_double(lua_State *L, const char *key, double def)
{
	lua_getfield(L, -1, key);
	double val = lua_isnumber(L, -1) ? lua_tonumber(L, -1) : def;
	lua_pop(L, 1);
	return val;
}

static bool
lua_get_bool(lua_State *L, const char *key, bool def)
{
	lua_getfield(L, -1, key);
	bool val = lua_isboolean(L, -1) ? lua_toboolean(L, -1) : def;
	lua_pop(L, 1);
	return val;
}

/* Tristate boolean: 1 = true, 0 = false, -1 = missing/neither key present.
 * Checks key first, then alias. Stack-balanced in all paths. */
static int
lua_get_bool_tristate(lua_State *L, const char *key, const char *alias)
{
	int ret = -1;
	lua_getfield(L, -1, key);
	if (lua_isboolean(L, -1)) {
		ret = lua_toboolean(L, -1) ? 1 : 0;
		lua_pop(L, 1);
		return ret;
	}
	lua_pop(L, 1);
	if (alias) {
		lua_getfield(L, -1, alias);
		if (lua_isboolean(L, -1))
			ret = lua_toboolean(L, -1) ? 1 : 0;
		lua_pop(L, 1);
	}
	return ret;
}

static void
parse_color_hex(uint32_t hex, float out[4])
{
	out[0] = ((hex >> 24) & 0xFF) / 255.0f;
	out[1] = ((hex >> 16) & 0xFF) / 255.0f;
	out[2] = ((hex >>  8) & 0xFF) / 255.0f;
	out[3] = ( hex        & 0xFF) / 255.0f;
}

static void
lua_get_color(lua_State *L, const char *key, float out[4],
              float r, float g, float b, float a)
{
	lua_getfield(L, -1, key);
	if (lua_isnumber(L, -1))
		parse_color_hex((uint32_t)lua_tointeger(L, -1), out);
	else {
		out[0] = r; out[1] = g; out[2] = b; out[3] = a;
	}
	lua_pop(L, 1);
}

static uint32_t
parse_modifier(const char *name)
{
	if (!strcmp(name, "shift")) return WLR_MODIFIER_SHIFT;
	if (!strcmp(name, "ctrl"))  return WLR_MODIFIER_CTRL;
	if (!strcmp(name, "alt"))   return WLR_MODIFIER_ALT;
	if (!strcmp(name, "logo"))  return WLR_MODIFIER_LOGO;
	if (!strcmp(name, "mod2"))  return WLR_MODIFIER_MOD2;
	if (!strcmp(name, "mod3"))  return WLR_MODIFIER_MOD3;
	if (!strcmp(name, "mod5"))  return WLR_MODIFIER_MOD5;
	fprintf(stderr, "swindle config: unknown modifier '%s', ignoring\n", name);
	return 0;
}

/* Section parsers */

static void
parse_appearance(lua_State *L, CfgAppearance *a)
{
	lua_getglobal(L, "appearance");
	if (!lua_istable(L, -1)) {
		fprintf(stderr, "swindle config: 'appearance' table missing' \n");
		lua_pop(L, 1);
		/* defaults */
		a->inner_border_px = 1;
		a->outer_border_px = 1;
		a->gaps       = 10;
		a->smart_gaps = false;
		parse_color_hex(0x222222ff, a->root_color);
		parse_color_hex(0x00000000, a->inner_border_color);
		parse_color_hex(0x00000000, a->outer_border_color);
		parse_color_hex(0xe8e8e8ff, a->inner_focus_color);
		parse_color_hex(0xe8e8e8ff, a->outer_focus_color);
		parse_color_hex(0xff0000ff, a->inner_urgent_color);
		parse_color_hex(0xff0000ff, a->outer_urgent_color);
		a->fullscreen_bg[3] = 1.0f;
		return;
	}

	a->inner_border_px = (unsigned int)lua_get_int(L, "inner_border_px",
		lua_get_int(L, "border_px", 1));
	a->outer_border_px = (unsigned int)lua_get_int(L, "outer_border_px",
		lua_get_int(L, "border_px", 1));
	a->gaps       = (unsigned int)lua_get_int(L, "gaps",       10);
	a->smart_gaps = lua_get_bool(L, "smart_gaps", false);

	/* New inner_/outer_ keys win; legacy border_/focus_/urgent_color
	 * keys map onto both layers for back-compat with example/config.lua. */
	lua_getfield(L, -1, "inner_border_color");
	bool has_inner_border = lua_isnumber(L, -1);
	lua_pop(L, 1);
	lua_getfield(L, -1, "inner_focus_color");
	bool has_inner_focus = lua_isnumber(L, -1);
	lua_pop(L, 1);
	lua_getfield(L, -1, "inner_urgent_color");
	bool has_inner_urgent = lua_isnumber(L, -1);
	lua_pop(L, 1);

	lua_getfield(L, -1, "border_color");
	uint32_t legacy_border = lua_isnumber(L, -1)
		? (uint32_t)lua_tointeger(L, -1) : 0x00000000;
	lua_pop(L, 1);
	lua_getfield(L, -1, "focus_color");
	uint32_t legacy_focus = lua_isnumber(L, -1)
		? (uint32_t)lua_tointeger(L, -1) : 0xe8e8e8ff;
	lua_pop(L, 1);
	lua_getfield(L, -1, "urgent_color");
	uint32_t legacy_urgent = lua_isnumber(L, -1)
		? (uint32_t)lua_tointeger(L, -1) : 0xff0000ff;
	lua_pop(L, 1);

	float lb[4], lf[4], lu[4];
	parse_color_hex(legacy_border, lb);
	parse_color_hex(legacy_focus, lf);
	parse_color_hex(legacy_urgent, lu);

	if (has_inner_border) lua_get_color(L, "inner_border_color", a->inner_border_color, 0.f,0.f,0.f,0.f);
	else memcpy(a->inner_border_color, lb, sizeof(lb));
	lua_get_color(L, "outer_border_color", a->outer_border_color, 0.f,0.f,0.f,0.f);
	if (!has_inner_border) memcpy(a->outer_border_color, lb, sizeof(lb));
	if (has_inner_focus) lua_get_color(L, "inner_focus_color", a->inner_focus_color, 0.91f,0.91f,0.91f,1.f);
	else memcpy(a->inner_focus_color, lf, sizeof(lf));
	lua_get_color(L, "outer_focus_color", a->outer_focus_color, 0.91f,0.91f,0.91f,1.f);
	if (!has_inner_focus) memcpy(a->outer_focus_color, lf, sizeof(lf));
	if (has_inner_urgent) lua_get_color(L, "inner_urgent_color", a->inner_urgent_color, 1.f,0.f,0.f,1.f);
	else memcpy(a->inner_urgent_color, lu, sizeof(lu));
	lua_get_color(L, "outer_urgent_color", a->outer_urgent_color, 1.f,0.f,0.f,1.f);
	if (!has_inner_urgent) memcpy(a->outer_urgent_color, lu, sizeof(lu));

	lua_get_color(L, "root_color",          a->root_color,          0.133f,0.133f,0.133f,1.f);

	lua_getfield(L, -1, "fullscreen_bg");
	if (lua_isnumber(L, -1))
		parse_color_hex((uint32_t)lua_tointeger(L, -1), a->fullscreen_bg);
	else {
		a->fullscreen_bg[0] = 0.f;
		a->fullscreen_bg[1] = 0.f;
		a->fullscreen_bg[2] = 0.f;
		a->fullscreen_bg[3] = 1.f;
	}
	lua_pop(L, 1);

	lua_pop(L, 1); /* pop appearance table */
}

static int
parse_scroll_method_string(const char *s)
{
	if      (!strcmp(s, "2fg"))    return LIBINPUT_CONFIG_SCROLL_2FG;
	else if (!strcmp(s, "edge"))   return LIBINPUT_CONFIG_SCROLL_EDGE;
	else if (!strcmp(s, "button")) return LIBINPUT_CONFIG_SCROLL_ON_BUTTON_DOWN;
	else if (!strcmp(s, "none"))   return LIBINPUT_CONFIG_SCROLL_NO_SCROLL;
	else                           return -1;
}

static void
parse_input(lua_State *L, CfgInput *inp)
{
	lua_getglobal(L, "input");
	if (!lua_istable(L, -1)) {
		fprintf(stderr, "swindle config: 'input' table missing, "
		        "using defaults. You'll feel the repeat delay.\n");
		lua_pop(L, 1);
		inp->repeat_rate  = 50;
		inp->repeat_delay = 150;
		inp->tap_to_click = true;
		inp->tap_and_drag = true;
		inp->drag_lock = true;
		inp->natural_scrolling = false;
		inp->natural_scrolling_touchpad = -1;
		inp->natural_scrolling_mouse = -1;
		inp->disable_while_typing = true;
		inp->left_handed = false;
		inp->middle_button_emulation = false;
		inp->scroll_method = LIBINPUT_CONFIG_SCROLL_2FG;
		inp->scroll_method_touchpad = -1;
		inp->scroll_method_mouse = -1;
		inp->click_method = LIBINPUT_CONFIG_CLICK_METHOD_BUTTON_AREAS;
		inp->send_events_mode = LIBINPUT_CONFIG_SEND_EVENTS_ENABLED;
		inp->accel_profile = LIBINPUT_CONFIG_ACCEL_PROFILE_ADAPTIVE;
		inp->accel_speed = 0.0;
		inp->tap_button_map = LIBINPUT_CONFIG_TAP_MAP_LRM;
		return;
	}

	inp->repeat_rate            = lua_get_int (L, "repeat_rate",            50);
	inp->repeat_delay           = lua_get_int (L, "repeat_delay",           150);
	inp->tap_to_click           = lua_get_bool(L, "tap_to_click",           true);
	inp->tap_and_drag           = lua_get_bool(L, "tap_and_drag",           true);
	inp->drag_lock              = lua_get_bool(L, "drag_lock",              true);
	inp->natural_scrolling      = lua_get_bool(L, "natural_scrolling",      false);
	inp->disable_while_typing   = lua_get_bool(L, "disable_while_typing",   true);
	inp->left_handed            = lua_get_bool(L, "left_handed",            false);
	inp->middle_button_emulation= lua_get_bool(L, "middle_button_emulation",false);
	inp->accel_speed            = lua_get_double(L, "accel_speed",          0.0);

	char scratch[CFG_MAX_STRLEN] = {0};

	lua_get_string(L, "scroll_method", scratch, sizeof(scratch));
	if (scratch[0]) {
		int m = parse_scroll_method_string(scratch);
		inp->scroll_method = (m >= 0) ? m : LIBINPUT_CONFIG_SCROLL_2FG;
	} else {
		inp->scroll_method = LIBINPUT_CONFIG_SCROLL_2FG;
	}

	/* Per-device overrides. Empty/missing = fall back to scroll_method. */
	memset(scratch, 0, sizeof(scratch));
	lua_get_string(L, "scroll_method_touchpad", scratch, sizeof(scratch));
	if (!scratch[0])
		lua_get_string(L, "touchpad_scroll_method", scratch, sizeof(scratch));
	inp->scroll_method_touchpad = scratch[0]
		? parse_scroll_method_string(scratch) : -1;

	memset(scratch, 0, sizeof(scratch));
	lua_get_string(L, "scroll_method_mouse", scratch, sizeof(scratch));
	if (!scratch[0])
		lua_get_string(L, "mouse_scroll_method", scratch, sizeof(scratch));
	inp->scroll_method_mouse = scratch[0]
		? parse_scroll_method_string(scratch) : -1;

	/* Tristate natural scrolling: explicit true/false wins, missing (-1)
	 * falls back to the generic natural_scrolling flag. Accepts both
	 * natural_scrolling_touchpad and touchpad_natural_scrolling spellings. */
	inp->natural_scrolling_touchpad = lua_get_bool_tristate(L,
		"natural_scrolling_touchpad", "touchpad_natural_scrolling");
	inp->natural_scrolling_mouse = lua_get_bool_tristate(L,
		"natural_scrolling_mouse", "mouse_natural_scrolling");

	lua_get_string(L, "accel_profile", scratch, sizeof(scratch));
	if (!strcmp(scratch, "flat"))        inp->accel_profile = LIBINPUT_CONFIG_ACCEL_PROFILE_FLAT;
	else                                 inp->accel_profile = LIBINPUT_CONFIG_ACCEL_PROFILE_ADAPTIVE;

	lua_get_string(L, "click_method", scratch, sizeof(scratch));
	if      (!strcmp(scratch, "clickfinger"))   inp->click_method = LIBINPUT_CONFIG_CLICK_METHOD_CLICKFINGER;
	else if (!strcmp(scratch, "button_areas"))  inp->click_method = LIBINPUT_CONFIG_CLICK_METHOD_BUTTON_AREAS;
	else                                        inp->click_method = LIBINPUT_CONFIG_CLICK_METHOD_BUTTON_AREAS;

	lua_get_string(L, "send_events_mode", scratch, sizeof(scratch));
	if      (!strcmp(scratch, "disabled"))             inp->send_events_mode = LIBINPUT_CONFIG_SEND_EVENTS_DISABLED;
	else if (!strcmp(scratch, "disabled_on_external")) inp->send_events_mode = LIBINPUT_CONFIG_SEND_EVENTS_DISABLED_ON_EXTERNAL_MOUSE;
	else                                               inp->send_events_mode = LIBINPUT_CONFIG_SEND_EVENTS_ENABLED;

	lua_get_string(L, "tap_button_map", scratch, sizeof(scratch));
	if (!strcmp(scratch, "lmr")) inp->tap_button_map = LIBINPUT_CONFIG_TAP_MAP_LMR;
	else                         inp->tap_button_map = LIBINPUT_CONFIG_TAP_MAP_LRM;

	lua_pop(L, 1);
}

static void
parse_keyboard(lua_State *L, CfgKeyboard *kb)
{
	/* defaults: US layout, everything else empty (= xkb defaults) */
	kb->layout[0] = '\0'; kb->variant[0] = '\0';
	kb->options[0] = '\0'; kb->model[0] = '\0';
	strcpy(kb->layout, "us");

	lua_getglobal(L, "keyboard");
	if (lua_istable(L, -1)) {
		lua_get_string(L, "layout",  kb->layout,  sizeof(kb->layout));
		lua_get_string(L, "variant", kb->variant, sizeof(kb->variant));
		lua_get_string(L, "options", kb->options, sizeof(kb->options));
		lua_get_string(L, "model",   kb->model,   sizeof(kb->model));
		if (!kb->layout[0]) strcpy(kb->layout, "us");
		lua_pop(L, 1);
	} else {
		lua_pop(L, 1);
		/* legacy spellings inside input = {...}: xkb_layout etc. */
		lua_getglobal(L, "input");
		if (lua_istable(L, -1)) {
			char tmp[CFG_MAX_STRLEN] = {0};
			if (lua_get_string(L, "xkb_layout", tmp, sizeof(tmp)) && tmp[0])
				strncpy(kb->layout, tmp, sizeof(kb->layout) - 1);
			memset(tmp, 0, sizeof(tmp));
			if (lua_get_string(L, "xkb_variant", tmp, sizeof(tmp)) && tmp[0])
				strncpy(kb->variant, tmp, sizeof(kb->variant) - 1);
			memset(tmp, 0, sizeof(tmp));
			if (lua_get_string(L, "xkb_options", tmp, sizeof(tmp)) && tmp[0])
				strncpy(kb->options, tmp, sizeof(kb->options) - 1);
			memset(tmp, 0, sizeof(tmp));
			if (lua_get_string(L, "xkb_model", tmp, sizeof(tmp)) && tmp[0])
				strncpy(kb->model, tmp, sizeof(kb->model) - 1);
		}
		lua_pop(L, 1);
	}
}

static void
parse_animations(lua_State *L, CfgAnimations *a)
{
	a->enabled = false;
	a->corner_radius = 0;
	a->fade_ms = 0;

	lua_getglobal(L, "animations");
	if (!lua_istable(L, -1)) {
		lua_pop(L, 1);
		return;
	}
	a->enabled = lua_get_bool(L, "enabled", false);
	a->corner_radius = lua_get_int(L, "corner_radius", 0);
	if (!a->corner_radius)
		a->corner_radius = lua_get_int(L, "border_radius", 0);
	a->fade_ms = lua_get_int(L, "fade_ms", 0);
	if (!a->fade_ms)
		a->fade_ms = lua_get_int(L, "duration_ms", 0);
	lua_pop(L, 1);

	if (a->enabled)
		fprintf(stderr, "swindle config: animations.enabled=true, but "
		        "scenefx is not linked yet (waiting on scenefx 0.5). "
		        "Parsed and ignored for now.\n");
}

static void
parse_rules(lua_State *L, Config *cfg)
{
	lua_getglobal(L, "rules");
	if (!lua_istable(L, -1)) {
		lua_pop(L, 1);
		return;
	}

	cfg->nrules = 0;
	int n = (int)lua_rawlen(L, -1);
	for (int i = 1; i <= n && cfg->nrules < CFG_MAX_RULES; i++) {
		lua_rawgeti(L, -1, i);
		if (!lua_istable(L, -1)) { lua_pop(L, 1); continue; }

		CfgRule *r = &cfg->rules[cfg->nrules++];
		memset(r, 0, sizeof(*r));
		lua_get_string(L, "app_id", r->app_id, sizeof(r->app_id));
		lua_get_string(L, "title",  r->title,  sizeof(r->title));
		r->tags     = (uint32_t)lua_get_int(L, "tags",     0);
		r->floating = lua_get_bool(L, "floating", false);
		r->monitor  = lua_get_int(L, "monitor", -1);

		lua_pop(L, 1);
	}
	lua_pop(L, 1);
}

static void
monitor_rule_default(CfgMonitorRule *m)
{
	memset(m, 0, sizeof(*m));
	m->mfact   = 0.55f;
	m->nmaster = 1;
	m->scale   = 1.0f;
	strcpy(m->layout, "dwindle");
	m->x = m->y = -1;
}

static void
parse_monitors(lua_State *L, Config *cfg)
{
	lua_getglobal(L, "monitors");
	if (!lua_istable(L, -1)) {
		lua_pop(L, 1);
		cfg->nmonitors = 1;
		monitor_rule_default(&cfg->monitors[0]);
		return;
	}

	cfg->nmonitors = 0;
	int n = (int)lua_rawlen(L, -1);
	for (int i = 1; i <= n && cfg->nmonitors < CFG_MAX_MONITORS; i++) {
		lua_rawgeti(L, -1, i);
		if (!lua_istable(L, -1)) { lua_pop(L, 1); continue; }

		CfgMonitorRule *m = &cfg->monitors[cfg->nmonitors++];
		memset(m, 0, sizeof(*m));

		lua_getfield(L, -1, "name");
		if (lua_isstring(L, -1))
			strncpy(m->name, lua_tostring(L, -1), sizeof(m->name)-1);
		lua_pop(L, 1);

		m->mfact   = (float)lua_get_double(L, "mfact",   0.55);
		m->nmaster =        lua_get_int   (L, "nmaster", 1);
		m->scale   = (float)lua_get_double(L, "scale",   1.0);
		m->x       =        lua_get_int   (L, "x",       -1);
		m->y       =        lua_get_int   (L, "y",       -1);
		m->transform =      lua_get_int   (L, "transform", 0);
		lua_get_string(L, "layout", m->layout, sizeof(m->layout));
		if (!m->layout[0]) strcpy(m->layout, "dwindle");

		lua_pop(L, 1);
	}
	lua_pop(L, 1);

	/* An empty/garbage table would leave zero rules and the compositor
	 * with uninitialized monitor state; fall back to the catch-all. */
	if (cfg->nmonitors == 0) {
		fprintf(stderr, "swindle config: no valid monitor rules, "
		        "using default catch-all\n");
		cfg->nmonitors = 1;
		monitor_rule_default(&cfg->monitors[0]);
	}
}

static void
parse_keybinds(lua_State *L, Config *cfg)
{
	lua_getglobal(L, "keybinds");
	if (!lua_istable(L, -1)) {
		lua_pop(L, 1);
		return;
	}

	cfg->nkeybinds = 0;
	struct xkb_context *xkb_ctx = xkb_context_new(XKB_CONTEXT_NO_FLAGS);

	int n = (int)lua_rawlen(L, -1);
	for (int i = 1; i <= n && cfg->nkeybinds < CFG_MAX_KEYBINDS; i++) {
		lua_rawgeti(L, -1, i);
		if (!lua_istable(L, -1)) { lua_pop(L, 1); continue; }

		CfgKeybind *kb = &cfg->keybinds[cfg->nkeybinds];
		memset(kb, 0, sizeof(*kb));

		/* mods: array of strings */
		kb->mods = 0;
		lua_getfield(L, -1, "mods");
		if (lua_istable(L, -1)) {
			int nm = (int)lua_rawlen(L, -1);
			for (int m = 1; m <= nm; m++) {
				lua_rawgeti(L, -1, m);
				if (lua_isstring(L, -1))
					kb->mods |= parse_modifier(lua_tostring(L, -1));
				lua_pop(L, 1);
			}
		}
		lua_pop(L, 1);

		/* key: string keysym name, e.g. "q", "Return", "XF86AudioMute" */
		char keyname[CFG_MAX_STRLEN] = {0};
		lua_get_string(L, "key", keyname, sizeof(keyname));
		kb->key = xkb_keysym_from_name(keyname, XKB_KEYSYM_CASE_INSENSITIVE);
		if (kb->key == XKB_KEY_NoSymbol) {
			fprintf(stderr, "swindle config: unknown key '%s', skipping bind\n",
			        keyname);
			lua_pop(L, 1); /* keybind entry table */
			continue;
		}

		lua_get_string(L, "action", kb->action, sizeof(kb->action));

		/* args: optional array of strings */
		kb->nargs = 0;
		lua_getfield(L, -1, "args");
		if (lua_istable(L, -1)) {
			int na = (int)lua_rawlen(L, -1);
			for (int a = 1; a <= na && kb->nargs < CFG_MAX_ARGS; a++) {
				lua_rawgeti(L, -1, a);
				if (lua_isstring(L, -1))
					strncpy(kb->args[kb->nargs++],
					        lua_tostring(L, -1),
					        CFG_MAX_STRLEN - 1);
				lua_pop(L, 1);
			}
		}
		lua_pop(L, 1); /* args */

		cfg->nkeybinds++;
		lua_pop(L, 1); /* keybind entry */
	}

	xkb_context_unref(xkb_ctx);
	lua_pop(L, 1); /* keybinds table */
}

static void
parse_buttons(lua_State *L, Config *cfg)
{
	lua_getglobal(L, "buttons");
	if (!lua_istable(L, -1)) {
		lua_pop(L, 1);
		return;
	}

	cfg->nbuttons = 0;
	int n = (int)lua_rawlen(L, -1);
	for (int i = 1; i <= n && cfg->nbuttons < CFG_MAX_KEYBINDS; i++) {
		lua_rawgeti(L, -1, i);
		if (!lua_istable(L, -1)) { lua_pop(L, 1); continue; }

		CfgButton *b = &cfg->buttons[cfg->nbuttons++];
		memset(b, 0, sizeof(*b));

		b->mods = 0;
		lua_getfield(L, -1, "mods");
		if (lua_istable(L, -1)) {
			int nm = (int)lua_rawlen(L, -1);
			for (int m = 1; m <= nm; m++) {
				lua_rawgeti(L, -1, m);
				if (lua_isstring(L, -1))
					b->mods |= parse_modifier(lua_tostring(L, -1));
				lua_pop(L, 1);
			}
		}
		lua_pop(L, 1);

		char btnname[CFG_MAX_STRLEN] = {0};
		lua_get_string(L, "button", btnname, sizeof(btnname));
		if      (!strcmp(btnname, "left"))   b->button = BTN_LEFT;
		else if (!strcmp(btnname, "right"))  b->button = BTN_RIGHT;
		else if (!strcmp(btnname, "middle")) b->button = BTN_MIDDLE;
		else b->button = (uint32_t)atoi(btnname); 

		lua_get_string(L, "action", b->action, sizeof(b->action));

		b->nargs = 0;
		lua_getfield(L, -1, "args");
		if (lua_istable(L, -1)) {
			int na = (int)lua_rawlen(L, -1);
			for (int a = 1; a <= na && b->nargs < CFG_MAX_ARGS; a++) {
				lua_rawgeti(L, -1, a);
				if (lua_isstring(L, -1))
					strncpy(b->args[b->nargs++],
					        lua_tostring(L, -1),
					        CFG_MAX_STRLEN - 1);
				lua_pop(L, 1);
			}
		}
		lua_pop(L, 1); /* args */

		lua_pop(L, 1);
	}
	lua_pop(L, 1);
}

static void
parse_autostart(lua_State *L, Config *cfg)
{
	lua_getglobal(L, "autostart");
	if (!lua_istable(L, -1)) {
		lua_pop(L, 1);
		return;
	}

	cfg->nautostart = 0;
	int n = (int)lua_rawlen(L, -1);
	for (int i = 1; i <= n && cfg->nautostart < CFG_MAX_AUTOSTART; i++) {
		lua_rawgeti(L, -1, i);
		if (!lua_isstring(L, -1)) {
			lua_pop(L, 1);
			continue;
		}
		strncpy(cfg->autostart[cfg->nautostart].cmd,
		        lua_tostring(L, -1),
		        CFG_MAX_AUTOSTART_CMD - 1);
		cfg->nautostart++;
		lua_pop(L, 1);
	}
	lua_pop(L, 1);
}

/* Public API */

const char *
config_fallback_path(void)
{
	return "/etc/swindle/config.lua";
}

char *
config_get_path(char *buf, size_t bufsz)
{
	const char *xdg = getenv("XDG_CONFIG_HOME");
	if (xdg && *xdg)
		snprintf(buf, bufsz, "%s/swindle/config.lua", xdg);
	else {
		const char *home = getenv("HOME");
		if (!home) home = "/root"; /* you're probably root, aren't you */
		snprintf(buf, bufsz, "%s/.config/swindle/config.lua", home);
	}
	return buf;
}

static bool
path_is_readable_file(const char *path)
{
	struct stat st;
	if (!path || !*path)
		return false;
	if (stat(path, &st) < 0)
		return false;
	if (!S_ISREG(st.st_mode))
		return false;
	return access(path, R_OK) == 0;
}

int
config_resolve_path(const char *preferred, char *out, size_t outsz)
{
	if (preferred && *preferred) {
		if (!path_is_readable_file(preferred))
			return -1;
		snprintf(out, outsz, "%s", preferred);
		return 0;
	}
	char user[1024];
	config_get_path(user, sizeof(user));
	if (path_is_readable_file(user)) {
		snprintf(out, outsz, "%s", user);
		return 0;
	}
	if (path_is_readable_file(config_fallback_path())) {
		snprintf(out, outsz, "%s", config_fallback_path());
		return 0;
	}
	return -1;
}

int
config_load_with_fallback(const char *preferred, Config *cfg,
                          char *used_path, size_t used_sz)
{
	char resolved[1024];
	if (config_resolve_path(preferred, resolved, sizeof(resolved)) < 0)
		return -1;
	if (config_load(resolved, cfg) < 0)
		return -1;
	if (used_path && used_sz)
		snprintf(used_path, used_sz, "%s", resolved);
	return 0;
}

int
config_load(const char *path, Config *cfg)
{
	memset(cfg, 0, sizeof(*cfg));

	lua_State *L = luaL_newstate();
	if (!L) {
		fprintf(stderr, "swindle config: failed to create Lua state. "
		        "Truly rock bottom.\n");
		return -1;
	}
	luaL_openlibs(L);

	if (luaL_dofile(L, path) != LUA_OK) {
		lua_log_error(L, "load");
		lua_close(L);
		return -1;
	}

	/* Top-level booleans */
	lua_getglobal(L, "sloppy_focus");
	cfg->sloppyfocus = lua_isboolean(L, -1) ? lua_toboolean(L, -1) : true;
	lua_pop(L, 1);

	lua_getglobal(L, "bypass_surface_visibility");
	cfg->bypass_surface_visibility = lua_isboolean(L, -1)
	                                 ? lua_toboolean(L, -1) : false;
	lua_pop(L, 1);

	lua_getglobal(L, "log_level");
	if (lua_isstring(L, -1)) {
		const char *ll = lua_tostring(L, -1);
		if      (!strcmp(ll, "silent")) cfg->log_level = WLR_SILENT;
		else if (!strcmp(ll, "info"))   cfg->log_level = WLR_INFO;
		else if (!strcmp(ll, "debug"))  cfg->log_level = WLR_DEBUG;
		else                            cfg->log_level = WLR_ERROR;
	} else {
		cfg->log_level = WLR_ERROR;
	}
	lua_pop(L, 1);

	parse_appearance(L, &cfg->appearance);
	parse_input     (L, &cfg->input);
	parse_keyboard  (L, &cfg->keyboard);
	parse_animations(L, &cfg->animations);
	parse_rules     (L, cfg);
	parse_monitors  (L, cfg);
	parse_keybinds  (L, cfg);
	parse_buttons   (L, cfg);
	parse_autostart (L, cfg);

	lua_close(L);
	return 0;
}

void
config_autostart_run(const Config *cfg)
{
	if (cfg->nautostart == 0)
		return;

	pid_t pid = fork();
	if (pid < 0) {
		perror("swindle autostart: fork");
		return;
	}
	if (pid != 0)
		return; /* compositor continues (sequencer runs in child) */

	/* The sequencer child resets any inherited signal handlers and becomes
	 * a new session so the compositor's SIGCHLD handling doesn't
	 * interfere with our waitpid calls. */
	signal(SIGCHLD, SIG_DFL);
	setsid();

	for (int i = 0; i < cfg->nautostart; i++) {
		const char *cmd = cfg->autostart[i].cmd;

		pid_t cpid = fork();
		if (cpid < 0) {
			fprintf(stderr, "swindle autostart: fork for '%s': %s\n",
			        cmd, strerror(errno));
			continue;
		}
		if (cpid == 0) {
			execl("/bin/sh", "sh", "-c", cmd, NULL);
			fprintf(stderr, "swindle autostart: exec '%s': %s\n",
			        cmd, strerror(errno));
			_exit(1);
		}

		/* wait a second because one shotters will fly past, but
		 * daemon processes (some, like swaybg) won't because of 
		 * daemon black magic. To say the least, it's good for the soul */
		struct timespec deadline, now;
		clock_gettime(CLOCK_MONOTONIC, &deadline);
		deadline.tv_sec += 0.5;

		int status;
		int done = 0;
		while (!done) {
			pid_t r = waitpid(cpid, &status, WNOHANG);
			if (r == cpid) {
				done = 1;
				if (WIFEXITED(status) && WEXITSTATUS(status) != 0)
					fprintf(stderr, "swindle autostart: '%s' exited with status %d\n",
					        cmd, WEXITSTATUS(status));
			} else {
				clock_gettime(CLOCK_MONOTONIC, &now);
				if (now.tv_sec > deadline.tv_sec ||
				    (now.tv_sec == deadline.tv_sec &&
				     now.tv_nsec >= deadline.tv_nsec))
					break; /* looks like a daemon, move on */
				struct timespec sl = { .tv_sec = 0, .tv_nsec = 50 * 1000 * 1000 };
				nanosleep(&sl, NULL);
			}
		}
	}

	_exit(0);
}

/* inotify watcher */

static int
watch_dispatch(int fd, uint32_t mask, void *data)
{
	WatchState *ws = data;

	/* Drain the inotify events */
	_Alignas(struct inotify_event) char buf[4096];
	ssize_t len = read(fd, buf, sizeof(buf));
	if (len < 0) {
		if (errno != EAGAIN)
			perror("swindle config: inotify read");
		return 0;
	}

	/* Re-parse */ 
	Config *fresh = calloc(1, sizeof(Config));
	if (!fresh) return 0;

	if (config_load(ws->path, fresh) == 0) {
		fprintf(stderr, "swindle config: reloaded '%s'\n", ws->path);
		if (ws->cb)
			ws->cb(fresh, ws->userdata);
	} else {
		fprintf(stderr, "swindle config: reload failed, keeping old config. "
		        "Story of my life.\n");
	}

	free(fresh);

	/*
	 * Some editors (vim, nano) delete-and-recreate files on save,
	 * which removes our watch (which is a pain in the ass. 
	 * Hence, re-add it defensively. It's good for the soul)
	 */
	inotify_rm_watch(ws->inotify_fd, ws->watch_fd);
	ws->watch_fd = inotify_add_watch(ws->inotify_fd, ws->path,
	                                  IN_CLOSE_WRITE | IN_MOVED_TO | IN_CREATE);

	return 0;
}

struct wl_event_source *
config_watch_start(struct wl_event_loop *loop, const char *path,
                   config_reload_cb cb, void *userdata)
{
	WatchState *ws = calloc(1, sizeof(*ws));
	if (!ws) return NULL;

	ws->inotify_fd = inotify_init1(IN_NONBLOCK | IN_CLOEXEC);
	if (ws->inotify_fd < 0) {
		perror("swindle config: inotify_init1");
		free(ws);
		return NULL;
	}

	ws->watch_fd = inotify_add_watch(ws->inotify_fd, path,
	                                  IN_CLOSE_WRITE | IN_MOVED_TO | IN_CREATE);
	if (ws->watch_fd < 0) {
		perror("swindle config: inotify_add_watch");
		close(ws->inotify_fd);
		free(ws);
		return NULL;
	}

	strncpy(ws->path, path, sizeof(ws->path) - 1);
	ws->cb       = cb;
	ws->userdata = userdata;

	struct wl_event_source *src = wl_event_loop_add_fd(
		loop, ws->inotify_fd,
		WL_EVENT_READABLE,
		watch_dispatch, ws
	);

	if (!src) {
		fprintf(stderr, "swindle config: failed to add inotify to event loop. "
		        "Classic.\n");
		close(ws->inotify_fd);
		free(ws);
		return NULL;
	}

	ws->event_src = src;
	fprintf(stderr, "swindle config: watching '%s'\n", path);
	return (struct wl_event_source *)ws;
}

void
config_watch_stop(struct wl_event_source *src)
{
	if (!src) return;
	WatchState *ws = (WatchState *)src;
	wl_event_source_remove(ws->event_src);
	close(ws->inotify_fd); 
	free(ws);
}
