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
  int64_t last_update;
} s_ctx;

static void mg_bthing_shadow_on_created(int ev, void *ev_data, void *userdata) {
  if (ev == MGOS_EV_BTHING_CREATED) {
    #if MGOS_BTHING_HAVE_SENSORS
    const char *key = mgos_bthing_get_id((mgos_bthing_t)ev_data);
    if (!mgos_bvar_add_key((mgos_bvar_t)s_ctx.state.full_shadow, key, (mgos_bvar_t)mg_bthing_get_raw_state((mgos_bthing_t)ev_data))) {
      LOG(LL_ERROR, ("Error including '%s' state to the full shadow.", key));
    }
    #else
    (void) ev_data;
    #endif
  }
  
  (void) userdata;
}

#if MGOS_BTHING_HAVE_SENSORS

bool mg_bthing_shadow_optimize_timeout_reached() {
  return ((s_ctx.last_update != 0 &&
         (mg_bthing_duration_micro(s_ctx.last_update, mgos_uptime_micros()) / 1000) >= s_ctx.optimize_timeout));
}

static int mg_bthing_shadow_start_optimize_timer(timer_callback cb) {
  if (s_ctx.optimize_timer_id != MGOS_INVALID_TIMER_ID) return s_ctx.optimize_timer_id;
  if (s_ctx.optimize_timeout > 0) {
    s_ctx.optimize_timer_id = mgos_set_timer(s_ctx.optimize_timeout, MGOS_TIMER_REPEAT, cb, NULL);
  }
  return s_ctx.optimize_timer_id;
}

static void mg_bthing_shadow_clear_optimize_timer() {
  if (s_ctx.optimize_timer_id != MGOS_INVALID_TIMER_ID) {
    mgos_clear_timer(s_ctx.optimize_timer_id);
    s_ctx.optimize_timer_id = MGOS_INVALID_TIMER_ID;
  }
}

static bool mg_bthing_shadow_trigger_events(bool force) {
  if (force || mg_bthing_shadow_optimize_timeout_reached()) {
    
    if ((s_ctx.state.state_flags & MGOS_BTHING_STATE_FLAG_CHANGED) == MGOS_BTHING_STATE_FLAG_CHANGED) {
      // raise the SHADOW_CHANGED event
      mgos_event_trigger(MGOS_EV_BTHING_SHADOW_CHANGED, &s_ctx.state);
    }

    // raise the SHADOW_UPDATED event
    s_ctx.state.state_flags |= MGOS_BTHING_STATE_FLAG_UPDATED;
    mgos_event_trigger(MGOS_EV_BTHING_SHADOW_UPDATED, &s_ctx.state);

    // remove all keys from delta shadow
    mgos_bvar_remove_keys((mgos_bvar_t)s_ctx.state.delta_shadow);

    s_ctx.state.state_flags = MGOS_BTHING_STATE_FLAG_UNCHANGED;
    s_ctx.last_update = 0;
    return true;
  }
  return false;
}

static void mg_bthing_shadow_multiupdate_timer_cb(void *arg) {
  LOG(LL_INFO, ("INFO: entering into mg_bthing_shadow_multiupdate_timer_cb()...")); // CANCEL
  if ((s_ctx.state.state_flags & MGOS_BTHING_STATE_FLAG_CHANGED) == MGOS_BTHING_STATE_FLAG_CHANGED ||
      s_ctx.state.state_flags == MGOS_BTHING_STATE_FLAG_UNCHANGED){
    // A bThing state was chenged, so the function
    // mg_bthing_shadow_trigger_events() is going to be invoked. or
    // there are no changes to trigger. Anyway, I stop the timer.
    mg_bthing_shadow_clear_optimize_timer();
    LOG(LL_INFO, ("WARN: multiupdate_timer aborted because a change.")); // CANCEL
  } else if (mg_bthing_shadow_trigger_events(false)) {
    // The timeout for optimizing/collecting multiple 
    // STATE_UPDATED events was reached.
    // Trigger events and stop the timer.
    mg_bthing_shadow_clear_optimize_timer();
    LOG(LL_INFO, ("INFO: multiupdate_timer aborted because events have been triggered.")); // CANCEL
  }

  (void) arg;
}

static void mg_bthing_shadow_on_state_changing(int ev, void *ev_data, void *userdata) {
  struct mgos_bthing_state_change *arg = (struct mgos_bthing_state_change *)ev_data;
  if (mgos_bvar_has_key(s_ctx.state.delta_shadow, mgos_bthing_get_id(arg->thing))) {
    // the changed state was already queued into delta-shadow, so
    // I must flush the queue and trigger events before moving on
    mg_bthing_shadow_trigger_events(true);
  }

  (void) userdata;
  (void) ev;
}

static void mg_bthing_shadow_on_state_updated(int ev, void *ev_data, void *userdata) {
  struct mgos_bthing_state *arg = (struct mgos_bthing_state *)ev_data;
  
  // check if the bThing must be ignored or not...
  const char *key = mgos_bthing_get_id(arg->thing);
  if (!mgos_bvar_has_key(s_ctx.state.full_shadow, key)) {
    return; // the bThing must be ignored
  }

  s_ctx.last_update = mgos_uptime_micros();

  if ((arg->state_flags & MGOS_BTHING_STATE_FLAG_UPD_REQUESTED) == MGOS_BTHING_STATE_FLAG_UPD_REQUESTED)
    s_ctx.state.state_flags |= MGOS_BTHING_STATE_FLAG_UPD_REQUESTED;
  
  if ((arg->state_flags & MGOS_BTHING_STATE_FLAG_CHANGED) == MGOS_BTHING_STATE_FLAG_CHANGED)
    s_ctx.state.state_flags |= MGOS_BTHING_STATE_FLAG_CHANGED;   

  if ((s_ctx.state.state_flags & MGOS_BTHING_STATE_FLAG_CHANGED) == MGOS_BTHING_STATE_FLAG_CHANGED ||
      (s_ctx.state.state_flags & MGOS_BTHING_STATE_FLAG_UPD_REQUESTED) == MGOS_BTHING_STATE_FLAG_UPD_REQUESTED) {
    mgos_bvar_add_key((mgos_bvar_t)s_ctx.state.delta_shadow, key, (mgos_bvar_t)arg->state);
  }

  if (!s_ctx.optimize_enabled) {
    // optimization is OFF
    if (((s_ctx.state.state_flags & MGOS_BTHING_STATE_FLAG_CHANGED) != MGOS_BTHING_STATE_FLAG_CHANGED)) {
      s_ctx.state.state_flags |= MGOS_BTHING_STATE_FLAG_UPDATED;
      // There is no state's change, so I try to optimize/collect
      // multiple STATE_UPDATED events into one single event.
      LOG(LL_INFO, ("INFO: multiupdate detected...")); // CANCEL
      if (mg_bthing_shadow_start_optimize_timer(mg_bthing_shadow_multiupdate_timer_cb) != MGOS_INVALID_TIMER_ID) {
        LOG(LL_INFO, ("managing in into the timer.")); // CANCEL
        // The timer for optimizing/collecting multiple 
        // STATE_UPDATED is started. Nothing to do.
        return;
      }
    }
    // A bThing state is changed, or the timer for 
    // optimizing/collecting multiple STATE_UPDATED
    // failed to start. I trigger events immediately
    mg_bthing_shadow_trigger_events(true);
  }

  (void) arg;
  (void) userdata;
  (void) ev;
}

static void mg_bthing_shadow_optimize_timer_cb(void *arg) {
  mg_bthing_shadow_trigger_events(false);
  (void) arg;
}

#endif //MGOS_BTHING_HAVE_SENSORS

bool mgos_bthing_shadow_disable(mgos_bthing_t thing) {
  const char *key = mgos_bthing_get_id(thing);
  if (mgos_bvar_has_key(s_ctx.state.full_shadow, key)) {
    if (mgos_bvar_remove_key((mgos_bvar_t)s_ctx.state.full_shadow, key) == NULL) {
      LOG(LL_ERROR, ("Error excluding '%s' state from the full shadow.", key));
      return false;
    }
  }
  return true;
}

#if MGOS_BTHING_HAVE_ACTUATORS

bool mgos_bthing_shadow_set(mgos_bvarc_t shadow) {
  if (shadow && mgos_bvar_is_dic(shadow)) {
    const char *thing_id;
    mgos_bvarc_t key_val;
    mgos_bvarc_enum_t keys = mgos_bvarc_get_keys(shadow);
    while (mgos_bvarc_get_next_key(&keys, &key_val, &thing_id)) {
      if (mgos_bvar_has_key(s_ctx.state.full_shadow, thing_id)) {
        mgos_bthing_t thing = mgos_bthing_get(thing_id);
        mgos_bthing_set_state(thing, key_val);
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
  s_ctx.last_update = 0;
  s_ctx.optimize_timeout = mgos_sys_config_get_bthing_shadow_optimize_timeout();
  if (s_ctx.optimize_timeout <= 0)
    s_ctx.optimize_timeout = MG_BTHING_SHADOW_OPTIMIZE_TIMEOUT;

  if (!mgos_event_register_base(MGOS_BTHING_SHADOW_EVENT_BASE, "bThing Shadow events")) return false;

   if (!mgos_event_add_handler(MGOS_EV_BTHING_CREATED, mg_bthing_shadow_on_created, NULL)) {
    LOG(LL_ERROR, ("Error registering MGOS_EV_BTHING_CREATED handler."));
    return false;
  }
  
  #if MGOS_BTHING_HAVE_SENSORS
  if (!mgos_event_add_handler(MGOS_EV_BTHING_STATE_CHANGING, mg_bthing_shadow_on_state_changing, NULL)) {
    LOG(LL_ERROR, ("Error registering MGOS_EV_BTHING_STATE_CHANGING handler."));
    return false;
  }

  if (!mgos_event_add_handler(MGOS_EV_BTHING_STATE_UPDATED, mg_bthing_shadow_on_state_updated, NULL)) {
    LOG(LL_ERROR, ("Error registering MGOS_EV_BTHING_STATE_UPDATED handler."));
    return false;
  }
  
  if (s_ctx.optimize_enabled) {
    if (mg_bthing_shadow_start_optimize_timer(mg_bthing_shadow_optimize_timer_cb) == MGOS_INVALID_TIMER_ID) {
      LOG(LL_DEBUG, ("Warning: unable to start the Shadow Optimizer."));
    } else {
      LOG(LL_DEBUG, ("Shadow Optimizer successfully stared (timeout %dms).", s_ctx.optimize_timeout));
    }
  } else {
    LOG(LL_DEBUG, ("Shadow Optimizer disabled."));
  }
  #endif //MGOS_BTHING_HAVE_SENSORS

  return true;
}