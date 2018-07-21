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
