#include <stdint.h>

typedef struct {
    unsigned status : 4;
    unsigned channel : 4;
    uint8_t  param1;
    uint8_t  param2;
    int64_t  delta_time;
} NoteEvent;

typedef struct {
    NoteEvent* elements;
    size_t size;
    size_t capacity;
} NoteEvents;

typedef struct {
    NoteEvents events;
    int channel;
} ChannelEventArray;

void init_note_event_array(NoteEvents* arr, size_t initialCapacity);
void append_note_event(NoteEvents* arr, NoteEvent event);
int create_event_arr(const char* filename, NoteEvents* events);
void create_channel_arrays(NoteEvents* events, ChannelEventArray* channelArrays, int* numChannelArrays);