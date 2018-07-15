#include "internal.h"

enum {
  RC_LBOARD_START    = 1 << 0,
  RC_LBOARD_CANCEL   = 1 << 1,
  RC_LBOARD_SUBMIT   = 1 << 2,
  RC_LBOARD_VALUE    = 1 << 3,
  RC_LBOARD_PROGRESS = 1 << 4,
  RC_LBOARD_COMPLETE = RC_LBOARD_START | RC_LBOARD_CANCEL | RC_LBOARD_SUBMIT | RC_LBOARD_VALUE
};

rc_lboard_t* rc_parse_lboard(int* ret, void* buffer, const char* memaddr, lua_State* L, int funcs_ndx) {
  rc_lboard_t* self, dummy;
  rc_value_t dummy_value;
  int found;

  *ret = 0;
  self = (rc_lboard_t*)rc_alloc(buffer, ret, sizeof(rc_lboard_t), &dummy);
  self->progress = 0;
  found = 0;

  for (;;)
  {
    if ((memaddr[0] == 's' || memaddr[0] == 'S') &&
        (memaddr[1] == 't' || memaddr[1] == 'T') &&
        (memaddr[2] == 'a' || memaddr[2] == 'A') && memaddr[3] == ':') {
      if ((found & RC_LBOARD_START) != 0) {
        *ret = RC_DUPLICATED_START;
        return 0;
      }

      found |= RC_LBOARD_START;
      memaddr += 4;
      rc_parse_trigger_internal(&self->start, ret, buffer, &memaddr, L, funcs_ndx);

      if (*ret < 0) {
        return 0;
      }
    }
    else if ((memaddr[0] == 'c' || memaddr[0] == 'C') &&
             (memaddr[1] == 'a' || memaddr[1] == 'A') &&
             (memaddr[2] == 'n' || memaddr[2] == 'N') && memaddr[3] == ':') {
      if ((found & RC_LBOARD_CANCEL) != 0) {
        *ret = RC_DUPLICATED_CANCEL;
        return 0;
      }

      found |= RC_LBOARD_CANCEL;
      memaddr += 4;
      rc_parse_trigger_internal(&self->cancel, ret, buffer, &memaddr, L, funcs_ndx);

      if (*ret < 0) {
        return 0;
      }
    }
    else if ((memaddr[0] == 's' || memaddr[0] == 'S') &&
             (memaddr[1] == 'u' || memaddr[1] == 'U') &&
             (memaddr[2] == 'b' || memaddr[2] == 'B') && memaddr[3] == ':') {
      if ((found & RC_LBOARD_SUBMIT) != 0) {
        *ret = RC_DUPLICATED_SUBMIT;
        return 0;
      }

      found |= RC_LBOARD_SUBMIT;
      memaddr += 4;
      rc_parse_trigger_internal(&self->submit, ret, buffer, &memaddr, L, funcs_ndx);

      if (*ret < 0) {
        return 0;
      }
    }
    else if ((memaddr[0] == 'v' || memaddr[0] == 'V') &&
             (memaddr[1] == 'a' || memaddr[1] == 'A') &&
             (memaddr[2] == 'l' || memaddr[2] == 'L') && memaddr[3] == ':') {
      if ((found & RC_LBOARD_VALUE) != 0) {
        *ret = RC_DUPLICATED_VALUE;
        return 0;
      }

      found |= RC_LBOARD_VALUE;
      memaddr += 4;
      rc_parse_value(&self->value, ret, buffer, &memaddr, L, funcs_ndx);

      if (*ret < 0) {
        return 0;
      }
    }
    else if ((memaddr[0] == 'p' || memaddr[0] == 'P') &&
             (memaddr[1] == 'r' || memaddr[1] == 'R') &&
             (memaddr[2] == 'o' || memaddr[2] == 'O') && memaddr[3] == ':') {
      if ((found & RC_LBOARD_PROGRESS) != 0) {
        *ret = RC_DUPLICATED_PROGRESS;
        return 0;
      }

      found |= RC_LBOARD_PROGRESS;
      memaddr += 4;

      self->progress = (rc_value_t*)rc_alloc(buffer, ret, sizeof(rc_value_t), &dummy_value);
      rc_parse_value(self->progress, ret, buffer, &memaddr, L, funcs_ndx);

      if (*ret < 0) {
        return 0;
      }
    }
    else {
      *ret = RC_INVALID_LBOARD_FIELD;
      return 0;
    }

    if (memaddr[0] != ':' || memaddr[1] != ':') {
      break;
    }

    memaddr += 2;
  }

  if ((found & RC_LBOARD_COMPLETE) != RC_LBOARD_COMPLETE) {
    if ((found & RC_LBOARD_START) == 0) {
      *ret = RC_MISSING_START;
    }
    else if ((found & RC_LBOARD_CANCEL) == 0) {
      *ret = RC_MISSING_CANCEL;
    }
    else if ((found & RC_LBOARD_SUBMIT) == 0) {
      *ret = RC_MISSING_SUBMIT;
    }
    else if ((found & RC_LBOARD_VALUE) == 0) {
      *ret = RC_MISSING_VALUE;
    }

    return 0;
  }

  self->started = self->submitted = 0;
  self->value_score = self->progress_score = 0;
  return self;
}

void rc_evaluate_lboard(rc_lboard_t* self, void* callback_ud, rc_peek_t peek, void* peek_ud, lua_State* L) {
  int unused1, unused2, start_ok, cancel_ok, submit_ok;

  /* ASSERT: these are always tested once every frame, to ensure delta variables work properly */
  unused1 = unused2 = 0;
  start_ok = rc_test_trigger(&self->start, &unused1, &unused2, peek, peek_ud, L);

  unused1 = unused2 = 0;
  cancel_ok = rc_test_trigger(&self->cancel, &unused1, &unused2, peek, peek_ud, L);

  unused1 = unused2 = 0;
  submit_ok = rc_test_trigger(&self->submit, &unused1, &unused2, peek, peek_ud, L);

  /* Update value and progress */
  self->value_score = rc_evaluate_value(&self->value, peek, peek_ud, L);

  if (self->progress == 0) {
    self->progress_score = self->value_score;
  }
  else {
    self->progress_score = rc_evaluate_value(self->progress, peek, peek_ud, L);
  }

  if (self->submitted) {
    /* if we've already submitted or canceled the leaderboard, don't reactivate it until it becomes inactive. */
    if (!start_ok) {
      self->submitted = 0;
    }
  }
  else if (!self->started) {
    /* leaderboard is not active, if the start condition is true, activate it */
    if (start_ok && !cancel_ok) {
      if (submit_ok) {
        /* start and submit both true in the same frame, just submit without announcing the leaderboard is available */
        self->submit_cb(self, callback_ud);
        /* prevent multiple submissions/notifications */
        self->submitted = 1;
      }
      else if (self->start.requirement != 0 || self->start.alternative != 0) {
        self->started = 1;
        self->start_cb(self, callback_ud);
      }
    }
  }
  else {
    /* leaderboard is active */
    if (cancel_ok) {
      /* cancel condition is true, deactivate the leaderboard */
      self->started = 0;
      self->cancel_cb(self, callback_ud);
      /* prevent multiple cancel notifications */
      self->submitted = 1;
    }
    else if (submit_ok) {
      /* submit condition is true, submit the current value */
      self->started = 0;
      self->submit_cb(self, callback_ud);
      self->submitted = 1;
    }
  }
}
