#include "pebble.h"

#define HOME_SCORE_KEY 1
#define AWAY_SCORE_KEY 2
#define TIME_KEY 3
#define COUNTDOWN_KEY 4

#define HOME_SCORE_DEFAULT 0
#define AWAY_SCORE_DEFAULT 0
#define TIME_DEFAULT 0
#define COUNTDOWN_DEFAULT 2700000
#define INTERVAL 83

static Window *window;
static TextLayer *text_home_score_layer;
static TextLayer *text_away_score_layer;
static TextLayer *text_stopwatch_layer;
static TextLayer *text_millisecond_layer;
static TextLayer *text_countdown_layer;

static bool running = false;
static bool low_time_notification = false;
static bool selecting_target_time;
static long elapsed_time;
static long target_time;
static long start_time;
static long pause_time;

static int home_score;
static int away_score;

static char time_text[] = "00:00";
static char millisecond_text[] = ".000";
static char time_secondary_text[] = "-00:00";

static AppTimer *update_timer;

static long get_millisecond_time() {
	return time(NULL) * 1000 + time_ms(NULL, NULL);
}

static void reset_timer(bool first_time) {
	long orig_time = get_millisecond_time();

	if (first_time) {
		pause_time = orig_time;
		start_time = orig_time - elapsed_time;
	} else {
		pause_time = orig_time;
		start_time = orig_time;
	}
}

static void update_elapsed_time() {
	if (running) {
		elapsed_time = get_millisecond_time() - start_time;
	} else {
		elapsed_time = pause_time - start_time;
	}
}

static void update_home_score() {
	static char score_text[] = "xxxx";
	snprintf(score_text, sizeof(score_text), "%u", home_score);
	text_layer_set_text(text_home_score_layer, score_text);
}

static void update_away_score() {
	static char score_text[] = "xxxx";
	snprintf(score_text, sizeof(score_text), "%u", away_score);
	text_layer_set_text(text_away_score_layer, score_text);
}

static void update_stopwatch() {
	update_elapsed_time();

	int minute;
	int second;
	int millisecond;

	if (selecting_target_time) {
		minute = (int) target_time / 60000;
		second = (int) target_time % 60000 / 1000;

		snprintf(time_text, sizeof(time_text), "%d:%02d", minute, second);
		snprintf(millisecond_text, sizeof(millisecond_text), " ");
		snprintf(time_secondary_text, sizeof(time_secondary_text), " ");
	} else {
		minute = (int) elapsed_time / 60000;
		second = (int) elapsed_time % 60000 / 1000;
		millisecond = (int) elapsed_time % 1000;

		snprintf(time_text, sizeof(time_text), "%d:%02d", minute, second);
		snprintf(millisecond_text, sizeof(millisecond_text), ".%03d", millisecond);

		long time_remaining = target_time - elapsed_time;

		if (time_remaining > 0) {
			minute = (int) time_remaining / 60000;
			second = (int) time_remaining % 60000 / 1000;

			snprintf(time_secondary_text, sizeof(time_secondary_text), "-%d:%02d", minute, second);
		} else {
			time_remaining *= -1;

			minute = (int) time_remaining / 60000;
			second = (int) time_remaining % 60000 / 1000;

			snprintf(time_secondary_text, sizeof(time_secondary_text), "+%d:%02d", minute, second);
		}
	}

	text_layer_set_text(text_stopwatch_layer, time_text);
	text_layer_set_text(text_millisecond_layer, millisecond_text);
	text_layer_set_text(text_countdown_layer, time_secondary_text);
}

static void timer_callback(void *data) {

	update_stopwatch();

	elapsed_time = get_millisecond_time() - start_time;

	if (elapsed_time >= 6000000) {
		start_time += 6000000;
	}

	if (target_time - elapsed_time <= 60000) {
		//vibes_long_pulse();
	}

	update_timer = app_timer_register(INTERVAL, (AppTimerCallback) timer_callback, NULL);
}

static void up_single_click_handler(ClickRecognizerRef recognizer, void *context) {
	if (running) {
		if (home_score < 9999) {
			home_score++;
			update_home_score();
		}
	} else if (selecting_target_time) {
		if (target_time < 5940000) {
			target_time += 60000;
			update_stopwatch();
		}
	} else {
		selecting_target_time = true;
		update_stopwatch();
	}
}

static void up_long_click_handler(ClickRecognizerRef recognizer, void *context) {
	if (running && home_score > 0) {
		home_score--;
		update_home_score();
	}
}

static void up_long_click_release_handler(ClickRecognizerRef recognizer, void *context) {

}

static void select_single_click_handler(ClickRecognizerRef recognizer, void *context) {
	if (running) {
		running = false;
		vibes_short_pulse();
		pause_time = get_millisecond_time();
		app_timer_cancel(update_timer);

	} else if (selecting_target_time) {
		selecting_target_time = false;
		update_stopwatch();

	} else {
		running = true;
		vibes_short_pulse();
		if (pause_time == 0) {
			start_time = get_millisecond_time();
		} else {
			start_time = get_millisecond_time() - pause_time + start_time;
		}
		update_timer = app_timer_register(INTERVAL, (AppTimerCallback) timer_callback, NULL);

	}
}

static void down_single_click_handler(ClickRecognizerRef recognizer, void *context) {
	if (running) {
		if (away_score < 9999) {
			away_score++;
			update_away_score();
		}
	} else if (selecting_target_time) {
		if (target_time > 60000) {
			target_time -= 60000;
			update_stopwatch();
		}
	} else {
		reset_timer(false);
		update_stopwatch();
	}
}

static void down_long_click_handler(ClickRecognizerRef recognizer, void *context) {
	if (running && away_score > 0) {
		away_score--;
		update_away_score();
	} else if (!selecting_target_time) {
		home_score = 0;
		away_score = 0;
		update_home_score();
		update_away_score();
	}
}

static void down_long_click_release_handler(ClickRecognizerRef recognizer, void *context) {

}

static void config_provider(Window *window) {
	window_single_click_subscribe(BUTTON_ID_UP, up_single_click_handler);
	window_long_click_subscribe(BUTTON_ID_UP, 0, up_long_click_handler, up_long_click_release_handler);

	window_single_click_subscribe(BUTTON_ID_SELECT, select_single_click_handler);

	window_single_click_subscribe(BUTTON_ID_DOWN, down_single_click_handler);
	window_long_click_subscribe(BUTTON_ID_DOWN, 0, down_long_click_handler, down_long_click_release_handler);
}

static void handle_init(void) {
	window = window_create();
	window_stack_push(window, true);
	window_set_background_color(window, GColorBlack);

	Layer *window_layer = window_get_root_layer(window);

	text_home_score_layer = text_layer_create(GRect(0, 0, 144, 27));
	text_layer_set_text_alignment(text_home_score_layer, GTextAlignmentCenter);
	text_layer_set_font(text_home_score_layer, fonts_get_system_font(FONT_KEY_ROBOTO_CONDENSED_21));
	layer_add_child(window_layer, text_layer_get_layer(text_home_score_layer));

	text_away_score_layer = text_layer_create(GRect(0, 168-27, 144, 27));
	text_layer_set_text_alignment(text_away_score_layer, GTextAlignmentCenter);
	text_layer_set_font(text_away_score_layer, fonts_get_system_font(FONT_KEY_ROBOTO_CONDENSED_21));
	layer_add_child(window_layer, text_layer_get_layer(text_away_score_layer));

	text_stopwatch_layer = text_layer_create(GRect(7, 51, 144-14, 168-92));
	text_layer_set_text_color(text_stopwatch_layer, GColorWhite);
	text_layer_set_background_color(text_stopwatch_layer, GColorClear);
  text_layer_set_text_alignment(text_stopwatch_layer, GTextAlignmentRight);
	text_layer_set_font(text_stopwatch_layer, fonts_get_system_font(FONT_KEY_ROBOTO_BOLD_SUBSET_49));
	layer_add_child(window_layer, text_layer_get_layer(text_stopwatch_layer));

	text_millisecond_layer = text_layer_create(GRect(7, 103, 144-14, 168-92));
	text_layer_set_text_color(text_millisecond_layer, GColorWhite);
	text_layer_set_background_color(text_millisecond_layer, GColorClear);
  text_layer_set_text_alignment(text_millisecond_layer, GTextAlignmentRight);
	text_layer_set_font(text_millisecond_layer, fonts_get_system_font(FONT_KEY_ROBOTO_CONDENSED_21));
	layer_add_child(window_layer, text_layer_get_layer(text_millisecond_layer));

	text_countdown_layer = text_layer_create(GRect(7, 38, 144-14, 168-92));
	text_layer_set_text_color(text_countdown_layer, GColorWhite);
	text_layer_set_background_color(text_countdown_layer, GColorClear);
  text_layer_set_text_alignment(text_countdown_layer, GTextAlignmentRight);
	text_layer_set_font(text_countdown_layer, fonts_get_system_font(FONT_KEY_ROBOTO_CONDENSED_21));
	layer_add_child(window_layer, text_layer_get_layer(text_countdown_layer));

	home_score = persist_exists(HOME_SCORE_KEY) ? persist_read_int(HOME_SCORE_KEY) : HOME_SCORE_DEFAULT;
	away_score = persist_exists(AWAY_SCORE_KEY) ? persist_read_int(AWAY_SCORE_KEY) : AWAY_SCORE_DEFAULT;
	elapsed_time = persist_exists(TIME_KEY) ? persist_read_int(TIME_KEY) : TIME_DEFAULT;
	target_time = persist_exists(COUNTDOWN_KEY) ? persist_read_int(COUNTDOWN_KEY) : COUNTDOWN_DEFAULT;

	update_home_score();
	update_away_score();
	reset_timer(true);
	update_stopwatch();

	window_set_click_config_provider(window, (ClickConfigProvider) config_provider);
}

static void handle_deinit(void) {
	text_layer_destroy(text_home_score_layer);
	text_layer_destroy(text_away_score_layer);
	text_layer_destroy(text_stopwatch_layer);
	text_layer_destroy(text_millisecond_layer);
	text_layer_destroy(text_countdown_layer);

	app_timer_cancel(update_timer);

	persist_write_int(HOME_SCORE_KEY, home_score);
	persist_write_int(AWAY_SCORE_KEY, away_score);
	persist_write_int(TIME_KEY, elapsed_time);
	persist_write_int(COUNTDOWN_KEY, target_time);
}

int main(void) {
	handle_init();

	app_event_loop();

	handle_deinit();
}
