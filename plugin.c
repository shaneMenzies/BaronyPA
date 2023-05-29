#include "MumblePlugin_v_1_0_x.h"

#if defined(_WIN32)
#pragma message ("Building for Windows")
#include <windows.h>
#include <Psapi.h>
#include <memoryapi.h>
#include <errhandlingapi.h>
#elif defined(__linux__)
#pragma message ("Building for Linux")
#define __USE_GNU
#include <sys/uio.h>
#include <signal.h>
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#define PLUGIN_NAME "BaronyPA"
#define PLUGIN_VERSION "0.1.0"

#define POS_OFFSET 0x2e4fd8

struct MumbleAPI_v_1_0_x mumbleAPI;
mumble_plugin_id_t ownID;

uint64_t baronyBaseAddr;
uint64_t baronyPID;
bool active = false;
struct baronyPosValue {
    double z;
    double x;
    double y;
    double horiz_angle;
    double vert_angle;
};

#if defined(_WIN32)

HANDLE baronyWinHandle;

#elif defined(__linux__)

#else
#error "Couldn't detect target OS!"
#endif

mumble_error_t mumble_init(mumble_plugin_id_t pluginID) {
    ownID = pluginID;
    active = false;

    if (mumbleAPI.log(ownID, "BaronyPA Waiting") != MUMBLE_STATUS_OK) {
        // Logging failed -> usually you'd probably want to log things like this in your plugin's
        // logging system (if there is any)
    }

    return MUMBLE_STATUS_OK;
}

void mumble_shutdown() {
    active = false;
    if (mumbleAPI.log(ownID, "BaronyPA Stopping") != MUMBLE_STATUS_OK) {
        // Logging failed -> usually you'd probably want to log things like this in your plugin's
        // logging system (if there is any)
    }
}

// Positional Audio

uint8_t mumble_initPositionalData(const char *const *programNames, const uint64_t *programPIDS, size_t programCount) {
    // Check if Barony is open

    for (size_t i = 0; i < programCount; i++) {
        if (strcmp(programNames[i], "barony.exe") == 0) {
            baronyPID = programPIDS[i];
            mumbleAPI.log(ownID, "Found Barony:");
            mumbleAPI.log(ownID, programNames[i]);

            // On Windows we need to open the Barony process
            #if defined(_WIN32)
            baronyWinHandle = OpenProcess(PROCESS_VM_READ | PROCESS_QUERY_INFORMATION, false, baronyPID);

            // Also need to determine base address
            HMODULE moduleBuffer[1024];
            DWORD bytesNeeded = 0;
            EnumProcessModules(baronyWinHandle, moduleBuffer, sizeof(moduleBuffer), &bytesNeeded);
            baronyBaseAddr = (uint64_t) moduleBuffer[0];

            #elif defined(__linux__)
            baronyBaseAddr = 0x400000;
            #endif

            // Linux doesn't need to open first

            // Mumble will now start calling mumble_fetchPositionalData
            active = true;
            return MUMBLE_PDEC_OK;
        }
    }

    // Barony isn't open
    return MUMBLE_PDEC_ERROR_TEMP;
}

// Helper function that grabs the Positional data from the Barony process
bool getBaronyPos(struct baronyPosValue* target) {

    void *baronyAddr = (void*)(baronyBaseAddr + POS_OFFSET);

    // Method differs between Windows and Linux
    #if defined(_WIN32)

        // Check to make sure Barony still exists
        DWORD baronyStatus;
        if ((GetExitCodeProcess(baronyWinHandle, &baronyStatus) == 0)
                || (baronyStatus != STILL_ACTIVE)) {
            mumbleAPI.log(ownID, "Lost connection to Barony. Stopping.");
            return false;

        }

        // Access Barony's memory
        size_t bytesRead = 0;
        if (ReadProcessMemory(baronyWinHandle, baronyAddr, target, sizeof(struct baronyPosValue), &bytesRead) == 0) {
            // An error occurred
            DWORD error_code = GetLastError();
   
            char buffer[256];
            sprintf(buffer, "An error occurred in ReadProcessMemory, Error Code: %u, targeted address: 0x%p", error_code, baronyAddr);
            mumbleAPI.log(ownID, buffer);

            return false;
        }
        if (bytesRead < sizeof(struct baronyPosValue)) {
            // An error occurred
            char buffer[256];
            sprintf(buffer, "ReadProcessMemory only read %u bytes!", (unsigned int)bytesRead);
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
        struct iovec local_iov = {target, sizeof(struct baronyPosValue)};
        struct iovec remote_iov = {baronyAddr, sizeof(struct baronyPosValue)};

        ssize_t bytesRead = process_vm_readv(baronyPID, &local_iov, 1, &remote_iov, 1, 0);
        if (bytesRead < sizeof(struct baronyPosValue)) {
            // An error occurred
            mumbleAPI.log(ownID, "An error occurred in process_vm_readv().");
            return false;
        }
    #endif

    // No errors occurred
    return true;
}

bool mumble_fetchPositionalData(float *avatarPos, float *avatarDir, float *avatarAxis, float *cameraPos, float *cameraDir,
                                float *cameraAxis, const char **context, const char **identity) {

    // Make sure we've acquired a pid for Barony first
    if (!active) {
        return false;
    }

    // Get Position data from Barony
    static struct baronyPosValue value;

    if (!getBaronyPos(&value)) {
        // Unable to fetch data
        active = false;
        return false;
    }

    // Position
    avatarPos[0] = cameraPos[0] = value.x;
    avatarPos[1] = cameraPos[1] = value.y;
    avatarPos[2] = cameraPos[2] = value.z;

    // Need to get direction from Barony's angles (also needs to be a unit vector)
    float dirY = -sin(value.vert_angle);
    float magnitude = sqrt(1 + (dirY * dirY));

    avatarDir[0] = cameraDir[0] = sin(value.horiz_angle) / magnitude;
    avatarDir[1] = cameraDir[1] = dirY / magnitude;
    avatarDir[2] = cameraDir[2] = cos(value.horiz_angle) / magnitude;

    // Axis - Character seems to be about 0.6 meters tall
    avatarAxis[0] = cameraAxis[0] = 0;
    avatarAxis[1] = cameraAxis[1] = 0.6;
    avatarAxis[2] = cameraAxis[2] = 0;

    // Context - currently constant
    static const char *contextString = "BaronyPA";
    *context = contextString;

    char buffer[256];
    sprintf(buffer, "Pos: (%.1f, %.1f, %.1f) Dir: (%.1f, %.1f, %.1f)", avatarPos[0], avatarPos[1], avatarPos[2], avatarDir[0], avatarDir[1], avatarDir[2]);
    mumbleAPI.log(ownID, buffer);

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
    static const char *name = PLUGIN_NAME;

    struct MumbleStringWrapper wrapper;
    wrapper.data = name;
    wrapper.size = strlen(name);
    wrapper.needsReleasing = false;

    return wrapper;
}

mumble_version_t mumble_getVersion() {
    static const mumble_version_t version = {0, 1, 0};
    return version;
}

struct MumbleStringWrapper mumble_getAuthor() {
    static const char *author = "Shane Menzies";

    struct MumbleStringWrapper wrapper;
    wrapper.data = author;
    wrapper.size = strlen(author);
    wrapper.needsReleasing = false;

    return wrapper;
}

struct MumbleStringWrapper mumble_getDescription() {
    static const char *description = "Simple positional audio for Barony.";

    struct MumbleStringWrapper wrapper;
    wrapper.data = description;
    wrapper.size = strlen(description);
    wrapper.needsReleasing = false;

    return wrapper;
}

// Unmodified functions

uint32_t mumble_getFeatures() {
    return MUMBLE_FEATURE_POSITIONAL;
}

mumble_version_t mumble_getAPIVersion() {
    // This constant will always hold the API version  that fits the included header files
    return MUMBLE_PLUGIN_API_VERSION;
}

void mumble_registerAPIFunctions(void *apiStruct) {
    // Provided mumble_getAPIVersion returns MUMBLE_PLUGIN_API_VERSION, this cast will make sure
    // that the passed pointer will be cast to the proper type
    mumbleAPI = MUMBLE_API_CAST(apiStruct);
}

void mumble_releaseResource(const void *pointer) {
    // As we never pass a resource to Mumble that needs releasing, this function should never
    // get called
    printf("Called mumble_releaseResource but expected that this never gets called -> Aborting");
    abort();
}
