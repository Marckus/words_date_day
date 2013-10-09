#include "pebble_os.h"
#include "pebble_app.h"
#include "pebble_fonts.h"
#include "num2words.h"


#define MY_UUID { 0xB4, 0xB9, 0x12, 0x4A, 0xC3, 0x72, 0x44, 0x42, 0x90, 0xE2, 0xEA, 0x3B, 0xCA, 0xB2, 0x48, 0xC9 }
#define TIME_SLOT_ANIMATION_DURATION 700
#define NUM_LAYERS 4

enum layer_names {
  MINUTES,
  TENS,
  HOURS,
  DATE
};

PBL_APP_INFO(MY_UUID,
             "Words + Date + DayI", "Marckus",  // based entirely on work from Daniel Hertz + James Hrisho - MSD 9-16-13 Inversion added 10-9-2013
             1, 0, /* App version */
             RESOURCE_ID_IMAGE_MENU_ICON,
             APP_INFO_WATCH_FACE);

Window window;
TextLayer text_date_layer;

typedef struct CommonWordsData {
  TextLayer label;
  PropertyAnimation in_animation;
  PropertyAnimation out_animation;
  void (*update) (PblTm *t, char *words);
  char buffer[BUFFER_SIZE];
} CommonWordsData;

static struct CommonWordsData layers[NUM_LAYERS] =
{{ .update = &fuzzy_sminutes_to_words },
 { .update = &fuzzy_minutes_to_words },
 { .update = &fuzzy_hours_to_words },
 { .update = &fuzzy_dates_to_words, .buffer = "Xxxx Xxx 00" }};  // Uupdated .buffer for Day of Week addition - MSD 9-18-13

void slide_out(PropertyAnimation *animation, CommonWordsData *layer) {
  GRect from_frame = layer_get_frame(&layer->label.layer);

  GRect to_frame = GRect(-window.layer.frame.size.w, from_frame.origin.y,
                          window.layer.frame.size.w, from_frame.size.h);

  property_animation_init_layer_frame(animation, &layer->label.layer, NULL,
                                        &to_frame);
  animation_set_duration(&animation->animation, TIME_SLOT_ANIMATION_DURATION);
  animation_set_curve(&animation->animation, AnimationCurveEaseIn);
}

void slide_in(PropertyAnimation *animation, CommonWordsData *layer) {
  GRect to_frame = layer_get_frame(&layer->label.layer);
  GRect from_frame = GRect(2*window.layer.frame.size.w, to_frame.origin.y,
                            window.layer.frame.size.w, to_frame.size.h);

  layer_set_frame(&layer->label.layer, from_frame);
  text_layer_set_text(&layer->label, layer->buffer);
  property_animation_init_layer_frame(animation, &layer->label.layer, NULL,
                                        &to_frame);
  animation_set_duration(&animation->animation, TIME_SLOT_ANIMATION_DURATION);
  animation_set_curve(&animation->animation, AnimationCurveEaseOut);
}

void slide_out_animation_stopped(Animation *slide_out_animation, bool finished,
                                  void *context) {
  CommonWordsData *layer = (CommonWordsData *)context;
  layer->label.layer.frame.origin.x = 0;
  PblTm t;
  get_time(&t);
  layer->update(&t, layer->buffer);
  slide_in(&layer->in_animation, layer);
  animation_schedule(&layer->in_animation.animation);
}

void update_layer(CommonWordsData *layer) {
  slide_out(&layer->out_animation, layer);
  animation_set_handlers(&layer->out_animation.animation, (AnimationHandlers){
    .stopped = (AnimationStoppedHandler)slide_out_animation_stopped
  }, (void *) layer);
  animation_schedule(&layer->out_animation.animation);
}

static void handle_minute_tick(AppContextRef app_ctx, PebbleTickEvent* e) {
  PblTm *t = e->tick_time;
  if((e->units_changed & MINUTE_UNIT) == MINUTE_UNIT) {
    if ((17 > t->tm_min || t->tm_min > 19)
          && (11 > t->tm_min || t->tm_min > 13)) {
      update_layer(&layers[MINUTES]);
    }
    if (t->tm_min % 10 == 0 || (t->tm_min > 10 && t->tm_min < 20)
          || t->tm_min == 1) {
      update_layer(&layers[TENS]);
    }
  }
  if ((e->units_changed & HOUR_UNIT) == HOUR_UNIT ||
        ((t->tm_hour == 00 || t->tm_hour == 12) && t->tm_min == 01)) {
    update_layer(&layers[HOURS]);
  }
  if ((e->units_changed & DAY_UNIT) == DAY_UNIT) {
    update_layer(&layers[DATE]);
  }
}

void init_layer(CommonWordsData *layer, GRect rect, GFont font) {
  text_layer_init(&layer->label, rect);
  text_layer_set_background_color(&layer->label, GColorClear); // changed for inversion - MSD 10-9-13
  text_layer_set_text_color(&layer->label, GColorBlack); // changed for inversion - MSD 10-9-13
  text_layer_set_font(&layer->label, font);
  layer_add_child(&window.layer, &layer->label.layer);
}

void handle_init(AppContextRef ctx) {
  (void)ctx;

  window_init(&window, "Words + Date + Day");  // added "+ Day" - MSD 9-16-13
  const bool animated = true;
  window_stack_push(&window, animated);
  window_set_background_color(&window, GColorWhite);  // changed for inversion - MSD 10-9-13
  resource_init_current_app(&APP_RESOURCES);

// Changed all fonts below to updated 1.12 SDK fonts - MSD 9-16-13)
// single digits
  init_layer(&layers[MINUTES],GRect(0, 76, window.layer.frame.size.w, 50),fonts_get_system_font(FONT_KEY_BITHAM_42_LIGHT));

// 00 minutes
  init_layer(&layers[TENS], GRect(0, 38, window.layer.frame.size.w, 50),fonts_get_system_font(FONT_KEY_BITHAM_42_LIGHT));

//hours
  init_layer(&layers[HOURS], GRect(0, 0, window.layer.frame.size.w, 50),fonts_get_system_font(FONT_KEY_BITHAM_42_BOLD));

//Date & Day of Week   (Changed font to Gothic 28 Bold to add Day of Week  and moved layer down a few pixels - MSD 9-16-13)
  init_layer(&layers[3], GRect(0, 117, window.layer.frame.size.w, 50),fonts_get_system_font(FONT_KEY_GOTHIC_28_BOLD));

//show your face
  PblTm t;
  get_time(&t);

  for (int i = 0; i < NUM_LAYERS; ++i)
  {
    layers[i].update(&t, layers[i].buffer);
    slide_in(&layers[i].in_animation, &layers[i]);
    animation_schedule(&layers[i].in_animation.animation);
  }
}


void pbl_main(void *params) {
 PebbleAppHandlers handlers = {
    .init_handler = &handle_init,

    .tick_info = {
      .tick_handler = &handle_minute_tick,
      .tick_units = MINUTE_UNIT
    }

  };
  app_event_loop(params, &handlers);
}
