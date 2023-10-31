#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <stdio.h>
#include <fcntl.h>
#include <errno.h>
#include <stdlib.h>

#ifdef _WIN32
#include <windows.h>
#include <memoryapi.h>

static void win_err_helper(const char* func, const char* path)
{
    const DWORD flags = FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM |
        FORMAT_MESSAGE_IGNORE_INSERTS;
    const DWORD dw = GetLastError();
    const DWORD lang = MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT);

    LPVOID lpMsgBuf;
    FormatMessage(flags, NULL, dw, lang, (LPTSTR)&lpMsgBuf, 0, NULL);

    printf("%s(%s): %s\n", func, path, (char*)lpMsgBuf);
    LocalFree(lpMsgBuf);
}

#else
#include <sys/mman.h>
#endif



#include "midi-parser.h"
#include "parsemidi.h"

#define NOTE_EVENT_CAP 100

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

void destroy_note_event_array(NoteEvents* arr) {
    free(arr->elements);
    arr->elements = NULL;
    arr->capacity = 0;
    arr->size = 0;
}

void destroy_channel_arrays(ChannelEventArray* arr) {
    destroy_note_event_array(&arr->events);
    arr->channel = -2;
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
                break;

            case MIDI_PARSER_TRACK:
                break;

            case MIDI_PARSER_TRACK_MIDI:
                if (parser->midi.status == 9 || parser->midi.status == 8) {
                    event.status = parser->midi.status;
                    event.channel = parser->midi.channel;
                    event.delta_time = parser->vtime;
                    event.param1 = parser->midi.param1;
                    event.param2 = parser->midi.param2;
                    append_note_event(events, event);
                }

                break;

            case MIDI_PARSER_TRACK_META:
                break;

            case MIDI_PARSER_TRACK_SYSEX:
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

#ifdef _WIN32

    HANDLE fhandle = (HANDLE)_get_osfhandle(fd);

    if (st.st_size == 0) {
        printf("file is empty\n");
        close(fd);
        return 1;
    }

    HANDLE hMapFile = CreateFileMapping(fhandle, NULL, PAGE_READONLY, 0, 0, NULL);

    if (!hMapFile) {
        win_err_helper("CreateFileMapping", path);
        close(fd);
        return 1;
    }

    void* mem = MapViewOfFile(hMapFile, FILE_MAP_READ, 0, 0, 0);

    if (!mem) {
        win_err_helper("MapViewOfFile", path);
        CloseHandle(hMapFile);
        close(fd);
        return 1;
    }

#else

    void *mem = mmap(NULL, st.st_size, PROT_READ, MAP_SHARED, fd, 0);
    if (mem == MAP_FAILED) {
        printf("mmap(%s): %m\n", path);
        close(fd);
        return 1;
    }

#endif

    struct midi_parser parser;
    parser.state = MIDI_PARSER_INIT;
    parser.size  = st.st_size;
    parser.in    = mem;

    parse_and_dump(&parser, events);

#ifdef _WIN32
    UnmapViewOfFile(mem);
    CloseHandle(hMapFile);
#else
    munmap(mem, st.st_size);
#endif

    return 0;
}

int create_event_arr(const char* filename, NoteEvents* events) {
    init_note_event_array(events, NOTE_EVENT_CAP);
    if (parse_file(filename, events) != 0) {
        fprintf(stderr, "Error while parsing file\n");
        return 1;
    }
    return 0;
}

void create_channel_arrays(NoteEvents* events, ChannelEventArray* channelArrays, int* numChannelArrays) {
    for (size_t i = 0; i < 16; i++) {
        channelArrays[i].channel = -2;
    }

    for (size_t i = 0; i < events->size; i++) {
        int channel = events->elements[i].channel;

        // Check if a subarray for this channel already exists
        int subArrayIndex = -1;
        for (int j = 0; j < *numChannelArrays; j++) {
            if (channelArrays[j].channel == channel) {
                subArrayIndex = j;
                break;
            }
        }

        if (subArrayIndex == -1) {
            // Create a new subarray for this channel
            channelArrays[*numChannelArrays].channel = channel;
            init_note_event_array(&channelArrays[*numChannelArrays].events, 100);
            subArrayIndex = (*numChannelArrays)++;
        }

        // Add the event to the subarray
        append_note_event(&channelArrays[subArrayIndex].events, events->elements[i]);
    }
}