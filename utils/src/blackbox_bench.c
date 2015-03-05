#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <time.h>
#include <unistd.h>

#include <getopt.h>

#ifdef __MACH__
#include <mach/clock.h>
#include <mach/mach.h>
#endif

#include "../src/serial.h"

#define P_FRAME_SIZE 22
#define I_FRAME_SIZE 50
#define I_FRAME_INTERVAL 32
#define I_FRAME_FILL_BYTE 0xA0
#define P_FRAME_FILL_BYTE 0xC7

#define BENCHMARK_HEADER_INTRO "Blackbox benchmark\n"

typedef struct benchOptions_t {
    int help;
    int duration;
    int baudRate;
    int looptime;
    const char *analyzeFilename;
    const char *outputDevice;
} benchOptions_t;

benchOptions_t defaultOptions = {
    .baudRate = 115200,
    .looptime = 2500,
    .duration = 15,
    .help = 0,
    .analyzeFilename = NULL, .outputDevice = NULL,
};

benchOptions_t options;

uint32_t micros() {
    struct timespec ts;

#ifdef __MACH__ // OS X does not have clock_gettime, use clock_get_time
    clock_serv_t cclock;
    mach_timespec_t mts;

    host_get_clock_service(mach_host_self(), CALENDAR_CLOCK, &cclock);
    clock_get_time(cclock, &mts);
    mach_port_deallocate(mach_task_self(), cclock);

    ts.tv_sec = mts.tv_sec;
    ts.tv_nsec = mts.tv_nsec;
#else
    clock_gettime(CLOCK_REALTIME, &ts);
#endif

    return ts.tv_sec * 1000000 + ts.tv_nsec / 1000;
}

/**
 * Read count bytes from the fd into the given buffer.
 */
bool readAll(int fd, char *buf, int count)
{
    while (count > 0) {
        ssize_t bytesRead = read(fd, buf, count);

        if (bytesRead <= 0)
            return false;

        buf += bytesRead;
        count -= bytesRead;
    }

    return true;
}

/**
 * Write count bytes from buffer to the fd.
 */
bool writeAll(int fd, uint8_t *buf, int count)
{
    while (count > 0) {
        ssize_t bytesWritten = write(fd, (char*) buf, count);

        if (bytesWritten < 0)
            return false;

        buf += bytesWritten;
        count -= bytesWritten;
    }

    return true;
}

void print(int fd, const char *s)
{
    writeAll(fd, (uint8_t*) s, strlen(s));
}

void writeBenchmarkFrames(int fd, int loopTime, int maxIterations, uint32_t *byteCount, uint32_t *timingErrorUs, uint32_t *actualDurationMsec)
{
    uint32_t firstLoop, lastLoop, thisLoop;
    uint32_t loopIteration;
    int64_t timingErrorSum;

    uint8_t iframe[I_FRAME_SIZE];
    uint8_t pframe[P_FRAME_SIZE];

    // Fill the frames with some blank spacer data:
    memset(iframe, I_FRAME_FILL_BYTE, sizeof(iframe));
    memset(pframe, P_FRAME_FILL_BYTE, sizeof(pframe));

    pframe[0] = 'P';
    iframe[0] = 'I';

    // Start things off with a nice tidy "previous loop" timestamp
    firstLoop = micros();
    lastLoop = micros() - loopTime;

    *byteCount = 0;
    timingErrorSum = 0;

    for (loopIteration = 0; loopIteration < maxIterations; loopIteration++) {
        //Burn cycles waiting for the start of frame
        do {
            thisLoop = micros();
        } while (lastLoop + loopTime > thisLoop);

        // Did we meet our scheduled timestamp for this iteration?
        timingErrorSum += thisLoop - lastLoop - loopTime;

        if (loopIteration % I_FRAME_INTERVAL == 0) {
            // Mark iteration counts so we know which loop is which when we read the log
            iframe[1] = loopIteration & 0xFF;
            iframe[2] = (loopIteration >> 8) & 0xFF;
            iframe[3] = (loopIteration >> 16) & 0xFF;
            iframe[4] = (loopIteration >> 24) & 0xFF;
            writeAll(fd, iframe, I_FRAME_SIZE);

            *byteCount += I_FRAME_SIZE;
        } else {
            pframe[1] = loopIteration & 0xFF;
            pframe[2] = (loopIteration >> 8) & 0xFF;
            pframe[3] = (loopIteration >> 16) & 0xFF;
            pframe[4] = (loopIteration >> 24) & 0xFF;
            writeAll(fd, pframe, P_FRAME_SIZE);

            *byteCount += P_FRAME_SIZE;
        }

        lastLoop = thisLoop;
    }

    *timingErrorUs = (uint32_t) (timingErrorSum / maxIterations);
    *actualDurationMsec = (micros() - firstLoop) / 1000;
}

bool runBenchmark(const char *deviceName)
{
    // Choose iteration count to achieve the required number of seconds of flight
    int maxIterations = (1000000 * options.duration) / options.looptime;
    char lineBuffer[256];
    int fd;
    uint32_t byteCount, timingErrorUs, actualDurationMsec;

    fprintf(stderr, "Opening %s at %d baud...\n", deviceName, options.baudRate);
    fd = serial_open(deviceName, options.baudRate);

    if (fd == -1) {
        fprintf(stderr, "Failed to open serial port, maybe try a different baud rate?\n");
        return false;
    }

    fprintf(stderr, "Waiting for OpenLog to be ready...\n");

    char ready[4];

    readAll(fd, ready, 3);
    ready[3] = '\0';

    if (strcmp(ready, "12<") != 0) {
        fprintf(stderr, "Unexpected response from Openlog \"%.*s\"\n", 3, ready);
        return false;
    }

    fprintf(stderr, "\nRunning %d second benchmark at looptime %d us...\n", options.duration, options.looptime);

    print(fd, BENCHMARK_HEADER_INTRO);

    snprintf(lineBuffer, sizeof(lineBuffer), "I interval:%d\n", I_FRAME_INTERVAL);
    print(fd, lineBuffer);

    snprintf(lineBuffer, sizeof(lineBuffer), "I size:%d\n", I_FRAME_SIZE);
    print(fd, lineBuffer);

    snprintf(lineBuffer, sizeof(lineBuffer), "P size:%d\n", P_FRAME_SIZE);
    print(fd, lineBuffer);

    snprintf(lineBuffer, sizeof(lineBuffer), "Looptime:%d\n", options.looptime);
    print(fd, lineBuffer);

    snprintf(lineBuffer, sizeof(lineBuffer), "Serial baud:%d\n", options.baudRate);
    print(fd, lineBuffer);

    snprintf(lineBuffer, sizeof(lineBuffer), "Iterations:%d\n", maxIterations);
    print(fd, lineBuffer);

    print(fd, "\n");

    // Give the header time to flush (I'd like to sleep for just 500ms but sleep takes seconds as an argument :/)
    sleep(1);

    writeBenchmarkFrames(fd, options.looptime, maxIterations, &byteCount, &timingErrorUs, &actualDurationMsec);

    fprintf(stderr, "\nWrote %u bytes (%u bytes/s, %u baud) with average frame start time error %d us\n\n", byteCount,
            (byteCount * 1000) / actualDurationMsec, (byteCount * 8 * 1000) / actualDurationMsec, timingErrorUs);

    // Wait for card to flush
    fprintf(stderr, "Waiting for OpenLog to finish...\n");
    sleep(6);

    close(fd);

    return true;
}

void analyzeLog(FILE *input)
{
    char lineBuffer[256];
    bool sawHeaderIntro = false;

    int iInterval, iSize, pSize;
    int looptime, baudrate, maxIterations = 0;
    int frameByteCount;
    char currentFrameType;
    int goodFrames, brokenFrames;

    int32_t lastIteration;
    int32_t iterationNumBuffer;

    int c;

    // Read header
    while ((fgets(lineBuffer, sizeof(lineBuffer), input))) {
        if (strcmp(lineBuffer, "\n") == 0) {
            // Blank line signals end of header
            break;
        }

        if (!sawHeaderIntro) {
            if (strcmp(lineBuffer, BENCHMARK_HEADER_INTRO) == 0) {
                sawHeaderIntro = true;
                fprintf(stderr, "Benchmark header fields:\n");
            }
        } else {
            char *colon = strchr(lineBuffer, ':');

            if (colon) {
                *colon = '\0';

                char *headerName = lineBuffer;
                char *headerValue = colon + 1;

                fprintf(stderr, "  %s: %s", headerName, headerValue);

                if (strcmp(headerName, "I interval") == 0) {
                    iInterval = atoi(headerValue);
                } else if (strcmp(headerName, "I size") == 0) {
                    iSize = atoi(headerValue);
                } else if (strcmp(headerName, "P size") == 0) {
                    pSize = atoi(headerValue);
                } else if (strcmp(headerName, "Looptime") == 0) {
                    looptime = atoi(headerValue);
                } else if (strcmp(headerName, "Serial baud") == 0) {
                    baudrate = atoi(headerValue);
                } else if (strcmp(headerName, "Iterations") == 0) {
                    maxIterations = atoi(headerValue);
                }
            }
        }
    }

    fprintf(stderr, "\n");

    goodFrames = brokenFrames = 0;

    lastIteration = -1;
    frameByteCount = 0;
    currentFrameType = '\0';

    while (true) {
        c = fgetc(input);

        switch (currentFrameType) {
            case '\0':
                switch (c) {
                    case 'I':
                    case 'P':
                        currentFrameType = c;
                        frameByteCount = 1;
                    break;
                    case EOF:
                    // done
                    goto done;
                }
            break;
            case 'I':
            case 'P':
                if (c == EOF) {
                    // Broken frame at end of log
                    goto done;
                }

                // Read the loop iteration from the front of the frame
                if (frameByteCount == 1) {
                    iterationNumBuffer = c;
                } else if (frameByteCount == 2) {
                    iterationNumBuffer |= c << 8;
                } else if (frameByteCount == 3) {
                    iterationNumBuffer |= c << 16;
                } else if (frameByteCount == 4) {
                    iterationNumBuffer |= c << 24;

                    // Does this look like a reasonable iteration number?
                    if (iterationNumBuffer > lastIteration + 200 || iterationNumBuffer < lastIteration) {
                        lastIteration = iterationNumBuffer;
                    }
                } else if ((currentFrameType == 'I' && c != I_FRAME_FILL_BYTE) ||
                        (currentFrameType == 'P' && c != P_FRAME_FILL_BYTE)) {
                    /*
                     * Frame was probably truncated since the padding isn't correct.
                     *
                     * Resynchronize the stream.
                     */
                    currentFrameType = '\0';
                    ungetc(c, input);
                    continue;
                }

                frameByteCount++;
                if ((currentFrameType == 'I' && frameByteCount >= iSize) ||
                    (currentFrameType == 'P' && frameByteCount >= pSize)) {
                    goodFrames++;
                    currentFrameType = '\0';
                }
            break;
        }
    }

    done:
    fprintf(stderr, "Good frames %d, broken/missing iterations %d, total iterations %d\n", goodFrames, maxIterations - goodFrames, maxIterations);
}

void printUsage(const char *argv0)
{
    fprintf(stderr,
        "Blackbox OpenLog benchmark tool by Nicholas Sherlock ("
            __DATE__ " " __TIME__ ")\n\n"
        "Usage:\n"
        "     %s [options]\n\n"
        "Options:\n"
        "   --help                 This page\n"
        "   --baud <num>           Serial port baud rate (default %d)\n"
        "   --looptime <microsec>  Simulated looptime (default %d us)\n"
        "   --duration <seconds>   Simulation duration (default %d seconds)\n"
        "   --device <filename>    Serial port to write to\n"
        "   --analyze <filename>   OpenLog benchmark log to analyze\n"
        "\n", argv0, defaultOptions.baudRate, defaultOptions.looptime, defaultOptions.duration
    );
}

static void parseCommandlineOptions(int argc, char **argv)
{
    int c;

    enum {
        SETTING_ANALYZE = 1,
        SETTING_DEVICE,
        SETTING_BAUDRATE,
        SETTING_LOOPTIME,
        SETTING_DURATION,
    };

    while (1)
    {
        static struct option long_options[] = {
            {"help", no_argument, &options.help, 1},
            {"analyze", required_argument, 0, SETTING_ANALYZE},
            {"device", required_argument, 0, SETTING_DEVICE},
            {"baud", required_argument, 0, SETTING_BAUDRATE},
            {"looptime", required_argument, 0, SETTING_LOOPTIME},
            {"duration", required_argument, 0, SETTING_DURATION},
            {0, 0, 0, 0}
        };

        int option_index = 0;

        opterr = 0;

        c = getopt_long(argc, argv, ":", long_options, &option_index);

        if (c == -1)
            break;

        switch (c) {
            case SETTING_ANALYZE:
                options.analyzeFilename = optarg;
            break;
            case SETTING_DEVICE:
                options.outputDevice = optarg;
            break;
            case SETTING_BAUDRATE:
                options.baudRate = atoi(optarg);
            break;
            case SETTING_DURATION:
                options.duration = atoi(optarg);
            break;
            case SETTING_LOOPTIME:
                options.looptime = atoi(optarg);

                if (options.looptime <= 0) {
                    fprintf(stderr, "Looptime must be greater than zero\n");
                    exit(EXIT_FAILURE);
                }
            break;
            case '\0':
                //Longopt which has set a flag
            break;
            case ':':
                fprintf(stderr, "%s: option '%s' requires an argument\n", argv[0], argv[optind - 1]);
                exit(-1);
            break;
            default:
                if (optopt == 0)
                    fprintf(stderr, "%s: option '%s' is invalid\n", argv[0], argv[optind - 1]);
                else
                    fprintf(stderr, "%s: option '-%c' is invalid\n", argv[0], optopt);

                exit(-1);
            break;
        }
    }
}

int main(int argc, char **argv)
{
    options = defaultOptions;

    parseCommandlineOptions(argc, argv);

    if (options.help) {
        printUsage(argv[0]);
        return EXIT_FAILURE;
    }

    if (options.analyzeFilename) {
        FILE *input = fopen(options.analyzeFilename, "rb");

        if (input) {
            analyzeLog(input);
        } else {
            fprintf(stderr, "Couldn't open log file '%s'\n", options.analyzeFilename);
            return EXIT_FAILURE;
        }
    } else if (options.outputDevice) {
        if (runBenchmark(options.outputDevice)) {
            fprintf(stderr, "\nBenchmark done! Now run with --analyze <filename> with the log file from the SD card.\n");
        }
    } else {
        printUsage(argv[0]);
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}
