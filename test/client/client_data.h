#ifndef RHASH_TEST_CLIENT_DATA_H
#define RHASH_TEST_CLIENT_DATA_H

#include "rc_export.h"

RC_BEGIN_C_DECLS

extern const char* patchdata_empty;
extern const char* patchdata_2ach_0lbd;
extern const char* patchdata_2ach_1lbd;
extern const char* patchdata_rich_presence_only;
extern const char* patchdata_leaderboard_only;
extern const char* patchdata_leaderboard_immediate_submit;
extern const char* patchdata_bounds_check_system;
extern const char* patchdata_bounds_check_8;
extern const char* patchdata_exhaustive;
extern const char* patchdata_exhaustive_typed;
extern const char* patchdata_big_ids;
extern const char* patchdata_leaderboards_hidden;
extern const char* patchdata_leaderboards_only_hidden;
extern const char* patchdata_leaderboards_unpromoted;
extern const char* patchdata_unpromoted_unsupported;
extern const char* patchdata_warning;
extern const char* patchdata_subset;
extern const char* patchdata_subset_unpromoted_unsupported;
extern const char* patchdata_alternate;
extern const char* patchdata_not_found;

extern const char* no_unlocks;
extern const char* unlock_5501h_and_5502;
extern const char* unlock_5501_and_5502;
extern const char* unlock_5501_5502_and_5503;
extern const char* unlock_8;
extern const char* unlock_8_and_5502;
extern const char* unlock_6_8h_and_9;

extern const char* response_429;
extern const char* response_502;
extern const char* response_503;

extern const char* default_game_badge;

RC_END_C_DECLS

#endif /* RHASH_TEST_CLIENT_DATA_H */
