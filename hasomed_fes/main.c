#include <windows.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <tchar.h>
#include <stdbool.h>
#include <time.h>
#include <sys/time.h>
#include <string.h>
#include <direct.h>

#include <src/smpt_ll_client.h>

// Correctly escaped pipe name: \\.\pipe\UnityToCPipe
#define PIPE_NAME L"\\\\.\\pipe\\UnityToCPipe"
#define BUFFER_SIZE 1024
#define MAX_FILENAME_LENGTH 200

char PORT[32] = "COM4";
int STIMULATION_INTERVAL_MS = 50; // 20Hz

double getCurrentUnixTimestamp(void) {
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return tv.tv_sec + (tv.tv_usec / 1000000.0);
}

void getTimestampedFilename(char* filename, size_t max_length) {
    time_t rawtime;
    struct tm timeinfo;
    char buffer[80];
    time(&rawtime);
    localtime_s(&timeinfo, &rawtime);
    strftime(buffer, sizeof(buffer), "%Y%m%d_%H%M%S", &timeinfo);
    _mkdir("csv");
    snprintf(filename, max_length, "csv\\received_values_%s.csv", buffer);
}

void logToCSV(FILE *logFile, double timestamp, uint16_t us, float ma, float kg, float min, float max, float target) {
    if (logFile) {
        fprintf(logFile, "%.3f;%hu;%.2f;%.2f;%.2f;%.2f;%.2f\n", timestamp, us, ma, kg, min, max, target);
        fflush(logFile);
    }
}

int main(int argc, char *argv[]) {
    // Usage: FESController.exe [COM_PORT] [INTERVAL_MS]
    if (argc > 1) {
        snprintf(PORT, sizeof(PORT), "%s", argv[1]);
    }
    if (argc > 2) {
        STIMULATION_INTERVAL_MS = atoi(argv[2]);
    }
    printf("Configuration: Port=%s, Interval=%d ms\n", PORT, STIMULATION_INTERVAL_MS);

    Smpt_device smpt_device;
    Smpt_ll_init smpt_ll_init;
    Smpt_ll_channel_config smpt_ll_channel_config;
    Smpt_ll_point smpt_ll_point;

    // Use a Handle for the pipe
    HANDLE hPipe;

    smpt_ll_channel_config.enable_stimulation = true;
    smpt_ll_channel_config.channel = Smpt_Channel_Red;
    smpt_ll_channel_config.connector = Smpt_Connector_Yellow;
    smpt_ll_channel_config.number_of_points = 1;

    printf("Opening Serial Port: %s\n", PORT);
    if (!smpt_open_serial_port(&smpt_device, PORT)) {
        printf("Failed to open port %s\n", PORT);
        return 1;
    }
    smpt_send_ll_init(&smpt_device, &smpt_ll_init);

    char filename[MAX_FILENAME_LENGTH];
    getTimestampedFilename(filename, MAX_FILENAME_LENGTH);
    FILE *logFile = fopen(filename, "w");
    if (logFile) fprintf(logFile, "UnixTimestamp;Microseconds;MilliAmpere;CurrentKG;TargetMin;TargetMax;TargetKG\n");

    // Create the named pipe
    // Note: PIPE_NAME must be L"\\.\\pipe\..."
    hPipe = CreateNamedPipeW(
        PIPE_NAME,
        PIPE_ACCESS_INBOUND,
        PIPE_TYPE_BYTE | PIPE_READMODE_BYTE | PIPE_WAIT,
        1, // Max instances
        BUFFER_SIZE * sizeof(WCHAR), // Out buffer
        BUFFER_SIZE * sizeof(WCHAR), // In buffer
        0, // Default timeout
        NULL // Security attributes
    );

    if (hPipe == INVALID_HANDLE_VALUE) {
        printf("CreateNamedPipe failed with error: %lu\n", GetLastError());
        // Clean up
        if (logFile) fclose(logFile);
        smpt_close_serial_port(&smpt_device);
        return 1;
    }

    printf("Waiting for client connection...\n");
    // Blocking connect (since PIPE_WAIT is set, but ConnectNamedPipe behaves differently depending on overlap)
    // Synchronous connect:
    BOOL fConnected = ConnectNamedPipe(hPipe, NULL) ? TRUE : (GetLastError() == ERROR_PIPE_CONNECTED);

    if (fConnected) {
        printf("Client connected. Logging to %s\n", filename);
        
        uint16_t cur_us = 0;
        float cur_ma = 0.0f, cur_kg = 0.0f, t_min = 0.0f, t_max = 0.0f, t_target = 0.0f;
        char buf[BUFFER_SIZE];

        while (1) {
            DWORD avail = 0;
            // Non-blocking check
            if (PeekNamedPipe(hPipe, NULL, 0, NULL, &avail, NULL) && avail > 0) {
                DWORD read = 0;
                if (ReadFile(hPipe, buf, BUFFER_SIZE - 1, &read, NULL)) {
                    buf[read] = '\0';
                    int count = sscanf(buf, "%hu,%f,%f,%f,%f,%f", &cur_us, &cur_ma, &cur_kg, &t_min, &t_max, &t_target);
                    if (count >= 2) {
                        logToCSV(logFile, getCurrentUnixTimestamp(), cur_us, cur_ma, cur_kg, t_min, t_max, t_target);
                        printf("UPDATE: %.2f mA | %.2f KG\n", cur_ma, cur_kg);
                    }
                } else {
                    // Read failed (pipe broken?)
                    break;
                }
            }

            // Keep-alive stimulation
            if (cur_us > 0 && cur_ma >= 1.0f) {
                smpt_ll_point.time = cur_us;
                smpt_ll_point.current = cur_ma;
                smpt_ll_channel_config.points[0] = smpt_ll_point;
                smpt_send_ll_channel_config(&smpt_device, &smpt_ll_channel_config);
            }
            Sleep(STIMULATION_INTERVAL_MS);
        }
    } else {
        printf("ConnectNamedPipe failed with error: %lu\n", GetLastError());
    }

    if (logFile) fclose(logFile);
    CloseHandle(hPipe);
    smpt_close_serial_port(&smpt_device);
    return 0;
}
