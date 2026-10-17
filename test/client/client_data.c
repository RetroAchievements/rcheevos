#include "client_data.h"

RC_BEGIN_C_DECLS

#define GENERIC_ACHIEVEMENT_JSON(id, memaddr) "{\"ID\":" id ",\"Title\":\"Achievement " id "\"," \
      "\"Description\":\"Desc " id "\",\"Flags\":3,\"Points\":5,\"MemAddr\":\"" memaddr "\"," \
      "\"Author\":\"User1\",\"BadgeName\":\"00" id "\",\"Created\":1367266583,\"Modified\":1376929305}"

#define UNPROMOTED_ACHIEVEMENT_JSON(id, memaddr) "{\"ID\":" id ",\"Title\":\"Achievement " id "\"," \
      "\"Description\":\"Desc " id "\",\"Flags\":5,\"Points\":5,\"MemAddr\":\"" memaddr "\"," \
      "\"Author\":\"User1\",\"BadgeName\":\"00" id "\",\"Created\":1367266583,\"Modified\":1376929305}"

#define TYPED_ACHIEVEMENT_JSON(id, memaddr, type, rarity, rarity_hardcore) "{\"ID\":" id ",\"Title\":\"Achievement " id "\"," \
      "\"Description\":\"Desc " id "\",\"Flags\":3,\"Points\":5,\"MemAddr\":\"" memaddr "\"," \
      "\"Type\":\"" type "\",\"Rarity\":" rarity ",\"RarityHardcore\":" rarity_hardcore "," \
      "\"Author\":\"User1\",\"BadgeName\":\"00" id "\",\"Created\":1367266583,\"Modified\":1376929305}"

#define GENERIC_LEADERBOARD_JSON(id, memaddr, format) "{\"ID\":" id ",\"Title\":\"Leaderboard " id "\"," \
      "\"Description\":\"Desc " id "\",\"Mem\":\"" memaddr "\",\"Format\":\"" format "\",\"State\":\"active\"}"

#define HIDDEN_LEADERBOARD_JSON(id, memaddr, format) "{\"ID\":" id ",\"Title\":\"Leaderboard " id "\"," \
      "\"Description\":\"Desc " id "\",\"Mem\":\"" memaddr "\",\"Format\":\"" format "\",\"State\":\"active\",\"Hidden\":true}"

#define UNPROMOTED_LEADERBOARD_JSON(id, memaddr, format) "{\"ID\":" id ",\"Title\":\"Leaderboard " id "\"," \
      "\"Description\":\"Desc " id "\",\"Mem\":\"" memaddr "\",\"Format\":\"" format "\",\"State\":\"unpromoted\"}"

const char* patchdata_empty = "{\"Success\":true,"
    "\"GameId\":1234,\"Title\":\"Sample Game\",\"ConsoleId\":17,"
    "\"ImageIconUrl\":\"http://server/Images/112233.png\","
    "\"RichPresenceGameId\":1234,\"RichPresencePatch\":\"\",\"Sets\":[{"
      "\"AchievementSetId\":1111,\"GameId\":1234,\"Title\":null,\"Type\":\"core\","
      "\"ImageIconUrl\":\"http://server/Images/112233.png\","
      "\"Achievements\":[],"
      "\"Leaderboards\":[]"
    "}]}";

const char* patchdata_2ach_0lbd = "{\"Success\":true,"
    "\"GameId\":1234,\"Title\":\"Sample Game\",\"ConsoleId\":17,"
    "\"ImageIconUrl\":\"http://server/Images/112233.png\","
    "\"RichPresenceGameId\":1234,\"RichPresencePatch\":\"\",\"Sets\":[{"
      "\"AchievementSetId\":1111,\"GameId\":1234,\"Title\":null,\"Type\":\"core\","
      "\"ImageIconUrl\":\"http://server/Images/112233.png\","
      "\"Achievements\":["
       "{\"ID\":5501,\"Title\":\"Ach1\",\"Description\":\"Desc1\",\"Flags\":3,\"Points\":5,"
        "\"MemAddr\":\"0xH0001=3_0xH0002=7\",\"Author\":\"User1\",\"BadgeName\":\"00234\","
        "\"Created\":1367266583,\"Modified\":1376929305},"
       "{\"ID\":5502,\"Title\":\"Ach2\",\"Description\":\"Desc2\",\"Flags\":3,\"Points\":2,"
        "\"MemAddr\":\"0xH0001=2_0x0002=9\",\"Author\":\"User1\",\"BadgeName\":\"00235\","
        "\"Created\":1376970283,\"Modified\":1376970283}"
      "],"
      "\"Leaderboards\":[]"
    "}]}";

const char* patchdata_2ach_1lbd = "{\"Success\":true,"
    "\"GameId\":1234,\"Title\":\"Sample Game\",\"ConsoleId\":17,"
    "\"ImageIconUrl\":\"http://server/Images/112233.png\","
    "\"RichPresenceGameId\":1234,\"RichPresencePatch\":\"\",\"Sets\":[{"
      "\"AchievementSetId\":1111,\"GameId\":1234,\"Title\":null,\"Type\":\"core\","
      "\"ImageIconUrl\":\"http://server/Images/112233.png\","
      "\"Achievements\":["
       "{\"ID\":5501,\"Title\":\"Ach1\",\"Description\":\"Desc1\",\"Flags\":3,\"Points\":5,"
        "\"MemAddr\":\"0xH0001=3_0xH0002=7\",\"Author\":\"User1\",\"BadgeName\":\"00234\","
        "\"Created\":1367266583,\"Modified\":1376929305},"
       "{\"ID\":5502,\"Title\":\"Ach2\",\"Description\":\"Desc2\",\"Flags\":3,\"Points\":2,"
        "\"MemAddr\":\"0xH0001=2_0x0002=9\",\"Author\":\"User1\",\"BadgeName\":\"00235\","
        "\"Created\":1376970283,\"Modified\":1376970283}"
      "],"
      "\"Leaderboards\":["
       "{\"ID\":4401,\"Title\":\"Leaderboard1\",\"Description\":\"Desc1\","
        "\"Mem\":\"STA:0xH000C=1::CAN:0xH000D=1::SUB:0xH000D=2::VAL:0x 000E\",\"Format\":\"SCORE\"}"
      "]"
    "}]}";

const char* patchdata_rich_presence_only = "{\"Success\":true,"
    "\"GameId\":1234,\"Title\":\"Sample Game\",\"ConsoleId\":17,"
    "\"ImageIconUrl\":\"http://server/Images/112233.png\","
    "\"RichPresenceGameId\":1234,\"RichPresencePatch\":\"Display:\\r\\n@Number(0xH0001)\","
    "\"Sets\":[{"
      "\"AchievementSetId\":1111,\"GameId\":1234,\"Title\":null,\"Type\":\"core\","
      "\"ImageIconUrl\":\"http://server/Images/112233.png\","
      "\"Achievements\":[],"
      "\"Leaderboards\":[]"
    "}]}";

const char* patchdata_leaderboard_only = "{\"Success\":true,"
    "\"GameId\":1234,\"Title\":\"Sample Game\",\"ConsoleId\":17,"
    "\"ImageIconUrl\":\"http://server/Images/112233.png\","
    "\"RichPresenceGameId\":1234,\"RichPresencePatch\":\"Display:\\r\\n@Number(0xH0001)\","
    "\"Sets\":[{"
      "\"AchievementSetId\":1111,\"GameId\":1234,\"Title\":null,\"Type\":\"core\","
      "\"ImageIconUrl\":\"http://server/Images/112233.png\","
      "\"Achievements\":[],"
      "\"Leaderboards\":["
        GENERIC_LEADERBOARD_JSON("44", "STA:0xH000B=1::CAN:0xH000C=1::SUB:0xH000D=1::VAL:0x 000E", "SCORE")
      "]"
    "}]}";

const char* patchdata_leaderboard_immediate_submit = "{\"Success\":true,"
    "\"GameId\":1234,\"Title\":\"Sample Game\",\"ConsoleId\":17,"
    "\"ImageIconUrl\":\"http://server/Images/112233.png\","
    "\"RichPresenceGameId\":1234,\"RichPresencePatch\":\"Display:\\r\\n@Number(0xH0001)\","
    "\"Sets\":[{"
      "\"AchievementSetId\":1111,\"GameId\":1234,\"Title\":null,\"Type\":\"core\","
      "\"ImageIconUrl\":\"http://server/Images/112233.png\","
      "\"Achievements\":[],"
      "\"Leaderboards\":["
        GENERIC_LEADERBOARD_JSON("44", "STA:0xH000B=1::CAN:0=1::SUB:1=1::VAL:0x 000E", "SCORE")
      "]"
    "}]}";

const char* patchdata_bounds_check_system = "{\"Success\":true,"
    "\"GameId\":1234,\"Title\":\"Sample Game\",\"ConsoleId\":7,"
    "\"ImageIconUrl\":\"http://server/Images/112233.png\","
    "\"RichPresenceGameId\":1234,\"RichPresencePatch\":\"\",\"Sets\":[{"
      "\"AchievementSetId\":1111,\"GameId\":1234,\"Title\":null,\"Type\":\"core\","
      "\"ImageIconUrl\":\"http://server/Images/112233.png\","
      "\"Achievements\":["
        GENERIC_ACHIEVEMENT_JSON("1", "0xH0000=5") ","
        GENERIC_ACHIEVEMENT_JSON("2", "0xHFFFF=5") ","
        GENERIC_ACHIEVEMENT_JSON("3", "0xH10000=5") ","
        GENERIC_ACHIEVEMENT_JSON("4", "0x FFFE=5") ","
        GENERIC_ACHIEVEMENT_JSON("5", "0x FFFF=5") ","
        GENERIC_ACHIEVEMENT_JSON("6", "0x 10000=5") ","
        GENERIC_ACHIEVEMENT_JSON("7", "I:0xH0000_0xHFFFF=5")
      "],"
      "\"Leaderboards\":[]"
    "}]}";

const char* patchdata_bounds_check_8 = "{\"Success\":true,"
    "\"GameId\":1234,\"Title\":\"Sample Game\",\"ConsoleId\":7,"
    "\"ImageIconUrl\":\"http://server/Images/112233.png\","
    "\"RichPresenceGameId\":1234,\"RichPresencePatch\":\"\",\"Sets\":[{"
      "\"AchievementSetId\":1111,\"GameId\":1234,\"Title\":null,\"Type\":\"core\","
      "\"ImageIconUrl\":\"http://server/Images/112233.png\","
      "\"Achievements\":["
        GENERIC_ACHIEVEMENT_JSON("408", "0xH0004=5") ","
        GENERIC_ACHIEVEMENT_JSON("508", "0xH0005=5") ","
        GENERIC_ACHIEVEMENT_JSON("608", "0xH0006=5") ","
        GENERIC_ACHIEVEMENT_JSON("708", "0xH0007=5") ","
        GENERIC_ACHIEVEMENT_JSON("808", "0xH0008=5") ","
        GENERIC_ACHIEVEMENT_JSON("416", "0x 0004=5") ","
        GENERIC_ACHIEVEMENT_JSON("516", "0x 0005=5") ","
        GENERIC_ACHIEVEMENT_JSON("616", "0x 0006=5") ","
        GENERIC_ACHIEVEMENT_JSON("716", "0x 0007=5") ","
        GENERIC_ACHIEVEMENT_JSON("816", "0x 0008=5") ","
        GENERIC_ACHIEVEMENT_JSON("424", "0xW0004=5") ","
        GENERIC_ACHIEVEMENT_JSON("524", "0xW0005=5") ","
        GENERIC_ACHIEVEMENT_JSON("624", "0xW0006=5") ","
        GENERIC_ACHIEVEMENT_JSON("724", "0xW0007=5") ","
        GENERIC_ACHIEVEMENT_JSON("824", "0xW0008=5") ","
        GENERIC_ACHIEVEMENT_JSON("432", "0xX0004=5") ","
        GENERIC_ACHIEVEMENT_JSON("532", "0xX0005=5") ","
        GENERIC_ACHIEVEMENT_JSON("632", "0xX0006=5") ","
        GENERIC_ACHIEVEMENT_JSON("732", "0xX0007=5") ","
        GENERIC_ACHIEVEMENT_JSON("832", "0xX0008=5")
      "],"
      "\"Leaderboards\":[]"
    "}]}";

const char* patchdata_exhaustive = "{\"Success\":true,"
    "\"GameId\":1234,\"Title\":\"Sample Game\",\"ConsoleId\":17,"
    "\"ImageIconUrl\":\"http://server/Images/112233.png\","
    "\"RichPresenceGameId\":1234,\"RichPresencePatch\":\"Display:\\r\\nPoints:@Number(0xH0003)\\r\\n\","
    "\"Sets\":[{"
      "\"AchievementSetId\":1111,\"GameId\":1234,\"Title\":null,\"Type\":\"core\","
      "\"ImageIconUrl\":\"http://server/Images/112233.png\","
      "\"Achievements\":["
        GENERIC_ACHIEVEMENT_JSON("5", "0xH0005=5") ","
        GENERIC_ACHIEVEMENT_JSON("6", "M:0xH0006=6") ","
        GENERIC_ACHIEVEMENT_JSON("7", "T:0xH0007=7_0xH0001=1") ","
        GENERIC_ACHIEVEMENT_JSON("8", "0xH0008=8") ","
        GENERIC_ACHIEVEMENT_JSON("9", "0xH0009=9") ","
        GENERIC_ACHIEVEMENT_JSON("70", "M:0xX0010=100000") ","
        GENERIC_ACHIEVEMENT_JSON("71", "G:0xX0010=100000")
      "],"
      "\"Leaderboards\":["
        GENERIC_LEADERBOARD_JSON("44", "STA:0xH000B=1::CAN:0xH000C=1::SUB:0xH000D=1::VAL:0x 000E", "SCORE") ","
        GENERIC_LEADERBOARD_JSON("45", "STA:0xH000A=1::CAN:0xH000C=2::SUB:0xH000D=1::VAL:0xH000E", "SCORE") ","   /* different size */
        GENERIC_LEADERBOARD_JSON("46", "STA:0xH000A=1::CAN:0xH000C=3::SUB:0xH000D=1::VAL:0x 000E", "VALUE") ","   /* different format */
        GENERIC_LEADERBOARD_JSON("47", "STA:0xH000A=1::CAN:0xH000C=4::SUB:0xH000D=2::VAL:0x 000E", "SCORE") ","   /* different submit */
        GENERIC_LEADERBOARD_JSON("48", "STA:0xH000A=2::CAN:0xH000C=5::SUB:0xH000D=1::VAL:0x 000E", "SCORE") ","   /* different start */
        GENERIC_LEADERBOARD_JSON("51", "STA:0xH000A=3::CAN:0xH000C=6::SUB:0xH000D=1::VAL:M:0xH0009=1", "VALUE") "," /* hit count */
        GENERIC_LEADERBOARD_JSON("52", "STA:0xH000B=3::CAN:0xH000C=7::SUB:0xH000D=1::VAL:M:0xH0009=1", "VALUE")     /* hit count */
      "]"
    "}]}";


const char* patchdata_exhaustive_typed = "{\"Success\":true,"
    "\"GameId\":1234,\"Title\":\"Sample Game\",\"ConsoleId\":17,"
    "\"ImageIconUrl\":\"http://server/Images/112233.png\","
    "\"RichPresenceGameId\":1234,\"RichPresencePatch\":\"Display:\\r\\nPoints:@Number(0xH0003)\\r\\n\","
    "\"Sets\":[{"
      "\"AchievementSetId\":1111,\"GameId\":1234,\"Title\":null,\"Type\":\"core\","
      "\"ImageIconUrl\":\"http://server/Images/112233.png\","
      "\"Achievements\":["
        TYPED_ACHIEVEMENT_JSON("5", "0xH0005=5", "", "100.0", "99.5") ","
        TYPED_ACHIEVEMENT_JSON("6", "M:0xH0006=6", "progression", "95.3", "84.7") ","
        TYPED_ACHIEVEMENT_JSON("7", "T:0xH0007=7_0xH0001=1", "missable", "47.6", "38.2") ","
        TYPED_ACHIEVEMENT_JSON("8", "0xH0008=8", "progression", "86.0", "73.1") ","
        TYPED_ACHIEVEMENT_JSON("9", "0xH0009=9", "win_condition", "81.4", "66.4") ","
        TYPED_ACHIEVEMENT_JSON("70", "M:0xX0010=100000", "missable", "11.4", "6.3") ","
        TYPED_ACHIEVEMENT_JSON("71", "G:0xX0010=100000", "", "8.7", "3.8")
      "],"
      "\"Leaderboards\":["
        GENERIC_LEADERBOARD_JSON("44", "STA:0xH000B=1::CAN:0xH000C=1::SUB:0xH000D=1::VAL:0x 000E", "SCORE") ","
        GENERIC_LEADERBOARD_JSON("45", "STA:0xH000A=1::CAN:0xH000C=2::SUB:0xH000D=1::VAL:0xH000E", "SCORE") ","   /* different size */
        GENERIC_LEADERBOARD_JSON("46", "STA:0xH000A=1::CAN:0xH000C=3::SUB:0xH000D=1::VAL:0x 000E", "VALUE") ","   /* different format */
        GENERIC_LEADERBOARD_JSON("47", "STA:0xH000A=1::CAN:0xH000C=4::SUB:0xH000D=2::VAL:0x 000E", "SCORE") ","   /* different submit */
        GENERIC_LEADERBOARD_JSON("48", "STA:0xH000A=2::CAN:0xH000C=5::SUB:0xH000D=1::VAL:0x 000E", "SCORE") ","   /* different start */
        GENERIC_LEADERBOARD_JSON("51", "STA:0xH000A=3::CAN:0xH000C=6::SUB:0xH000D=1::VAL:M:0xH0009=1", "VALUE") "," /* hit count */
        GENERIC_LEADERBOARD_JSON("52", "STA:0xH000B=3::CAN:0xH000C=7::SUB:0xH000D=1::VAL:M:0xH0009=1", "VALUE")     /* hit count */
      "]"
    "}]}";

const char* patchdata_big_ids = "{\"Success\":true,"
    "\"GameId\":1234,\"Title\":\"Sample Game\",\"ConsoleId\":17,"
    "\"ImageIconUrl\":\"http://server/Images/112233.png\","
    "\"RichPresenceGameId\":1234,\"RichPresencePatch\":\"Display:\\r\\nPoints:@Number(0xH0003)\\r\\n\","
    "\"Sets\":[{"
      "\"AchievementSetId\":1111,\"GameId\":1234,\"Title\":null,\"Type\":\"core\","
      "\"ImageIconUrl\":\"http://server/Images/112233.png\","
      "\"Achievements\":["
        GENERIC_ACHIEVEMENT_JSON("5", "0xH0005=5") ","
        GENERIC_ACHIEVEMENT_JSON("6", "M:0xH0006=6") ","
        GENERIC_ACHIEVEMENT_JSON("4294967295", "0xH0009=9") "," /* UINT_MAX */
        GENERIC_ACHIEVEMENT_JSON("71", "G:0xX0010=100000")
      "],"
      "\"Leaderboards\":[]"
    "}]}";

const char* patchdata_leaderboards_hidden = "{\"Success\":true,"
    "\"GameId\":1234,\"Title\":\"Sample Game\",\"ConsoleId\":17,"
    "\"ImageIconUrl\":\"http://server/Images/112233.png\","
    "\"RichPresenceGameId\":1234,\"RichPresencePatch\":\"Display:\\r\\nPoints:@Number(0xH0003)\\r\\n\","
    "\"Sets\":[{"
      "\"AchievementSetId\":1111,\"GameId\":1234,\"Title\":null,\"Type\":\"core\","
      "\"ImageIconUrl\":\"http://server/Images/112233.png\","
      "\"Achievements\":[],"
      "\"Leaderboards\":["
        GENERIC_LEADERBOARD_JSON("44", "STA:0xH000B=1::CAN:0xH000C=1::SUB:0xH000D=1::VAL:0x 000E", "SCORE") ","
        HIDDEN_LEADERBOARD_JSON("45", "STA:0xH000A=1::CAN:0xH000C=2::SUB:0xH000D=1::VAL:0xH000E", "SCORE") ","
        GENERIC_LEADERBOARD_JSON("46", "STA:0xH000A=1::CAN:0xH000C=3::SUB:0xH000D=1::VAL:0x 000E", "VALUE") ","
        GENERIC_LEADERBOARD_JSON("47", "STA:0xH000A=1::CAN:0xH000C=4::SUB:0xH000D=2::VAL:0x 000E", "SCORE") ","
        HIDDEN_LEADERBOARD_JSON("48", "STA:0xH000A=2::CAN:0xH000C=5::SUB:0xH000D=1::VAL:0x 000E", "SCORE")
      "]"
    "}]}";

const char* patchdata_leaderboards_only_hidden = "{\"Success\":true,"
    "\"GameId\":1234,\"Title\":\"Sample Game\",\"ConsoleId\":17,"
    "\"ImageIconUrl\":\"http://server/Images/112233.png\","
    "\"RichPresenceGameId\":1234,\"RichPresencePatch\":\"Display:\\r\\nPoints:@Number(0xH0003)\\r\\n\","
    "\"Sets\":[{"
      "\"AchievementSetId\":1111,\"GameId\":1234,\"Title\":null,\"Type\":\"core\","
      "\"ImageIconUrl\":\"http://server/Images/112233.png\","
      "\"Achievements\":[],"
      "\"Leaderboards\":["
        HIDDEN_LEADERBOARD_JSON("45", "STA:0xH000A=1::CAN:0xH000C=2::SUB:0xH000D=1::VAL:0xH000E", "SCORE") ","
        HIDDEN_LEADERBOARD_JSON("48", "STA:0xH000A=2::CAN:0xH000C=5::SUB:0xH000D=1::VAL:0x 000E", "SCORE")
      "]"
    "}]}";



const char* patchdata_leaderboards_unpromoted = "{\"Success\":true,"
    "\"GameId\":1234,\"Title\":\"Sample Game\",\"ConsoleId\":17,"
    "\"ImageIconUrl\":\"http://server/Images/112233.png\","
    "\"RichPresenceGameId\":1234,\"RichPresencePatch\":\"Display:\\r\\nPoints:@Number(0xH0003)\\r\\n\","
    "\"Sets\":[{"
      "\"AchievementSetId\":1111,\"GameId\":1234,\"Title\":null,\"Type\":\"core\","
      "\"ImageIconUrl\":\"http://server/Images/112233.png\","
      "\"Achievements\":[],"
      "\"Leaderboards\":["
        GENERIC_LEADERBOARD_JSON("44", "STA:0xH000B=1::CAN:0xH000C=1::SUB:0xH000D=1::VAL:0x 000E", "SCORE") ","
        UNPROMOTED_LEADERBOARD_JSON("45", "STA:0xH000A=1::CAN:0xH000C=2::SUB:0xH000D=1::VAL:0xH000E", "SCORE") ","
        GENERIC_LEADERBOARD_JSON("46", "STA:0xH000A=1::CAN:0xH000C=3::SUB:0xH000D=1::VAL:0x 000E", "VALUE") ","
        GENERIC_LEADERBOARD_JSON("47", "STA:0xH000A=1::CAN:0xH000C=4::SUB:0xH000D=2::VAL:0x 000E", "SCORE") ","
        UNPROMOTED_LEADERBOARD_JSON("48", "STA:0xH000A=2::CAN:0xH000C=5::SUB:0xH000D=1::VAL:0x 000E", "SCORE")
      "]"
    "}]}";

const char* patchdata_unpromoted_unsupported = "{\"Success\":true,"
    "\"GameId\":1234,\"Title\":\"Sample Game\",\"ConsoleId\":17,"
    "\"ImageIconUrl\":\"http://server/Images/112233.png\","
    "\"RichPresenceGameId\":1234,\"RichPresencePatch\":\"Display:\\r\\nPoints:@Number(0xH0003)\\r\\n\","
    "\"Sets\":[{"
      "\"AchievementSetId\":1111,\"GameId\":1234,\"Title\":null,\"Type\":\"core\","
      "\"ImageIconUrl\":\"http://server/Images/112233.png\","
      "\"Achievements\":["
       "{\"ID\":5501,\"Title\":\"Ach1\",\"Description\":\"Desc1\",\"Flags\":3,\"Points\":5,"
        "\"MemAddr\":\"0xH0001=1_0xH0002=7\",\"Author\":\"User1\",\"BadgeName\":\"00234\","
        "\"Created\":1367266583,\"Modified\":1376929305},"
       "{\"ID\":5502,\"Title\":\"Ach2\",\"Description\":\"Desc2\",\"Flags\":5,\"Points\":2,"
        "\"MemAddr\":\"0xH0001=2_0x0002=9\",\"Author\":\"User1\",\"BadgeName\":\"00235\","
        "\"Created\":1376970283,\"Modified\":1376970283},"
       "{\"ID\":5503,\"Title\":\"Ach3\",\"Description\":\"Desc3\",\"Flags\":3,\"Points\":2,"
        "\"MemAddr\":\"0xHFEFEFEFE=2_0x0002=9\",\"Author\":\"User1\",\"BadgeName\":\"00236\","
        "\"Created\":1376971283,\"Modified\":1376971283}"
      "],"
      "\"Leaderboards\":["
       "{\"ID\":4401,\"Title\":\"Leaderboard1\",\"Description\":\"Desc1\","
        "\"Mem\":\"STA:0xH000C=1::CAN:0xH000D=1::SUB:0xHFEFEFEFE=2::VAL:0x 000E\",\"Format\":\"SCORE\"}"
      "]"
    "}]}";

const char* patchdata_warning = "{\"Success\":true,"
    "\"GameId\":1234,\"Title\":\"Sample Game\",\"ConsoleId\":17,"
    "\"ImageIconUrl\":\"http://server/Images/112233.png\","
    "\"RichPresenceGameId\":1234,\"RichPresencePatch\":\"\",\"Sets\":[{"
      "\"AchievementSetId\":1111,\"GameId\":1234,\"Title\":null,\"Type\":\"core\","
      "\"ImageIconUrl\":\"http://server/Images/112233.png\","
      "\"Achievements\":["
       "{\"ID\":5501,\"Title\":\"Ach1\",\"Description\":\"Desc1\",\"Flags\":3,\"Points\":5,"
        "\"MemAddr\":\"0xH0001=3_0xH0002=7\",\"Author\":\"User1\",\"BadgeName\":\"00234\","
        "\"Created\":1367266583,\"Modified\":1376929305},"
       "{\"ID\":5502,\"Title\":\"Ach2\",\"Description\":\"Desc2\",\"Flags\":3,\"Points\":2,"
        "\"MemAddr\":\"0xH0001=2_0x0002=9\",\"Author\":\"User1\",\"BadgeName\":\"00235\","
        "\"Created\":1376970283,\"Modified\":1376970283},"
       "{\"ID\":101000001,\"Title\":\"Warning: Unsupported Emulator\",\"Description\":\"Hardcore unlocks cannot be earned using this emulator.\",\"Flags\":3,\"Points\":0,"
        "\"MemAddr\":\"1=1.300.\",\"Author\":\"\",\"BadgeName\":\"00000\","
        "\"Created\":1376970283,\"Modified\":1376970283}"
      "],"
      "\"Leaderboards\":[]"
    "}]}";

const char* patchdata_subset = "{\"Success\":true,"
    "\"GameId\":1234,\"Title\":\"Sample Game\",\"ConsoleId\":17,"
    "\"ImageIconUrl\":\"http://server/Images/112233.png\","
    "\"RichPresenceGameId\":1234,\"RichPresencePatch\":\"Display:\\r\\nPoints:@Number(0xH0003)\\r\\n\","
    "\"Sets\":[{"
      "\"AchievementSetId\":1111,\"GameId\":1234,\"Title\":null,\"Type\":\"core\","
      "\"ImageIconUrl\":\"http://server/Images/112233.png\","
      "\"Achievements\":["
        GENERIC_ACHIEVEMENT_JSON("5", "0xH0005=5") ","
        GENERIC_ACHIEVEMENT_JSON("6", "M:0xH0006=6") ","
        GENERIC_ACHIEVEMENT_JSON("7", "T:0xH0007=7_0xH0001=1") ","
        GENERIC_ACHIEVEMENT_JSON("8", "0xH0008=8") ","
        GENERIC_ACHIEVEMENT_JSON("9", "0xH0009=9") ","
        GENERIC_ACHIEVEMENT_JSON("70", "M:0xX0010=100000") ","
        GENERIC_ACHIEVEMENT_JSON("71", "G:0xX0010=100000")
      "],"
      "\"Leaderboards\":["
        GENERIC_LEADERBOARD_JSON("44", "STA:0xH000B=1::CAN:0xH000C=1::SUB:0xH000D=1::VAL:0x 000E", "SCORE") ","
        GENERIC_LEADERBOARD_JSON("45", "STA:0xH000A=1::CAN:0xH000C=2::SUB:0xH000D=1::VAL:0xH000E", "SCORE") ","   /* different size */
        GENERIC_LEADERBOARD_JSON("46", "STA:0xH000A=1::CAN:0xH000C=3::SUB:0xH000D=1::VAL:0x 000E", "VALUE") ","   /* different format */
        GENERIC_LEADERBOARD_JSON("47", "STA:0xH000A=1::CAN:0xH000C=4::SUB:0xH000D=2::VAL:0x 000E", "SCORE") ","   /* different submit */
        GENERIC_LEADERBOARD_JSON("48", "STA:0xH000A=2::CAN:0xH000C=5::SUB:0xH000D=1::VAL:0x 000E", "SCORE") ","   /* different start */
        GENERIC_LEADERBOARD_JSON("51", "STA:0xH000A=3::CAN:0xH000C=6::SUB:0xH000D=1::VAL:M:0xH0009=1", "VALUE") "," /* hit count */
        GENERIC_LEADERBOARD_JSON("52", "STA:0xH000B=3::CAN:0xH000C=7::SUB:0xH000D=1::VAL:M:0xH0009=1", "VALUE")     /* hit count */
      "]"
    "},{"
      "\"AchievementSetId\":2345,\"GameId\":1235,\"Title\":\"Bonus\",\"Type\":\"bonus\","
      "\"ImageIconUrl\":\"http://server/Images/112234.png\","
      "\"Achievements\":["
        GENERIC_ACHIEVEMENT_JSON("5501", "0xH0017=7") ","
        GENERIC_ACHIEVEMENT_JSON("5502", "0xH0018=8") ","
        GENERIC_ACHIEVEMENT_JSON("5503", "0xH0019=9")
      "],"
      "\"Leaderboards\":["
        GENERIC_LEADERBOARD_JSON("81", "STA:0xH0008=1::CAN:0xH000C=1::SUB:0xH000D=1::VAL:0x 000E", "SCORE") ","
        GENERIC_LEADERBOARD_JSON("82", "STA:0xH0008=2::CAN:0xH000C=1::SUB:0xH000D=1::VAL:0x 000E", "SCORE")
      "]"
    "}]}";

const char* patchdata_subset_unpromoted_unsupported = "{\"Success\":true,"
    "\"GameId\":1234,\"Title\":\"Sample Game\",\"ConsoleId\":17,"
    "\"ImageIconUrl\":\"http://server/Images/112233.png\","
    "\"RichPresenceGameId\":1234,\"RichPresencePatch\":\"Display:\\r\\nPoints:@Number(0xH0003)\\r\\n\","
    "\"Sets\":[{"
      "\"AchievementSetId\":1111,\"GameId\":1234,\"Title\":null,\"Type\":\"core\","
      "\"ImageIconUrl\":\"http://server/Images/112233.png\","
      "\"Achievements\":["
        GENERIC_ACHIEVEMENT_JSON("5", "0xH0005=5") ","
        GENERIC_ACHIEVEMENT_JSON("6", "M:0xH0006=6") ","
        GENERIC_ACHIEVEMENT_JSON("7", "T:0xH0007=7_0xH0001=1") ","
        UNPROMOTED_ACHIEVEMENT_JSON("8", "0xH0008=8") ","
        GENERIC_ACHIEVEMENT_JSON("9", "0xH0009=9") ","
        GENERIC_ACHIEVEMENT_JSON("70", "M:0xX0010=100000") ","
        GENERIC_ACHIEVEMENT_JSON("71", "G:0xX0010=100000")
      "],"
      "\"Leaderboards\":["
        GENERIC_LEADERBOARD_JSON("44", "STA:0xH000B=1::CAN:0xH000C=1::SUB:0xH000D=1::VAL:0x 000E", "SCORE") ","
        GENERIC_LEADERBOARD_JSON("45", "STA:0xH000A=1::CAN:0xH000C=2::SUB:0xH000D=1::VAL:0xH000E", "SCORE") ","   /* different size */
        GENERIC_LEADERBOARD_JSON("46", "STA:0xH000A=1::CAN:0xH000C=3::SUB:0xH000D=1::VAL:0x 000E", "VALUE") ","   /* different format */
        GENERIC_LEADERBOARD_JSON("47", "STA:0xH000A=1::CAN:0xH000C=4::SUB:0xH000D=2::VAL:0x 000E", "SCORE") ","   /* different submit */
        GENERIC_LEADERBOARD_JSON("48", "STA:0xH000A=2::CAN:0xH000C=5::SUB:0xH000D=1::VAL:0x 000E", "SCORE") ","   /* different start */
        GENERIC_LEADERBOARD_JSON("51", "STA:0xH000A=3::CAN:0xH000C=6::SUB:0xH000D=1::VAL:M:0xH0009=1", "VALUE") "," /* hit count */
        GENERIC_LEADERBOARD_JSON("52", "STA:0xH000B=3::CAN:0xH000C=7::SUB:0xH000D=1::VAL:M:0xH0009=1", "VALUE")     /* hit count */
      "]"
    "},{"
      "\"AchievementSetId\":2345,\"GameId\":1235,\"Title\":\"Bonus\",\"Type\":\"bonus\","
      "\"ImageIconUrl\":\"http://server/Images/112234.png\","
      "\"Achievements\":["
        GENERIC_ACHIEVEMENT_JSON("5501", "0xH0017=7") ","
        UNPROMOTED_ACHIEVEMENT_JSON("5502", "0xH0018=8") ","
        GENERIC_ACHIEVEMENT_JSON("5503", "0xHFEFEFEFE=9")
      "],"
      "\"Leaderboards\":["
        GENERIC_LEADERBOARD_JSON("81", "STA:0xH0008=1::CAN:0xH000C=1::SUB:0xH000D=1::VAL:0x 000E", "SCORE") ","
        GENERIC_LEADERBOARD_JSON("82", "STA:0xH0008=2::CAN:0xH000C=1::SUB:0xH000D=1::VAL:0x 000E", "SCORE")
      "]"
    "}]}";

const char* patchdata_alternate = "{\"Success\":true,"
    "\"GameId\":2345,\"Title\":\"Other Game\",\"ConsoleId\":7,"
    "\"ImageIconUrl\":\"http://server/Images/555555.png\","
    "\"RichPresenceGameId\":2345,\"RichPresencePatch\":\"\",\"Sets\":[{"
      "\"AchievementSetId\":2222,\"GameId\":2345,\"Title\":null,\"Type\":\"core\","
      "\"ImageIconUrl\":\"http://server/Images/555555.png\","
      "\"Achievements\":["
        GENERIC_ACHIEVEMENT_JSON("1", "0xH0000=5") ","
        GENERIC_ACHIEVEMENT_JSON("2", "0xHFFFF=5") ","
        GENERIC_ACHIEVEMENT_JSON("3", "0xH10000=5")
      "],"
      "\"Leaderboards\":[]"
    "}]}";

const char* patchdata_not_found = "{\"Success\":false,\"Error\":\"Unknown game\",\"Code\":\"not_found\",\"Status\":404}";

const char* no_unlocks = "{\"Success\":true,\"Unlocks\":[],\"HardcoreUnlocks\":[]}";

/* startsession API only returns HardcoreUnlocks if an achievement has been earned in hardcore,
 * even if the casual unlock has a different timestamp */
const char* unlock_5501h_and_5502 = "{\"Success\":true,\"Unlocks\":["
      "{\"ID\":5502,\"When\":1234567899}"
    "],\"HardcoreUnlocks\":["
      "{\"ID\":5501,\"When\":1234567890}"
    "]}";
const char* unlock_5501_and_5502 = "{\"Success\":true,\"HardcoreUnlocks\":["
      "{\"ID\":5501,\"When\":1234567890},"
      "{\"ID\":5502,\"When\":1234567899}"
    "]}";
const char* unlock_5501_5502_and_5503 = "{\"Success\":true,\"HardcoreUnlocks\":["
      "{\"ID\":5501,\"When\":1234567890},"
      "{\"ID\":5502,\"When\":1234567899},"
      "{\"ID\":5503,\"When\":1234567999}"
    "]}";
const char* unlock_8 = "{\"Success\":true,\"HardcoreUnlocks\":[{\"ID\":8,\"When\":1234567890}]}";
const char* unlock_8_and_5502 = "{\"Success\":true,\"HardcoreUnlocks\":["
      "{\"ID\":8,\"When\":1234567890},"
      "{\"ID\":5502,\"When\":1234567890}"
    "]}";
const char* unlock_6_8h_and_9 = "{\"Success\":true,\"Unlocks\":["
      "{\"ID\":6,\"When\":1234567890},"
      "{\"ID\":9,\"When\":1234567899}"
    "],\"HardcoreUnlocks\":["
      "{\"ID\":8,\"When\":1234567895}"
    "]}";

const char* response_429 =
    "<html>\n"
    "<head><title>429 Too Many Requests</title></head>\n"
    "<body>\n"
    "<center><h1>429 Too Many Requests</h1></center>\n"
    "<hr><center>nginx</center>\n"
    "</body>\n"
    "</html>";

const char* response_502 =
    "<html>\n"
    "<head><title>502 Bad Gateway</title></head>\n"
    "<body>\n"
    "<center><h1>502 Bad Gateway</h1></center>\n"
    "<hr><center>nginx</center>\n"
    "</body>\n"
    "</html>";

const char* response_503 =
    "<html>\n"
    "<head><title>503 Service Temporarily Unavailable</title></head>\n"
    "<body>\n"
    "<center><h1>503 Service Temporarily Unavailable</h1></center>\n"
    "<hr><center>nginx</center>\n"
    "</body>\n"
    "</html>";

const char* default_game_badge = "https://media.retroachievements.org/Images/000001.png";

RC_END_C_DECLS
