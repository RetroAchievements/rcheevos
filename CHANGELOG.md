# v9.0.0

* new size: RC_MEMSIZE_BITCOUNT
* new flag: RC_CONDITION_OR_NEXT
* new flag: RC_CONDITION_TRIGGER
* new flag: RC_CONDITION_MEASURED_IF
* new operators: RC_OPERATOR_MULT / RC_OPERATOR_DIV
* is_bcd removed from memref - now part of RC_MEMSIZE
* add rc_runtime_t and associated functions
* add rc_hash_ functions
* add rc_error_str function
* add game_hash parameter to rc_url_award_cheevo
* remove hash parameter from rc_url_submit_lboard
* add rc_url_ping function
* add rc_console_ functions

# v8.1.0

* new flag: RC_CONDITION_MEASURED
* new flag: RC_CONDITION_ADD_ADDRESS
* add rc_evaluate_trigger - extended version of rc_test_trigger with more granular return codes
* make rc_evaluate_value return a signed int (was unsigned int)
* new formats: RC_FORMAT_MINUTES and RC_FORMAT_SECONDS_AS_MINUTES
* removed " Points" text from RC_FORMAT_SCORE format
* removed RC_FORMAT_OTHER format. "OTHER" format now parses to RC_FORMAT_SCORE
* bugfix: AddHits will now honor AndNext on previous condition

# v8.0.1

* bugfix: prevent null reference exception if rich presence contains condition without display string
* bugfix: 24-bit read from memory should only read 24-bits

# v8.0.0

* support for prior operand type
* support for AndNext condition flag
* support for rich presence
* bugfix: update delta/prior memory values while group is paused
* bugfix: allow floating point number without leading 0
* bugfix: support empty alt groups

# v7.1.1

* Address signed/unsigned mismatch warnings

# v7.1.0

* Added the RC_DISABLE_LUA macro to compile rcheevos without Lua support

# v7.0.2

* Make sure the code is C89-compliant
* Use 32-bit types in Lua
* Only evaluate Lua operands when the Lua state is not `NULL`

# v7.0.1

* Fix the alignment of memory allocations

# v7.0.0

* Removed **rjson**

# v6.5.0

* Added a schema for errors returned by the server

# v6.4.0

* Added an enumeration with the console identifiers used in RetroAchievements

# v6.3.1

* Pass the peek function and the user data to the Lua functions used in operands.

# v6.3.0

* Added **rurl**, an API to build URLs to access RetroAchievements web services.

# v6.2.0

* Added **rjson**, an API to easily decode RetroAchievements JSON files into C structures.

# v6.1.0

* Added support for 24-bit operands with the `'W'` prefix (`RC_OPERAND_24_BITS`)

# v6.0.2

* Only define RC_ALIGNMENT if it has not been already defined

# v6.0.1

* Use `sizeof(void*)` as a better default for `RC_ALIGNMENT`

# v6.0.0

* Simplified API: separate functions to get the buffer size and to parse `memaddr` into the provided buffer
* Fixed crash trying to call `rc_update_condition_pause` during a dry-run
* The callers are now responsible to pass down a scratch buffer to avoid accesses to out-of-scope memory

# v5.0.0

* Pre-compute if a condition has a pause condition in its group
* Added a pre-computed flag that tells if the condition set has at least one pause condition
* Removed the link to the previous condition in a condition set chain

# v4.0.0

* Fixed `ret` not being properly initialized in `rc_parse_trigger`
* Build the unit tests with optimizations and `-Wall` to help catch more issues
* Added `extern "C"` around the inclusion of the Lua headers so that **rcheevos** can be compiled cleanly as C++
* Exposed `rc_parse_value` and `rc_evaluate_value` to be used with rich presence
* Removed the `reset` and `dirty` flags from the external API

# v3.2.0

* Added the ability to reset triggers and leaderboards
* Add a function to parse a format string and return the format enum, and some unit tests for it

# v3.1.0

* Added `rc_format_value` to the API

# v3.0.1

* Fixed wrong 32-bit value on 64-bit platforms

# v3.0.0

* Removed function rc_evaluate_value from the API

# v2.0.0

* Removed leaderboard callbacks in favor of a simpler scheme

# v1.1.2

* Fixed NULL pointer deference when there's an error during the parse

# v1.1.1

* Removed unwanted garbage
* Should be v1.0.1 :/

# v1.0.0

* First version
