#include <stdio.h>
#define WIN32_LEAN_AND_MEAN
#define NOGDI
#define NOUSER
#include <windows.h>
#include "hotreload.h"

static const char *libplug_file_name = "./libplug.dll";
static const char *libplug_temp_file_name = "./libplug_temp.dll";
static void *libplug = NULL;

#define PLUG(name, ...) name##_t *name = NULL;
LIST_OF_PLUGS
#undef PLUG

char *win32_error_message(DWORD err) {
    static char win32ErrMsg[100] = {0};
    DWORD errMsgSize = FormatMessageA(FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS, NULL, err, LANG_USER_DEFAULT, win32ErrMsg,
                                      100, NULL);
    if (errMsgSize == 0) {
        if (GetLastError() != ERROR_MR_MID_NOT_FOUND) {
            if (sprintf(win32ErrMsg, "Could not get error message for 0x%lX", err) > 0) {
                return (char *)&win32ErrMsg;
            } else {
                return NULL;
            }
        } else {
            if (sprintf(win32ErrMsg, "Invalid Windows Error code (0x%lX)", err) > 0) {
                return (char *)&win32ErrMsg;
            } else {
                return NULL;
            }
        }
    }
    while (errMsgSize > 1 && isspace(win32ErrMsg[errMsgSize - 1])) {
        win32ErrMsg[--errMsgSize] = '\0';
    }
    return win32ErrMsg;
}

bool reload_libplug(void)
{
    if (libplug != NULL) {
        FreeLibrary(libplug);
        libplug = NULL;
    }
    
    // Delete old file
    DeleteFile(libplug_file_name);
    
    // Copy the new file
    if (!CopyFile(libplug_temp_file_name, libplug_file_name, FALSE)) {
        printf("HOTRELOAD_WIN32: Error copying libplug: %s\n", win32_error_message(GetLastError()));
        return false;
    }
    
    // Load the new library
    libplug = LoadLibrary(libplug_file_name);
    if (libplug == NULL) {
        printf("HOTRELOAD_WIN32 could not load %s: %s\n", libplug_file_name, win32_error_message(GetLastError()));
        return false;
    }
    
    // Load all function pointers
    #define PLUG(name, ...) \
        name = (void*)GetProcAddress(libplug, #name); \
        if (name == NULL) { \
            printf("HOTRELOAD_WIN32: could not find %s symbol in %s: %s\n", \
                     #name, libplug_file_name, win32_error_message(GetLastError())); \
            return false; \
        }
    LIST_OF_PLUGS
    #undef PLUG
    return true;
}