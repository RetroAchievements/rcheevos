#include "rurl.h"

#ifdef RARCH_INTERNAL
#include <libretro-common/include/rhash.h>
#define md5_state_t MD5_CTX
#define md5_byte_t unsigned char
#define md5_init(state) MD5_Init(state)
#define md5_append(state, buffer, size) MD5_Update(state, buffer, size)
#define md5_finish(state, hash) MD5_Final(hash, state)
#else
#include "..\rhash\md5.h"
#endif

#include <stdio.h>

static int rc_url_encode(char* encoded, size_t len, const char* str) {
  for (;;) {
    switch (*str) {
      case 'a': case 'b': case 'c': case 'd': case 'e': case 'f': case 'g': case 'h': case 'i': case 'j':
      case 'k': case 'l': case 'm': case 'n': case 'o': case 'p': case 'q': case 'r': case 's': case 't':
      case 'u': case 'v': case 'w': case 'x': case 'y': case 'z':
      case 'A': case 'B': case 'C': case 'D': case 'E': case 'F': case 'G': case 'H': case 'I': case 'J':
      case 'K': case 'L': case 'M': case 'N': case 'O': case 'P': case 'Q': case 'R': case 'S': case 'T':
      case 'U': case 'V': case 'W': case 'X': case 'Y': case 'Z':
      case '0': case '1': case '2': case '3': case '4': case '5': case '6': case '7': case '8': case '9':
      case '-': case '_': case '.': case '~':
        if (len >= 2) {
          *encoded++ = *str++;
          len--;
        }
        else {
          return -1;
        }

        break;
      
      default:
        if (len >= 4) {
          snprintf(encoded, len, "%%%02x", (unsigned char)*str);
          encoded += 3;
          str++;
          len -= 3;
        }
        else {
          return -1;
        }

        break;
      
      case 0:
        *encoded = 0;
        return 0;
    }
  }
}

int rc_url_award_cheevo(char* buffer, size_t size, const char* user_name, const char* login_token,
                        unsigned cheevo_id, int hardcore, const char* game_hash) {
  char urle_user_name[64];
  char urle_login_token[64];
  int written;

  if (rc_url_encode(urle_user_name, sizeof(urle_user_name), user_name) != 0) {
    return -1;
  }
  
  if (rc_url_encode(urle_login_token, sizeof(urle_login_token), login_token) != 0) {
    return -1;
  }
  
  written = snprintf(
    buffer,
    size,
    "http://retroachievements.org/dorequest.php?r=awardachievement&u=%s&t=%s&a=%u&h=%d",
    urle_user_name,
    urle_login_token,
    cheevo_id,
    hardcore ? 1 : 0
  );

  if (game_hash && strlen(game_hash) == 32 && (size - (size_t)written) >= 35) {
     written += snprintf(buffer + written, size - (size_t)written, "&m=%s", game_hash);
  }

  return (size_t)written >= size ? -1 : 0;
}

int rc_url_submit_lboard(char* buffer, size_t size, const char* user_name, const char* login_token, unsigned lboard_id, int value) {
  char urle_user_name[64];
  char urle_login_token[64];
  char signature[64];
  unsigned char hash[16];
  md5_state_t state;
  int written;

  if (rc_url_encode(urle_user_name, sizeof(urle_user_name), user_name) != 0) {
    return -1;
  }
  
  if (rc_url_encode(urle_login_token, sizeof(urle_login_token), login_token) != 0) {
    return -1;
  }

  /* Evaluate the signature. */
  snprintf(signature, sizeof(signature), "%u%s%u", lboard_id, user_name, lboard_id);
  md5_init(&state);
  md5_append(&state, (unsigned char*)signature, (int)strlen(signature));
  md5_finish(&state, hash);

  written = snprintf(
    buffer,
    size,
    "http://retroachievements.org/dorequest.php?r=submitlbentry&u=%s&t=%s&i=%u&s=%d&v=%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x",
    urle_user_name,
    urle_login_token,
    lboard_id,
    value,
    hash[ 0], hash[ 1], hash[ 2], hash[ 3], hash[ 4], hash[ 5], hash[ 6], hash[ 7],
    hash[ 8], hash[ 9], hash[10], hash[11],hash[12], hash[13], hash[14], hash[15]
  );

  return (size_t)written >= size ? -1 : 0;
}

int rc_url_get_gameid(char* buffer, size_t size, const char* hash) {
  int written = snprintf(
    buffer,
    size,
    "http://retroachievements.org/dorequest.php?r=gameid&m=%s",
    hash
  );

  return (size_t)written >= size ? -1 : 0;
}

int rc_url_get_patch(char* buffer, size_t size, const char* user_name, const char* login_token, unsigned gameid) {
  char urle_user_name[64];
  char urle_login_token[64];
  int written;

  if (rc_url_encode(urle_user_name, sizeof(urle_user_name), user_name) != 0) {
    return -1;
  }
  
  if (rc_url_encode(urle_login_token, sizeof(urle_login_token), login_token) != 0) {
    return -1;
  }
  
  written = snprintf(
    buffer,
    size,
    "http://retroachievements.org/dorequest.php?r=patch&u=%s&t=%s&g=%u",
    urle_user_name,
    urle_login_token,
    gameid
  );

  return (size_t)written >= size ? -1 : 0;
}

int rc_url_get_badge_image(char* buffer, size_t size, const char* badge_name) {
  int written = snprintf(
    buffer,
    size,
    "http://i.retroachievements.org/Badge/%s",
    badge_name
  );

  return (size_t)written >= size ? -1 : 0;
}

int rc_url_login_with_password(char* buffer, size_t size, const char* user_name, const char* password) {
  char urle_user_name[64];
  char urle_password[64];
  int written;

  if (rc_url_encode(urle_user_name, sizeof(urle_user_name), user_name) != 0) {
    return -1;
  }
  
  if (rc_url_encode(urle_password, sizeof(urle_password), password) != 0) {
    return -1;
  }
  
  written = snprintf(
    buffer,
    size,
    "http://retroachievements.org/dorequest.php?r=login&u=%s&p=%s",
    urle_user_name,
    urle_password
  );

  return (size_t)written >= size ? -1 : 0;
}

int rc_url_login_with_token(char* buffer, size_t size, const char* user_name, const char* login_token) {
  char urle_user_name[64];
  char urle_login_token[64];
  int written;

  if (rc_url_encode(urle_user_name, sizeof(urle_user_name), user_name) != 0) {
    return -1;
  }
  
  if (rc_url_encode(urle_login_token, sizeof(urle_login_token), login_token) != 0) {
    return -1;
  }
  
  written = snprintf(
    buffer,
    size,
    "http://retroachievements.org/dorequest.php?r=login&u=%s&t=%s",
    urle_user_name,
    urle_login_token
  );

  return (size_t)written >= size ? -1 : 0;
}

int rc_url_get_unlock_list(char* buffer, size_t size, const char* user_name, const char* login_token, unsigned gameid, int hardcore) {
  char urle_user_name[64];
  char urle_login_token[64];
  int written;

  if (rc_url_encode(urle_user_name, sizeof(urle_user_name), user_name) != 0) {
    return -1;
  }
  
  if (rc_url_encode(urle_login_token, sizeof(urle_login_token), login_token) != 0) {
    return -1;
  }
  
  written = snprintf(
    buffer,
    size,
    "http://retroachievements.org/dorequest.php?r=unlocks&u=%s&t=%s&g=%u&h=%d",
    urle_user_name,
    urle_login_token,
    gameid,
    hardcore ? 1 : 0
  );

  return (size_t)written >= size ? -1 : 0;
}

int rc_url_post_playing(char* buffer, size_t size, const char* user_name, const char* login_token, unsigned gameid) {
  char urle_user_name[64];
  char urle_login_token[64];
  int written;

  if (rc_url_encode(urle_user_name, sizeof(urle_user_name), user_name) != 0) {
    return -1;
  }
  
  if (rc_url_encode(urle_login_token, sizeof(urle_login_token), login_token) != 0) {
    return -1;
  }
  
  written = snprintf(
    buffer,
    size,
    "http://retroachievements.org/dorequest.php?r=postactivity&u=%s&t=%s&a=3&m=%u",
    urle_user_name,
    urle_login_token,
    gameid
  );

  return (size_t)written >= size ? -1 : 0;
}

static int rc_url_append_num(char* buffer, size_t buffer_size, size_t* buffer_offset, const char* param, unsigned value) {
  size_t written = 0;

  if (!buffer_offset || *buffer_offset > buffer_size)
    return -1;

  if (*buffer_offset)
  {
    buffer += *buffer_offset;
    buffer_size -= *buffer_offset;

    if (buffer[-1] != '?') {
      *buffer++ = '&';
      buffer_size--;
      written = 1;
    }
  }

  written += (size_t)snprintf(buffer, buffer_size, "%s=%u", param, value);
  *buffer_offset += written;
  return 0;
}

static int rc_url_append_str(char* buffer, size_t buffer_size, size_t* buffer_offset, const char* param, const char* value) {
  size_t written = 0;
  size_t param_written;

  if (!buffer_offset || *buffer_offset >= buffer_size)
    return -1;

  if (*buffer_offset)
  {
    buffer += *buffer_offset;
    buffer_size -= *buffer_offset;

    if (buffer[-1] != '?') {
      *buffer++ = '&';
      buffer_size--;
      written = 1;
    }
  }

  param_written = (size_t)snprintf(buffer, buffer_size, "%s=", param);

  written += param_written;
  if (written > buffer_size)
    return -1;

  buffer += param_written;
  buffer_size -= param_written;

  if (rc_url_encode(buffer, buffer_size, value) != 0)
    return -1;

  written += strlen(buffer);
  *buffer_offset += written;
  return 0;
}

int rc_url_ping(char* url_buffer, size_t url_buffer_size, char* post_buffer, size_t post_buffer_size,
                const char* user_name, const char* login_token, unsigned gameid, const char* rich_presence) {
  int success = 0;
  size_t written;

  written = (size_t)snprintf(url_buffer, url_buffer_size, "http://retroachievements.org/dorequest.php");
  if (written >= url_buffer_size)
    return -1;

  written = 0;
  success |= rc_url_append_str(post_buffer, post_buffer_size, &written, "r", "ping");
  success |= rc_url_append_str(post_buffer, post_buffer_size, &written, "u", user_name);
  success |= rc_url_append_str(post_buffer, post_buffer_size, &written, "t", login_token);
  success |= rc_url_append_num(post_buffer, post_buffer_size, &written, "g", gameid);

  if (rich_presence && *rich_presence)
    success |= rc_url_append_str(post_buffer, post_buffer_size, &written, "m", rich_presence);

  return success;
}
