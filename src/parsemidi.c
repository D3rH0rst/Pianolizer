#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <stdio.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/mman.h>
#include <stdlib.h>

#include "midi-parser.h"
#include "parsemidi.h"

void init_note_event_array(NoteEvents* arr, size_t initialCapacity) {
    arr->elements = malloc(initialCapacity * sizeof(NoteEvent));
    arr->size = 0;
    arr->capacity = initialCapacity;
}

void append_note_event(NoteEvents* arr, NoteEvent event) {
    if (arr->size == arr->capacity) {
        arr->capacity *= 2;
        arr->elements = realloc(arr->elements, arr->capacity * sizeof(NoteEvent));
    }
    arr->elements[arr->size++] = event;
}

static void parse_and_dump(struct midi_parser *parser, NoteEvents* events)
{
    enum midi_parser_status status;

    while (1) {
        NoteEvent event = {0};
        status = midi_parse(parser);
        switch (status) {
            case MIDI_PARSER_EOB:
                puts("eob");
                return;

            case MIDI_PARSER_ERROR:
                puts("error");
                return;

            case MIDI_PARSER_INIT:
                puts("init");
                break;

            case MIDI_PARSER_HEADER:
                /*
                printf("header\n");
                printf("  size: %d\n", parser->header.size);
                printf("  format: %d [%s]\n", parser->header.format, midi_file_format_name(parser->header.format));
                printf("  tracks count: %d\n", parser->header.tracks_count);
                printf("  time division: %d\n", parser->header.time_division);
                */
                break;

            case MIDI_PARSER_TRACK:
                // puts("track");
                // printf("  length: %d\n", parser->track.size);
                break;

            case MIDI_PARSER_TRACK_MIDI:
                /*puts("track-midi");
                printf("  time: %ld\n", parser->vtime);
                printf("  status: %d [%s]\n", parser->midi.status, midi_status_name(parser->midi.status));
                printf("  channel: %d\n", parser->midi.channel);
                printf("  param1: %d\n", parser->midi.param1);
                printf("  param2: %d\n", parser->midi.param2);
                */
                event.status = parser->midi.status;
                event.channel = parser->midi.channel;
                event.delta_time = parser->vtime;
                event.param1 = parser->midi.param1;
                event.param2 = parser->midi.param2;
                append_note_event(events, event);
                break;

            case MIDI_PARSER_TRACK_META:
                /*
                printf("track-meta\n");
                printf("  time: %ld\n", parser->vtime);
                printf("  type: %d [%s]\n", parser->meta.type, midi_meta_name(parser->meta.type));
                printf("  length: %d\n", parser->meta.length);
                */
                break;

            case MIDI_PARSER_TRACK_SYSEX:
                // puts("track-sysex");
                // printf("  time: %ld\n", parser->vtime);
                break;

            default:
                printf("unhandled state: %d\n", status);
                return;
        }
    }
}

static int parse_file(const char *path, NoteEvents* events)
{
    struct stat st;

    if (stat(path, &st)) {
        printf("stat(%s): %m\n", path);
        return 1;
    }

    int fd = open(path, O_RDONLY);
    if (fd < 0) {
        printf("open(%s): %m\n", path);
        return 1;
    }
    void *mem = mmap(NULL, st.st_size, PROT_READ, MAP_SHARED, fd, 0);
    if (mem == MAP_FAILED) {
        printf("mmap(%s): %m\n", path);
        close(fd);
        return 1;
    }
    struct midi_parser parser;
    parser.state = MIDI_PARSER_INIT;
    parser.size  = st.st_size;
    parser.in    = mem;

    parse_and_dump(&parser, events);
    munmap(mem, st.st_size);
    close(fd);
    return 0;
}

int create_event_arr(const char* filename, NoteEvents* events) {
    if (parse_file(filename, events) != 0) {
        fprintf(stderr, "Error while parsing file\n");
        return 1;
    }
    return 0;
}