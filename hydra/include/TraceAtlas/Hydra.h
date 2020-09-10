#pragma once


#ifdef __cplusplus
extern "C" {
#endif

void HydraInit();

void HydraExecute(char* function);

void HydraLock(int code);

void HydraUnlock(int code);

void HydraWait(int code);

#ifdef __cplusplus
}
#endif