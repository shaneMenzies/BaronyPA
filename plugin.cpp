#include "MumblePlugin_v_1_0_x.h"

#if defined(_WIN32)
    #pragma message("Building for Windows")
    #include <windows.h>
    #include <Psapi.h>
    #include <memoryapi.h>
    #include <errhandlingapi.h>
#elif defined(__linux__)
    #pragma message("Building for Linux")
    #include <sys/uio.h>
    #include <signal.h>
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

// #define DEBUG 1

#define PLUGIN_NAME    "BaronyPA"
#define PLUGIN_VERSION "0.1.4"

#define BARONY_LEGACY_VERSION           "v3.3.7"
#define BARONY_LEGACY_DEFAULT_BASE      0x400000
#define BARONY_LEGACY_VERSION_OFFSET    0x2a6948
#define BARONY_LEGACY_CLIENT_NUM_OFFSET 0
#define BARONY_LEGACY_PLAYERS_OFFSET    0

#define BARONY_STABLE_VERSION           "v4.2.1"
#define BARONY_STABLE_DEFAULT_BASE      0x140000000
#define BARONY_STABLE_VERSION_OFFSET    0x7f2ca0
#define BARONY_STABLE_CLIENT_NUM_OFFSET 0x8ee76c
#define BARONY_STABLE_PLAYERS_OFFSET    0x930a98

enum offset_index {
    OFFSET_VERSION,
    OFFSET_CLIENT_NUM,
    OFFSET_PLAYERS,
};
size_t                   offsets[] = {BARONY_STABLE_VERSION_OFFSET,
                                      BARONY_STABLE_CLIENT_NUM_OFFSET,
                                      BARONY_STABLE_PLAYERS_OFFSET};
struct MumbleAPI_v_1_0_x mumbleAPI;
mumble_plugin_id_t       ownID;

uintptr_t baronyBaseAddr;
uint64_t  baronyPID;
bool      legacy = false;
bool      active = false;
struct view_t {
    double z;
    double x;
    double y;
    double horiz_angle;
    double vert_angle;
};

struct playerHeader {
    bool    local_host;
    char    fill[7];
    view_t* camera;
    int     player_num;
};

#if defined(_WIN32)

HANDLE baronyWinHandle;

#elif defined(__linux__)

#else
    #error "Couldn't detect target OS!"
#endif

// Helper functions

bool readFromBarony(uintptr_t baronyAddr, void* target, size_t size) {

// Method differs between Windows and Linux
#if defined(_WIN32)

    // Check to make sure Barony still exists
    DWORD baronyStatus;
    if ((GetExitCodeProcess(baronyWinHandle, &baronyStatus) == 0) ||
        (baronyStatus != STILL_ACTIVE)) {
        mumbleAPI.log(ownID, "Lost connection to Barony. Stopping.");
        return false;
    }

    // Access Barony's memory
    size_t bytesRead = 0;
    if (ReadProcessMemory(baronyWinHandle, (void*)baronyAddr, target, size,
                          &bytesRead) == 0) {
        // An error occurred
        DWORD error_code = GetLastError();

        char buffer[256];
        sprintf(buffer,
                "An error occurred in ReadProcessMemory, Error Code: %u, "
                "targeted address: 0x%p",
                error_code, (void*)baronyAddr);
        mumbleAPI.log(ownID, buffer);

        return false;
    }
        if (bytesRead < size) {
        // An error occurred
        char buffer[256];
        sprintf(buffer, "ReadProcessMemory only read %u bytes!",
                (unsigned int)bytesRead);
        mumbleAPI.log(ownID, buffer);

        return false;
        }

#elif defined(__linux__)

    // Check to make sure barony still exists
    if (kill(baronyPID, 0) == -1) {
        mumbleAPI.log(ownID, "Lost connection to Barony. Stopping.");
        return false;
    }

    // Access Barony's memory
    struct iovec local_iov  = {target, size};
    struct iovec remote_iov = {(void*)baronyAddr, size};

    ssize_t bytesRead =
        process_vm_readv(baronyPID, &local_iov, 1, &remote_iov, 1, 0);
    if (bytesRead < size) {
        // An error occurred
        mumbleAPI.log(ownID, "An error occurred in process_vm_readv().");
        return false;
    }
#endif

    // No errors occurred
    return true;
}

// Grabs the Positional data of a player
bool getBaronyPlayerPos(struct view_t* target, int player) {
        bool successful = true;

        // Need to index into players
        uintptr_t targetPlayerAddr;
        successful = readFromBarony(baronyBaseAddr + offsets[OFFSET_PLAYERS] +
                                        (8 * player),
                                    &targetPlayerAddr, sizeof(void*));

        // Grab player info
        playerHeader playerInfo;
        if (successful)
            successful = readFromBarony(targetPlayerAddr, &playerInfo,
                                        sizeof(playerHeader));

        // Grab camera value from player
        if (successful)
            successful = readFromBarony((uintptr_t)playerInfo.camera, target,
                                        sizeof(view_t));

        return successful;
}

// Grabs the client number
bool getBaronyClientNum(int* target) {
        return readFromBarony(baronyBaseAddr + offsets[OFFSET_CLIENT_NUM],
                              target, sizeof(int));
}

mumble_error_t mumble_init(mumble_plugin_id_t pluginID) {
        ownID  = pluginID;
        active = false;

        if (mumbleAPI.log(ownID, "BaronyPA Waiting") != MUMBLE_STATUS_OK) {
            // Logging failed -> usually you'd probably want to log things like
            // this in your plugin's logging system (if there is any)
        }

        return MUMBLE_STATUS_OK;
}

void mumble_shutdown() {
        active = false;
        if (mumbleAPI.log(ownID, "BaronyPA Stopping") != MUMBLE_STATUS_OK) {
            // Logging failed -> usually you'd probably want to log things like
            // this in your plugin's logging system (if there is any)
        }
}

// Positional Audio

uint8_t mumble_initPositionalData(const char* const* programNames,
                                  const uint64_t*    programPIDS,
                                  size_t             programCount) {
        // Check if Barony is open

        for (size_t i = 0; i < programCount; i++) {
            if (strcmp(programNames[i], "barony.exe") == 0) {
                baronyPID = programPIDS[i];
                mumbleAPI.log(ownID, "Found Barony:");
                mumbleAPI.log(ownID, programNames[i]);

#if defined(_WIN32)
                // On Windows we need to open the Barony process
                baronyWinHandle =
                    OpenProcess(PROCESS_VM_READ | PROCESS_QUERY_INFORMATION,
                                false, baronyPID);

                // Also need to determine base address
                HMODULE moduleBuffer[1024];
                DWORD   bytesNeeded = 0;
                EnumProcessModules(baronyWinHandle, moduleBuffer,
                                   sizeof(moduleBuffer), &bytesNeeded);
                baronyBaseAddr = (uint64_t)moduleBuffer[0];

                // Check the version
                size_t bytesRead = 0;
                char   buffer[64];
                void*  baronyAddr =
                    (void*)(baronyBaseAddr + BARONY_LEGACY_VERSION_OFFSET);
                bool failed =
                    (ReadProcessMemory(baronyWinHandle, baronyAddr, buffer,
                                       sizeof(BARONY_LEGACY_VERSION),
                                       &bytesRead) == 0);
                failed = failed || (bytesRead < sizeof(BARONY_LEGACY_VERSION));

                // Stable version check
                if (failed || (strcmp(buffer, BARONY_LEGACY_VERSION) != 0)) {
                    // Not legacy, assume stable
                    legacy = false;
                } else {
                    legacy = true;
                }
#elif defined(__linux__)
            // Linux doesn't need to open first
            baronyBaseAddr = BARONY_LEGACY_DEFAULT_BASE;

            // Check the version
            ssize_t bytesRead = 0;
            char    buffer[64];
            void*   baronyAddr =
                (void*)(baronyBaseAddr + BARONY_LEGACY_VERSION_OFFSET);
            struct iovec local_iov  = {buffer, sizeof(BARONY_LEGACY_VERSION)};
            struct iovec remote_iov = {baronyAddr,
                                       sizeof(BARONY_LEGACY_VERSION)};

            bytesRead =
                process_vm_readv(baronyPID, &local_iov, 1, &remote_iov, 1, 0);
            bool failed = (bytesRead < sizeof(BARONY_LEGACY_VERSION));

            // Legacy version check
            if (failed || (strcmp(buffer, BARONY_LEGACY_VERSION) != 0)) {
                // Not legacy, assume stable
                legacy         = false;
                baronyBaseAddr = BARONY_STABLE_DEFAULT_BASE;
            } else {
                legacy = true;
            }
#endif

                if (legacy) {
                    mumbleAPI.log(ownID, "Detected as Legacy Version");

                    // Swap offsets
                    offsets[OFFSET_VERSION] = BARONY_LEGACY_VERSION_OFFSET;
                    offsets[OFFSET_CLIENT_NUM] =
                        BARONY_LEGACY_CLIENT_NUM_OFFSET;
                    offsets[OFFSET_PLAYERS] = BARONY_LEGACY_PLAYERS_OFFSET;
                }

                // Mumble will now start calling mumble_fetchPositionalData
                active = true;
                return MUMBLE_PDEC_OK;
            }
        }

        // Barony isn't open
        return MUMBLE_PDEC_ERROR_TEMP;
}

bool mumble_fetchPositionalData(float* avatarPos, float* avatarDir,
                                float* avatarAxis, float* cameraPos,
                                float* cameraDir, float* cameraAxis,
                                const char** context, const char** identity) {

        // Make sure we've acquired a pid for Barony first
        if (!active) {
            return false;
        }

        // Get Position data from Barony
        static struct view_t value;
        static int           clientNum;

        if (!getBaronyClientNum(&clientNum) ||
            !getBaronyPlayerPos(&value, clientNum)) {
            // Unable to fetch data
            active = false;
            return false;
        }

        // Position
        cameraPos[0] = avatarPos[0] = value.x;
        cameraPos[1] = avatarPos[1] = value.y;
        cameraPos[2] = avatarPos[2] = value.z;

        // Need to get direction from Barony's angles (also needs to be a unit
        // vector)
        double dirY      = -sin(value.vert_angle);
        float  magnitude = sqrt(1 + (dirY * dirY));

        cameraDir[0] = avatarDir[0] = sin(value.horiz_angle) / magnitude;
        cameraDir[1] = avatarDir[1] = dirY / magnitude;
        cameraDir[2] = avatarDir[2] = cos(value.horiz_angle) / magnitude;

        // Axis - Character seems to be about 0.6 meters tall
        avatarAxis[0] = cameraAxis[0] = 0;
        avatarAxis[1] = cameraAxis[1] = 0.6;
        avatarAxis[2] = cameraAxis[2] = 0;

        // Context - currently constant
        const static char contextString[] = "BaronyPA";
        *context                    = contextString;

#ifdef DEBUG
        /*
         * Mumble doesn't seem to have a positional audio viewer on Windows,
         * but this works instead. It just prints the values in to the console.
         */
        static int interval = 0;
        interval++;
        if (interval >= 16) {
            char buffer[256];
            sprintf(buffer, "Player #%i - Pos: (%.1f, %.1f, %.1f) Dir: (%.1f, %.1f, %.1f)",
                clientNum, avatarPos[0], avatarPos[1], avatarPos[2], avatarDir[0], 
                avatarDir[1], avatarDir[2]); mumbleAPI.log(ownID, buffer);
            interval = 0;
        }
#endif

        return true;
}

void mumble_shutdownPositionalData() {
        // Disconnect from Barony
        active = false;

// On Windows we need to close the process handle
#ifdef _WIN32
        CloseHandle(baronyWinHandle);
#endif
}

// Plugin details

struct MumbleStringWrapper mumble_getName() {
        static const char* name = PLUGIN_NAME;

        struct MumbleStringWrapper wrapper;
        wrapper.data           = name;
        wrapper.size           = strlen(name);
        wrapper.needsReleasing = false;

        return wrapper;
}

mumble_version_t mumble_getVersion() {
        static const mumble_version_t version = {0, 1, 2};
        return version;
}

struct MumbleStringWrapper mumble_getAuthor() {
        static const char* author = "Shane Menzies";

        struct MumbleStringWrapper wrapper;
        wrapper.data           = author;
        wrapper.size           = strlen(author);
        wrapper.needsReleasing = false;

        return wrapper;
}

struct MumbleStringWrapper mumble_getDescription() {
        static const char* description = "Simple positional audio for Barony.";

        struct MumbleStringWrapper wrapper;
        wrapper.data           = description;
        wrapper.size           = strlen(description);
        wrapper.needsReleasing = false;

        return wrapper;
}

// Unmodified functions

uint32_t mumble_getFeatures() {
        return MUMBLE_FEATURE_POSITIONAL;
}

mumble_version_t mumble_getAPIVersion() {
        // This constant will always hold the API version  that fits the
        // included header files
        return MUMBLE_PLUGIN_API_VERSION;
}

void mumble_registerAPIFunctions(void* apiStruct) {
        // Provided mumble_getAPIVersion returns MUMBLE_PLUGIN_API_VERSION, this
        // cast will make sure that the passed pointer will be cast to the
        // proper type
        mumbleAPI = MUMBLE_API_CAST(apiStruct);
}

void mumble_releaseResource(const void* pointer) {
        // As we never pass a resource to Mumble that needs releasing, this
        // function should never get called
        printf(
            "Called mumble_releaseResource but expected that this never gets "
            "called -> Aborting");
        abort();
}
