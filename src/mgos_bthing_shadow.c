#include "mgos.h"
#include "mgos_bthing_shadow.h"
#include "mgos_bvar_json.h"
#include "mgos_bvar_dic.h"
#include "mg_bthing_sdk.h"

#ifdef MGOS_HAVE_MJS
#include "mjs.h"
#endif

#define MG_BTHING_SHADOW_OPTIMIZE_TIMEOUT 50 // milliseconds

static struct mg_bthing_shadow_ctx {
  struct mgos_bthing_shadow_state state;
  bool optimize_enabled; 
  mgos_timer_id optimize_timer_id;
  int optimize_timeout; 
  int64_t last_event;
} s_ctx;

#if MGOS_BTHING_HAVE_SENSORS

bool mg_bthing_shadow_add_state(mgos_bvar_t shadow, mgos_bthing_t thing) {
  const char *dom = mgos_bthing_get_domain(thing);
  mgos_bvar_t dic = shadow;
  if (dom) {
    if (!mgos_bvar_try_get_key(shadow, dom, &dic)) {
      // create the domain dictionary
      dic = mgos_bvar_new_dic();
      if (!mgos_bvar_add_key(shadow, dom, dic)) {
        mgos_bvar_free(dic);
        return false;
      }
    }
  }
  return mgos_bvar_add_key(dic, mgos_bthing_get_id(thing), (mgos_bvar_t)mg_bthing_get_raw_state(thing));
}

mgos_bvarc_t mg_bthing_shadow_get_state(mgos_bvarc_t shadow, mgos_bthing_t thing) {
  const char *dom = mgos_bthing_get_domain(thing);
  mgos_bvarc_t dic = shadow;
  if (dom) {
    if (!mgos_bvarc_try_get_key(shadow, dom, &dic)) return NULL;
  }
  return mgos_bvarc_get_key(dic, mgos_bthing_get_id(thing));
}

void mg_bthing_shadow_remove_state(mgos_bvar_t shadow, mgos_bthing_t thing) {
  const char *dom = mgos_bthing_get_domain(thing);
  mgos_bvar_t dic = shadow;
  if (dom) {
    if (!mgos_bvar_try_get_key(shadow, dom, &dic)) return;
  }
  mgos_bvar_remove_key(dic, mgos_bthing_get_id(thing));
  if (dic != shadow && mgos_bvar_length(dic) == 0)
    mgos_bvar_free(dic); // delete empty domain dictionary
}

void mg_bthing_shadow_empty(mgos_bvar_t shadow) {
  mgos_bvar_t key_value;
  const char *key_name;
  mgos_bvar_enum_t keys = mgos_bvar_get_keys(shadow);
  while (mgos_bvar_get_next_key(&keys, &key_value, &key_name)) {
    mgos_bthing_enum_t things = mgos_bthing_get_all();
    if (mgos_bvar_is_dic(key_value) &&
        mgos_bthing_filter_get_next(&things, NULL, MGOS_BTHING_FILTER_BY_DOMAIN, key_name)) {
      // the shadow key a domain dictionary, so I must...
      // remove all keys (all the the bThing states), and
      mgos_bvar_remove_keys(key_value);
      // delete the (empty) domain dictionary.
      mgos_bvar_delete_key(shadow, key_name);
    } else {
      // the shadow key is the bThing state, so
      // I just have to remove it.
      mgos_bvar_remove_key(shadow, key_name);
    }
  }
}

#endif // MGOS_BTHING_HAVE_SENSORS

static void mg_bthing_shadow_on_created(int ev, void *ev_data, void *userdata) {
  #if MGOS_BTHING_HAVE_SENSORS
  if (!mg_bthing_shadow_add_state((mgos_bvar_t)s_ctx.state.full_shadow, (mgos_bthing_t)ev_data)) {
    LOG(LL_ERROR, ("Something went wrong adding '%s' state to the full-shadow.",
      mgos_bthing_get_uid((mgos_bthing_t)ev_data)));
  }
  #else
  (void) ev_data;
  #endif
  
  (void) ev;
  (void) userdata;
}

static void mg_bthing_shadow_on_made_private(int ev, void *ev_data, void *userdata) {
  #if MGOS_BTHING_HAVE_SENSORS
  if (ev_data) {
    mg_bthing_shadow_remove_state((mgos_bvar_t)s_ctx.state.full_shadow, (mgos_bthing_t)ev_data);
  }
  #else
  (void) ev_data;
  #endif
  
  (void) ev;
  (void) userdata;
}

/* bool mg_bthing_shadow_must_ignore_item(mgos_bthing_t thing) {
  return (mg_bthing_shadow_get_state(s_ctx.state.full_shadow, thing) == NULL);
} */

// bool mg_bthing_shadow_must_ignore_item(mgos_bthing_t thing) {
//   mgos_bvarc_t s = mg_bthing_shadow_get_state(s_ctx.state.full_shadow, thing);
//   if (mg_bthing_has_flag(thing, MG_BTHING_FLAG_ISPRIVATE)) {
//     if (s) mg_bthing_shadow_remove_state((mgos_bvar_t)s_ctx.state.full_shadow, thing);
//     return true;
//   } else {
//     if (!s) mg_bthing_shadow_add_state((mgos_bvar_t)s_ctx.state.full_shadow, thing);
//     return false;
//   }
// }

// bool mg_bthing_shadow_optimize_timeout_reached() {
//   return ((s_ctx.last_event != 0) &&
//           ((mg_bthing_duration_micro(s_ctx.last_event, mgos_uptime_micros()) / 1000) >= s_ctx.optimize_timeout));
// }

// bool mg_bthing_shadow_optimize_timeout_reached() {
//   if (s_ctx.last_event == 0)
//     LOG(LL_INFO, ("s_ctx.last_event = 0"));
//   else
//     LOG(LL_INFO, ("timeout: %lld-%lld=%lld", s_ctx.last_event, mgos_uptime_micros(), mg_bthing_duration_micro(s_ctx.last_event, mgos_uptime_micros())));
//   return (s_ctx.last_event == 0 ? true : ((mg_bthing_duration_micro(s_ctx.last_event, mgos_uptime_micros()) / 1000) >= s_ctx.optimize_timeout));
// }

// static int mg_bthing_shadow_start_optimize_timer(timer_callback cb) {
//   if (s_ctx.optimize_timer_id == MGOS_INVALID_TIMER_ID && s_ctx.optimize_timeout > 0) {
//     s_ctx.optimize_timer_id = mgos_set_timer(s_ctx.optimize_timeout, MGOS_TIMER_REPEAT, cb, NULL);
//   }
//   return s_ctx.optimize_timer_id;
// }

// static void mg_bthing_shadow_clear_optimize_timer() {
//   if (s_ctx.optimize_timer_id != MGOS_INVALID_TIMER_ID) {
//     mgos_clear_timer(s_ctx.optimize_timer_id);
//     s_ctx.optimize_timer_id = MGOS_INVALID_TIMER_ID;
//   }
// }

#if MGOS_BTHING_HAVE_SENSORS

// static bool mg_bthing_shadow_trigger_events(bool force) {
//   if (force || mg_bthing_shadow_optimize_timeout_reached()) {
    
//     enum mgos_bthing_state_flag state_flags = s_ctx.state.state_flags;
//     if ((state_flags & MGOS_BTHING_STATE_FLAG_CHANGED) == MGOS_BTHING_STATE_FLAG_CHANGED) {
//       s_ctx.state.state_flags = MGOS_BTHING_STATE_FLAG_CHANGED;
//     } else {
//       s_ctx.state.state_flags = MGOS_BTHING_STATE_FLAG_UNCHANGED;
//     }
    
//     if ((state_flags & MGOS_BTHING_STATE_FLAG_FORCED_PUB) == MGOS_BTHING_STATE_FLAG_FORCED_PUB) {
//       s_ctx.state.state_flags |= MGOS_BTHING_STATE_FLAG_FORCED_PUB;
//     }
    
//     if ((s_ctx.state.state_flags & MGOS_BTHING_STATE_FLAG_CHANGED) == MGOS_BTHING_STATE_FLAG_CHANGED) {
//       // raise the SHADOW_CHANGED event
//       mgos_event_trigger(MGOS_EV_BTHING_SHADOW_CHANGED, &s_ctx.state);
//       LOG(LL_INFO, ("TRIGGER SHADOW_CHANGED"));
//     }  

//     if ((state_flags & MGOS_BTHING_STATE_FLAG_UPDATED) == MGOS_BTHING_STATE_FLAG_UPDATED) {
//       // raise the SHADOW_UPDATED event
//       s_ctx.state.state_flags |= MGOS_BTHING_STATE_FLAG_UPDATED;
//       mgos_event_trigger(MGOS_EV_BTHING_SHADOW_UPDATED, &s_ctx.state);
//       LOG(LL_INFO, ("TRIGGER SHADOW_UPDATED"));
    
//       if ((s_ctx.state.state_flags & MGOS_BTHING_STATE_FLAG_FORCED_PUB) == MGOS_BTHING_STATE_FLAG_FORCED_PUB ||
//           (s_ctx.state.state_flags & MGOS_BTHING_STATE_FLAG_CHANGED) == MGOS_BTHING_STATE_FLAG_CHANGED) {
//         // raise the SHADOW_PUBLISHING event
//         mgos_event_trigger(MGOS_EV_BTHING_SHADOW_PUBLISHING, &s_ctx.state);
//         LOG(LL_INFO, ("TRIGGER SHADOW_PUBLISHING"));
//       }
//     }

//     // remove all keys from delta shadow
//     mg_bthing_shadow_empty((mgos_bvar_t)s_ctx.state.delta_shadow);

//     s_ctx.state.state_flags = MGOS_BTHING_STATE_FLAG_UNCHANGED;
//     s_ctx.last_event = 0;
//     return true;
//   }
//   return false;
// }

static bool mg_bthing_shadow_trigger_events() {
  if (s_ctx.last_event == 0) return false;
  
  if ((s_ctx.state.state_flags & MGOS_BTHING_STATE_FLAG_CHANGED) == MGOS_BTHING_STATE_FLAG_CHANGED) {
    // raise the SHADOW_CHANGED event
    mgos_event_trigger(MGOS_EV_BTHING_SHADOW_CHANGED, &s_ctx.state);
  }

  if ((s_ctx.state.state_flags & MGOS_BTHING_STATE_FLAG_PUBLISHING) == MGOS_BTHING_STATE_FLAG_PUBLISHING) {
    // raise the SHADOW_CHANGED event
    mgos_event_trigger(MGOS_EV_BTHING_SHADOW_PUBLISHING, &s_ctx.state);
  }  

  // remove all keys from delta shadow
  mg_bthing_shadow_empty((mgos_bvar_t)s_ctx.state.delta_shadow);

  s_ctx.state.state_flags = MGOS_BTHING_STATE_FLAG_UNCHANGED;
  s_ctx.last_event = 0;
  return true;
}

// static void mg_bthing_shadow_optimize_timer_cb(void *arg) {
//   if (mg_bthing_shadow_trigger_events(false)) {
//     // stop the optimizer timer
//     if (s_ctx.optimize_timer_id != MGOS_INVALID_TIMER_ID) {
//       mgos_clear_timer(s_ctx.optimize_timer_id);
//       s_ctx.optimize_timer_id = MGOS_INVALID_TIMER_ID;
//       LOG(LL_INFO, ("STOP OPTIMIZER"));
//     }
//   }
//   (void) arg;
// }

static void mg_bthing_shadow_optimize_timer_cb(void *arg) {
  if (!mg_bthing_shadow_trigger_events() && (s_ctx.optimize_timer_id != MGOS_INVALID_TIMER_ID))  {
    // stop the optimizer timer
    mgos_clear_timer(s_ctx.optimize_timer_id);
    s_ctx.optimize_timer_id = MGOS_INVALID_TIMER_ID;
    LOG(LL_INFO, ("STOP OPTIMIZER"));
  }
  (void) arg;
}

// static void mg_bthing_shadow_multiupdate_timer_cb(void *arg) {
//   if ((s_ctx.state.state_flags & MGOS_BTHING_STATE_FLAG_CHANGED) == MGOS_BTHING_STATE_FLAG_CHANGED ||
//       s_ctx.state.state_flags == MGOS_BTHING_STATE_FLAG_UNCHANGED) {
//     // A bThing state was chenged, so the function
//     // mg_bthing_shadow_trigger_events() is going to be invoked. or
//     // there are no changes to trigger. Anyway, I stop the timer.
//     mg_bthing_shadow_clear_optimize_timer();
//   } else if (mg_bthing_shadow_trigger_events(false)) {
//     // The timeout for optimizing/collecting multiple 
//     // STATE_UPDATED events was reached.
//     // Trigger events and stop the timer.
//     mg_bthing_shadow_clear_optimize_timer();
//   }
//   (void) arg;
// }

static void mg_bthing_shadow_on_state_changing(int ev, void *ev_data, void *userdata) {
  struct mgos_bthing_state_change *arg = (struct mgos_bthing_state_change *)ev_data;
  if (mg_bthing_shadow_get_state(s_ctx.state.delta_shadow, arg->thing) != NULL) {
    // the changed state was already queued into delta-shadow, so
    // I must flush the queue and trigger events before moving on
    mg_bthing_shadow_trigger_events();
    //mg_bthing_shadow_trigger_events(true);
  }

  (void) userdata;
  (void) ev;
}

// static void mg_bthing_shadow_on_state_changed(int ev, void *ev_data, void *userdata) {
//   struct mgos_bthing_state *arg = (struct mgos_bthing_state *)ev_data;
  
//   if (mg_bthing_shadow_must_ignore_item(arg->thing)) {
//     return; // the bThing must be ignored
//   }

//   s_ctx.last_event = mgos_uptime_micros();
//   s_ctx.state.state_flags |= MGOS_BTHING_STATE_FLAG_CHANGED;   

//   if (!mg_bthing_shadow_add_state((mgos_bvar_t)s_ctx.state.delta_shadow, arg->thing)) {
//     LOG(LL_ERROR, ("Something went wrong adding '%s' state to the delta-shadow on STATE_CHANGED event.",
//       mgos_bthing_get_uid(arg->thing)));
//   }

//   if (!s_ctx.optimize_enabled) {
//     // optimization is OFF, I must trigger events immediately
//     mg_bthing_shadow_trigger_events(true);
//   }

//   (void) userdata;
//   (void) ev;
// }

// static void mg_bthing_shadow_on_state_updated(int ev, void *ev_data, void *userdata) {
//   struct mgos_bthing_state *arg = (struct mgos_bthing_state *)ev_data;

//   if ((arg->state_flags & MGOS_BTHING_STATE_FLAG_CHANGED) == MGOS_BTHING_STATE_FLAG_CHANGED)
//     return; // already managed in mg_bthing_shadow_on_state_changed()
//   if (mg_bthing_shadow_must_ignore_item(arg->thing)) {
//     return; // the bThing must be ignored
//   }

//   s_ctx.last_event = mgos_uptime_micros();
//   s_ctx.state.state_flags |= MGOS_BTHING_STATE_FLAG_UPDATED;   

//   if (!mg_bthing_shadow_add_state((mgos_bvar_t)s_ctx.state.delta_shadow, arg->thing)) {
//     LOG(LL_ERROR, ("Something went wrong adding '%s' state to the delta-shadow on STATE_UPDATED event.",
//       mgos_bthing_get_uid(arg->thing)));
//   }

//   if (!s_ctx.optimize_enabled) {
//     // optimization is OFF, I must try to collect multiple 
//     // STATE_UPDATED event into a single one.
//     if (mg_bthing_shadow_start_optimize_timer(mg_bthing_shadow_multiupdate_timer_cb) != MGOS_INVALID_TIMER_ID) {
//       // The timer for collecting multiple STATE_UPDATED is started. Nothing to do.
//       return;
//     }
//     // The timer for collecting multiple STATE_UPDATED failed to start.
//     // I must trigger events immediately.
//     mg_bthing_shadow_trigger_events(true);
//   }

//   (void) arg;
//   (void) userdata;
//   (void) ev;
// }

// static void mg_bthing_shadow_on_state_updated(int ev, void *ev_data, void *userdata) {
//   struct mgos_bthing_state *arg = (struct mgos_bthing_state *)ev_data;

//   // ignore events from private instances
//   if (mg_bthing_has_flag(arg->thing, MG_BTHING_FLAG_ISPRIVATE)) return;

//   s_ctx.last_event = mgos_uptime_micros();

//   if ((arg->state_flags & MGOS_BTHING_STATE_FLAG_CHANGED) == MGOS_BTHING_STATE_FLAG_CHANGED) {
//     // set MGOS_BTHING_STATE_FLAG_CHANGED flag
//     s_ctx.state.state_flags |= MGOS_BTHING_STATE_FLAG_CHANGED;
//     // add the changed thing to the delta-shadow
//     if (!mg_bthing_shadow_add_state((mgos_bvar_t)s_ctx.state.delta_shadow, arg->thing)) {
//       LOG(LL_ERROR, ("Something went wrong adding '%s' state to the delta-shadow on STATE_UPDATED event.", mgos_bthing_get_uid(arg->thing)));
//     }
//   }

//   // set MGOS_BTHING_STATE_FLAG_UPDATED flag
//   s_ctx.state.state_flags |= MGOS_BTHING_STATE_FLAG_UPDATED;

//   // set MGOS_BTHING_STATE_FLAG_FORCED_PUB flag
//   if ((arg->state_flags & MGOS_BTHING_STATE_FLAG_FORCED_PUB) == MGOS_BTHING_STATE_FLAG_FORCED_PUB) {
//     s_ctx.state.state_flags |= MGOS_BTHING_STATE_FLAG_FORCED_PUB;
//     LOG(LL_INFO, ("IS FORCED!"));
//   }

//   if (s_ctx.optimize_enabled || ((s_ctx.state.state_flags & MGOS_BTHING_STATE_FLAG_FORCED_PUB) == MGOS_BTHING_STATE_FLAG_FORCED_PUB)) {
//     // Optimization is ON or a forced publish has been requsted.
//     // In both cases I must collect multiple STATE_UPDATED events
//     // into a single one. So I start the optimizer timer.
//     if (s_ctx.optimize_timer_id == MGOS_INVALID_TIMER_ID) {
//       LOG(LL_INFO, ("START OPTIMIZER"));
//       s_ctx.optimize_timer_id = mgos_set_timer(s_ctx.optimize_timeout, MGOS_TIMER_REPEAT, mg_bthing_shadow_optimize_timer_cb, NULL);
//       if (s_ctx.optimize_timer_id == MGOS_INVALID_TIMER_ID) {
//         // The timer for collecting multiple STATE_UPDATED events
//         // failed to start. I must trigger events immediately.
//         mg_bthing_shadow_trigger_events(true);
//       }
//     }
//   }

//   (void) arg;
//   (void) userdata;
//   (void) ev;
// }

static void mg_bthing_shadow_on_state_event(int ev, void *ev_data, void *userdata) {
  struct mgos_bthing_state *arg = (struct mgos_bthing_state *)ev_data;

  // ignore events from private instances
  if (mg_bthing_has_flag(arg->thing, MG_BTHING_FLAG_ISPRIVATE)) return;

  s_ctx.last_event = mgos_uptime_micros();

  bool forced_pub = false;
  if (ev == MGOS_EV_BTHING_STATE_CHANGED) {
    // set MGOS_BTHING_STATE_FLAG_CHANGED flag
    s_ctx.state.state_flags |= MGOS_BTHING_STATE_FLAG_CHANGED;
    // add the changed thing to the delta-shadow
    if (!mg_bthing_shadow_add_state((mgos_bvar_t)s_ctx.state.delta_shadow, arg->thing)) {
      LOG(LL_ERROR, ("Something went wrong adding '%s' state to the delta-shadow on MGOS_EV_BTHING_STATE_CHANGED event.",
        mgos_bthing_get_uid(arg->thing)));
    }
  } else if (ev == MGOS_EV_BTHING_STATE_PUBLISHING) {
    s_ctx.state.state_flags |= MGOS_BTHING_STATE_FLAG_PUBLISHING;
    forced_pub = ((arg->state_flags & MGOS_BTHING_STATE_FLAG_CHANGED) != MGOS_BTHING_STATE_FLAG_CHANGED);
  )

  if ((s_ctx.optimize_enabled || forced_pub) && (s_ctx.optimize_timer_id == MGOS_INVALID_TIMER_ID)) {
    // Optimization is ON or a forced publish has been requested.
    // In both cases I must collect multiple STATE_UPDATED events
    // into a single one. So I start the optimizer timer.
    s_ctx.optimize_timer_id = mgos_set_timer(s_ctx.optimize_timeout, MGOS_TIMER_REPEAT, mg_bthing_shadow_optimize_timer_cb, NULL);
    LOG(LL_INFO, ("START OPTIMIZER"));
  }

  if (s_ctx.optimize_timer_id == MGOS_INVALID_TIMER_ID) {
    mg_bthing_shadow_trigger_events();
  }

  (void) arg;
  (void) userdata;
  (void) ev;
}

#endif //MGOS_BTHING_HAVE_SENSORS

/* bool mgos_bthing_shadow_disable(mgos_bthing_t thing) {
  mg_bthing_shadow_remove_state((mgos_bvar_t)s_ctx.state.full_shadow, thing);
  return true;
}  */

#if MGOS_BTHING_HAVE_ACTUATORS

bool mgos_bthing_shadow_set(mgos_bvarc_t shadow) {
  if (shadow && mgos_bvar_is_dic(shadow)) {

    mgos_bthing_enum_t things = mgos_bthing_get_all();
    mgos_bthing_t thing;
    while (mgos_bthing_get_next(&things, &thing)) {
      if (mg_bthing_shadow_get_state(s_ctx.state.full_shadow, thing)) {
        // the bThing enabled for shadow
        mgos_bvarc_t state = mg_bthing_shadow_get_state(shadow, thing);
        if (state)
          mgos_bthing_set_state(thing, state);
      }
    }
    return true;
  }
  LOG(LL_ERROR, ("Invalid shadow. A bVariandDictionary is expected."));
  return false;
}

#ifdef MGOS_BVAR_HAVE_JSON

bool mgos_bthing_shadow_json_set(const char *json, int json_len){
  mgos_bvar_t shadow = NULL;
  if (mgos_bvar_json_try_bscanf(json, json_len, &shadow)) {
    bool ret = mgos_bthing_shadow_set(shadow);
    mgos_bvar_free(shadow);
    return ret;
  }
  LOG(LL_ERROR, ("Invalid json shadow format. A json object is expected."));
  return false;
}

#endif // MGOS_BVAR_HAVE_JSON
#endif //MGOS_BTHING_HAVE_ACTUATORS

bool mgos_bthing_shadow_init() {

  if (!mgos_sys_config_get_bthing_shadow_enable()) return true;

  // init context
  s_ctx.state.full_shadow = mgos_bvar_new_dic();
  s_ctx.state.delta_shadow = mgos_bvar_new_dic();
  s_ctx.state.state_flags = MGOS_BTHING_STATE_FLAG_UNCHANGED;
  s_ctx.optimize_enabled = mgos_sys_config_get_bthing_shadow_optimize();
  s_ctx.optimize_timer_id = MGOS_INVALID_TIMER_ID;
  s_ctx.last_event = 0;
  s_ctx.optimize_timeout = mgos_sys_config_get_bthing_shadow_optimize_timeout();
  if (s_ctx.optimize_timeout <= 0)
    s_ctx.optimize_timeout = MG_BTHING_SHADOW_OPTIMIZE_TIMEOUT;

  if (!mgos_event_register_base(MGOS_BTHING_SHADOW_EVENT_BASE, "bThing Shadow events")) return false;

  if (!mgos_event_add_handler(MGOS_EV_BTHING_CREATED, mg_bthing_shadow_on_created, NULL)) {
    LOG(LL_ERROR, ("Error registering MGOS_EV_BTHING_CREATED handler."));
    return false;
  }

  if (!mgos_event_add_handler(MGOS_EV_BTHING_MADE_PRIVATE, mg_bthing_shadow_on_made_private, NULL)) {
    LOG(LL_ERROR, ("Error registering MGOS_EV_BTHING_MADE_PRIVATE handler."));
    return false;
  } 
  
  #if MGOS_BTHING_HAVE_SENSORS
  if (!mgos_event_add_handler(MGOS_EV_BTHING_STATE_CHANGING, mg_bthing_shadow_on_state_changing, NULL)) {
    LOG(LL_ERROR, ("Error registering MGOS_EV_BTHING_STATE_CHANGING handler."));
    return false;
  }

  // if (!mgos_event_add_handler(MGOS_EV_BTHING_STATE_CHANGED, mg_bthing_shadow_on_state_changed, NULL)) {
  //   LOG(LL_ERROR, ("Error registering MGOS_EV_BTHING_STATE_CHANGED handler."));
  //   return false;
  // }

  // if (!mgos_event_add_handler(MGOS_EV_BTHING_STATE_UPDATED, mg_bthing_shadow_on_state_updated, NULL)) {
  //   LOG(LL_ERROR, ("Error registering MGOS_EV_BTHING_STATE_UPDATED handler."));
  //   return false;
  // }
  
  if (!mgos_event_add_handler(MGOS_EV_BTHING_STATE_CHANGED, mg_bthing_shadow_on_state_event, NULL)) {
    LOG(LL_ERROR, ("Error registering MGOS_EV_BTHING_STATE_CHANGED handler."));
    return false;
  }

  if (!mgos_event_add_handler(MGOS_EV_BTHING_STATE_PUBLISHING mg_bthing_shadow_on_state_event, NULL)) {
    LOG(LL_ERROR, ("Error registering MGOS_EV_BTHING_STATE_PUBLISHING handler."));
    return false;
  }

  // if (s_ctx.optimize_enabled) {
  //   if (mg_bthing_shadow_start_optimize_timer(mg_bthing_shadow_optimize_timer_cb) == MGOS_INVALID_TIMER_ID) {
  //     LOG(LL_DEBUG, ("Warning: unable to start the Shadow Optimizer."));
  //   } else {
  //     LOG(LL_DEBUG, ("Shadow Optimizer successfully stared (timeout %dms).", s_ctx.optimize_timeout));
  //   }
  // } else {
  //   LOG(LL_DEBUG, ("Shadow Optimizer disabled."));
  // }
  #endif //MGOS_BTHING_HAVE_SENSORS

  return true;
}