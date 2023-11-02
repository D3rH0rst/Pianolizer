#include "fluid_getters.h"

struct _fluid_player_t
{
    int status;
    int stopping; /* Flag for sending all_notes_off when player is stopped */
    int ntracks;
    void* track[128];
    void* synth;
    void* system_timer;
    void* sample_timer;

    int loop; /* -1 = loop infinitely, otherwise times left to loop the playlist */
    void* playlist; /* List of fluid_playlist_item* objects */
    void* currentfile; /* points to an item in files, or NULL if not playing */

    char use_system_timer;   /* if zero, use sample timers, otherwise use system clock timer */
    char reset_synth_between_songs; /* 1 if system reset should be sent to the synth between songs. */
    int seek_ticks; /* new position in tempo ticks (midi ticks) for seeking */
    int start_ticks;          /* the number of tempo ticks passed at the last tempo change */
    int cur_ticks;            /* the number of tempo ticks passed */
    int last_callback_ticks;  /* the last tick number that was passed to player->tick_callback */
    int begin_msec;           /* the time (msec) of the beginning of the file */
    int start_msec;           /* the start time of the last tempo change */
    unsigned int cur_msec;    /* the current time */
    int end_msec;             /* when >=0, playback is extended until this time (for, e.g., reverb) */
    char end_pedals_disabled; /* 1 once the pedals have been released after the last midi event, 0 otherwise */
    /* sync mode: indicates the tempo mode the player is driven by (see fluid_player_set_tempo()):
       1, the player is driven by internal tempo (miditempo). This is the default.
       0, the player is driven by external tempo (exttempo)
    */
    int sync_mode;
    /* miditempo: internal tempo coming from MIDI file tempo change events
      (in micro seconds per quarter note)
    */
    int miditempo;     /* as indicated by MIDI SetTempo: n 24th of a usec per midi-clock. bravo! */
    /* exttempo: external tempo set by fluid_player_set_tempo() (in micro seconds per quarter note) */
    int exttempo;
    /* multempo: tempo multiplier set by fluid_player_set_tempo() */
    float multempo;
    float deltatime;   /* milliseconds per midi tick. depends on current tempo mode (see sync_mode) */
    unsigned int division;

    void* playback_callback; /* function fired on each midi event as it is played */
    void* playback_userdata; /* pointer to user-defined data passed to playback_callback function */
    void* tick_callback; /* function fired on each tick change */
    void* tick_userdata; /* pointer to user-defined data passed to tick_callback function */

    int channel_isplaying[16]; /* flags indicating channels on which notes have played */
};

