#pragma once

/**
 * @brief time structure for storing both the real time and game time
 * for any time representation. (i.e. splits, segments, wr etc)
 */
typedef struct ls_time {
    long long real_time; /*!< Real time means the actual real world elapsed time */
    long long game_time; /*!< Game time is the internal time controlled either by the autosplitter, or derived from real_time - load_time */
} ls_time;

/**
 * @brief enum used for selecting comparison methods. This should correspond to the ls_time struct
 * and have one enum value per ls_time member. The enum value is also stored in the splits file
 * as the game's comparison method for determining which time value is authoritative for PBs and the like.
 * Therefore, all enums should always explicitly declare their backed value to prevent drift
 * or split file incompatibilities.
 */
typedef enum ls_time_method {
    LS_REAL_TIME = 0, /*!< LS_REAL_TIME corresponds to ls_time.real_time */
    LS_GAME_TIME = 1, /*!< LS_GAME_TIME corresponds to ls_time.game_time */
} ls_time_method;
