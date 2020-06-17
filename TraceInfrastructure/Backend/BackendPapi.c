#include "Backend/BackendPapi.h"
#include <papi.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

char *BackendPapiNames;
int PapiEventSet = PAPI_NULL;
uint64_t PapiEventCount;
long long *PapiData;
FILE *BackendPapiFile;
bool PapiRunning = 0;

int indeces[11];

void InitializePapi()
{
    PAPI_library_init(PAPI_VER_CURRENT);
    PAPI_create_eventset(&PapiEventSet);

    BackendPapiNames = (char *)malloc(58 * PAPI_MAX_STR_LEN);
    strcpy(BackendPapiNames + PAPI_MAX_STR_LEN * 0, "PAPI_TOT_INS");
    strcpy(BackendPapiNames + PAPI_MAX_STR_LEN * 1, "PAPI_TOT_CYC");
    strcpy(BackendPapiNames + PAPI_MAX_STR_LEN * 2, "PAPI_L1_DCM");
    strcpy(BackendPapiNames + PAPI_MAX_STR_LEN * 3, "PAPI_L1_ICM");
    strcpy(BackendPapiNames + PAPI_MAX_STR_LEN * 4, "PAPI_L2_ICM");
    strcpy(BackendPapiNames + PAPI_MAX_STR_LEN * 5, "PAPI_L1_TCM");
    strcpy(BackendPapiNames + PAPI_MAX_STR_LEN * 6, "PAPI_L2_ICA");
    strcpy(BackendPapiNames + PAPI_MAX_STR_LEN * 7, "PAPI_L3_ICA");
    strcpy(BackendPapiNames + PAPI_MAX_STR_LEN * 8, "PAPI_L2_ICR");
    strcpy(BackendPapiNames + PAPI_MAX_STR_LEN * 9, "PAPI_L3_ICR");

    strcpy(BackendPapiNames + PAPI_MAX_STR_LEN * 10, "PAPI_L2_DCM");
    strcpy(BackendPapiNames + PAPI_MAX_STR_LEN * 11, "PAPI_L2_TCM");
    strcpy(BackendPapiNames + PAPI_MAX_STR_LEN * 12, "PAPI_L3_TCM");
    strcpy(BackendPapiNames + PAPI_MAX_STR_LEN * 13, "PAPI_L3_DCA");
    strcpy(BackendPapiNames + PAPI_MAX_STR_LEN * 14, "PAPI_L3_TCA");
    strcpy(BackendPapiNames + PAPI_MAX_STR_LEN * 15, "PAPI_CA_SNP");
    strcpy(BackendPapiNames + PAPI_MAX_STR_LEN * 16, "PAPI_CA_SHR");
    strcpy(BackendPapiNames + PAPI_MAX_STR_LEN * 17, "PAPI_CA_CLN");
    strcpy(BackendPapiNames + PAPI_MAX_STR_LEN * 18, "PAPI_CA_INV");
    strcpy(BackendPapiNames + PAPI_MAX_STR_LEN * 19, "PAPI_CA_ITV");

    strcpy(BackendPapiNames + PAPI_MAX_STR_LEN * 20, "PAPI_TLB_IM");
    strcpy(BackendPapiNames + PAPI_MAX_STR_LEN * 21, "PAPI_L3_LDM");
    strcpy(BackendPapiNames + PAPI_MAX_STR_LEN * 22, "PAPI_TLB_DM");
    strcpy(BackendPapiNames + PAPI_MAX_STR_LEN * 23, "PAPI_L1_LDM");
    strcpy(BackendPapiNames + PAPI_MAX_STR_LEN * 24, "PAPI_L1_STM");
    strcpy(BackendPapiNames + PAPI_MAX_STR_LEN * 25, "PAPI_L2_LDM");
    strcpy(BackendPapiNames + PAPI_MAX_STR_LEN * 26, "PAPI_PRF_DM");
    strcpy(BackendPapiNames + PAPI_MAX_STR_LEN * 27, "PAPI_L2_STM");
    strcpy(BackendPapiNames + PAPI_MAX_STR_LEN * 28, "PAPI_MEM_WCY");
    strcpy(BackendPapiNames + PAPI_MAX_STR_LEN * 29, "PAPI_STL_ICY");

    strcpy(BackendPapiNames + PAPI_MAX_STR_LEN * 30, "PAPI_L3_DCW");
    strcpy(BackendPapiNames + PAPI_MAX_STR_LEN * 31, "PAPI_L3_TCW");
    strcpy(BackendPapiNames + PAPI_MAX_STR_LEN * 32, "PAPI_FUL_ICY");
    strcpy(BackendPapiNames + PAPI_MAX_STR_LEN * 33, "PAPI_STL_CCY");
    strcpy(BackendPapiNames + PAPI_MAX_STR_LEN * 34, "PAPI_FUL_CCY");
    strcpy(BackendPapiNames + PAPI_MAX_STR_LEN * 35, "PAPI_BR_UCN");
    strcpy(BackendPapiNames + PAPI_MAX_STR_LEN * 36, "PAPI_BR_CN");
    strcpy(BackendPapiNames + PAPI_MAX_STR_LEN * 37, "PAPI_BR_INS");
    strcpy(BackendPapiNames + PAPI_MAX_STR_LEN * 38, "PAPI_BR_TKN");
    strcpy(BackendPapiNames + PAPI_MAX_STR_LEN * 39, "PAPI_BR_NTK");

    strcpy(BackendPapiNames + PAPI_MAX_STR_LEN * 40, "PAPI_BR_MSP");
    strcpy(BackendPapiNames + PAPI_MAX_STR_LEN * 41, "PAPI_BR_PRC");
    strcpy(BackendPapiNames + PAPI_MAX_STR_LEN * 42, "PAPI_LD_INS");
    strcpy(BackendPapiNames + PAPI_MAX_STR_LEN * 43, "PAPI_SR_INS");
    strcpy(BackendPapiNames + PAPI_MAX_STR_LEN * 44, "PAPI_RES_STL");
    strcpy(BackendPapiNames + PAPI_MAX_STR_LEN * 45, "PAPI_L2_DCA");
    strcpy(BackendPapiNames + PAPI_MAX_STR_LEN * 46, "PAPI_L2_DCR");
    strcpy(BackendPapiNames + PAPI_MAX_STR_LEN * 47, "PAPI_L3_DCR");
    strcpy(BackendPapiNames + PAPI_MAX_STR_LEN * 48, "PAPI_L2_DCW");
    strcpy(BackendPapiNames + PAPI_MAX_STR_LEN * 49, "PAPI_L2_ICH");

    strcpy(BackendPapiNames + PAPI_MAX_STR_LEN * 50, "PAPI_L2_TCW");
    strcpy(BackendPapiNames + PAPI_MAX_STR_LEN * 51, "PAPI_L2_TCA");
    strcpy(BackendPapiNames + PAPI_MAX_STR_LEN * 52, "PAPI_L2_TCR");
    strcpy(BackendPapiNames + PAPI_MAX_STR_LEN * 53, "PAPI_L3_TCR");
    strcpy(BackendPapiNames + PAPI_MAX_STR_LEN * 54, "PAPI_LST_INS");
    strcpy(BackendPapiNames + PAPI_MAX_STR_LEN * 55, "PAPI_HW_INT");
    strcpy(BackendPapiNames + PAPI_MAX_STR_LEN * 56, "PAPI_L1_DCA");
    strcpy(BackendPapiNames + PAPI_MAX_STR_LEN * 57, "PAPI_L2_DCA");

    indeces[0] = 0;
    indeces[1] = 1;
    PapiEventCount = 2;
    char *pi = getenv("PAPI_INDEX");
    char *env = getenv("PAPI_PLATFORM");
    if (pi == NULL)
    {
        pi = "0";
    }
    if (env == NULL)
    {
        env = "SPADE";
    }
    int PapiIndex = atoi(pi);
    int PapiPlatform = -1;
    if (!strcmp(env, "SPADE"))
    {
        PapiPlatform = 0;
    }
    else if (!strcmp(env, "ZYNQ"))
    {
        PapiPlatform = 1;
    }
    switch (PapiPlatform)
    {
        case 0:
        {
            switch (PapiIndex)
            {
                case 0:
                {
                    indeces[2] = 2;
                    indeces[3] = 3;
                    indeces[4] = 4;
                    indeces[5] = 5;
                    indeces[6] = 6;
                    indeces[7] = 7;
                    indeces[8] = 8;
                    indeces[9] = 9;
                    PapiEventCount = 10;
                    break;
                }
                case 1:
                {
                    indeces[2] = 10;
                    indeces[3] = 11;
                    indeces[4] = 12;
                    indeces[5] = 13;
                    indeces[6] = 14;
                    PapiEventCount = 7;
                    break;
                }
                case 2:
                {
                    indeces[2] = 15;
                    indeces[3] = 16;
                    indeces[4] = 17;
                    PapiEventCount = 5;
                    break;
                }
                case 3:
                {
                    indeces[2] = 18;
                    indeces[3] = 19;
                    indeces[4] = 20;
                    PapiEventCount = 5;
                    break;
                }
                case 4:
                {
                    indeces[2] = 21;
                    PapiEventCount = 3;
                    break;
                }
                case 5:
                {
                    indeces[2] = 22;
                    indeces[3] = 23;
                    PapiEventCount = 4;
                    break;
                }
                case 6:
                {
                    indeces[2] = 24;
                    indeces[3] = 25;
                    indeces[4] = 26;
                    PapiEventCount = 5;
                    break;
                }
                case 7:
                {
                    indeces[2] = 27;
                    indeces[3] = 28;
                    indeces[4] = 29;
                    indeces[5] = 30;
                    indeces[6] = 31;
                    PapiEventCount = 7;
                    break;
                }
                case 8:
                {
                    indeces[2] = 32;
                    indeces[3] = 33;
                    PapiEventCount = 4;
                    break;
                }
                case 9:
                {
                    indeces[2] = 34;
                    indeces[3] = 35;
                    indeces[4] = 36;
                    indeces[5] = 37;
                    PapiEventCount = 6;
                    break;
                }
                case 10:
                {
                    indeces[2] = 38;
                    indeces[3] = 39;
                    indeces[4] = 40;
                    indeces[5] = 41;
                    PapiEventCount = 6;
                    break;
                }
                case 11:
                {
                    indeces[2] = 42;
                    PapiEventCount = 3;
                    break;
                }
                case 12:
                {
                    indeces[2] = 43;
                    PapiEventCount = 3;
                    break;
                }
                case 13:
                {
                    indeces[2] = 44;
                    indeces[3] = 45;
                    indeces[4] = 46;
                    PapiEventCount = 5;
                    break;
                }
                case 14:
                {
                    indeces[2] = 47;
                    indeces[3] = 48;
                    indeces[4] = 49;
                    indeces[5] = 50;
                    PapiEventCount = 6;
                    break;
                }
                case 15:
                {
                    indeces[2] = 51;
                    indeces[3] = 52;
                    PapiEventCount = 4;
                    break;
                }
                case 16:
                {
                    indeces[2] = 53;
                    PapiEventCount = 3;
                    break;
                }
                case 17:
                {
                    indeces[1] = 54;
                    PapiEventCount = 2;
                    break;
                }
                default:
                {
                    printf("Invalid PapiIndex for SPADE platform.\n");
                    break;
                }
            }
            break;
        }
        case 1:
        {
            switch (PapiIndex)
            {
                case 0:
                {
                    indeces[2] = 2;
                    indeces[3] = 3;
                    indeces[4] = 10;
                    indeces[5] = 22;
                    indeces[6] = 20;
                    PapiEventCount = 7;
                    break;
                }
                case 1:
                {
                    indeces[2] = 55;
                    indeces[3] = 40;
                    indeces[4] = 42;
                    indeces[5] = 43;
                    indeces[6] = 37;
                    PapiEventCount = 7;
                    break;
                }
                case 2:
                {
                    indeces[2] = 56;
                    indeces[3] = 57;
                    PapiEventCount = 4;
                    break;
                }
                default:
                {
                    printf("Invalid PapiIndex for ZYNQ platform.\n");
                    break;
                }
            }
            break;
        }
        default:
        {
            printf("Invalid platform specified. No performance counters will be used.\n");
        }
    }

    for (uint32_t i = 0; i < PapiEventCount; i++)
    {
        int code = PAPI_NULL;
        int index = indeces[i];
        if (PAPI_event_name_to_code(&BackendPapiNames[index * PAPI_MAX_STR_LEN], &code) != PAPI_OK)
        {
            printf("Failed to map event %s\n", &BackendPapiNames[index * PAPI_MAX_STR_LEN]);
        }
        if (PAPI_add_event(PapiEventSet, code) != PAPI_OK)
        {
            printf("Failed to add event %s\n", &BackendPapiNames[index * PAPI_MAX_STR_LEN]);
        }
    }

    PapiData = (long long *)malloc(PapiEventCount * (sizeof(long long)));
    memset(PapiData, 0, PapiEventCount * (sizeof(long long)));
}

void TerminatePapi()
{
    if (PapiRunning)
    {
        StopPapi();
    }
    long long tempData[PapiEventCount];
    if (PAPI_stop(PapiEventSet, tempData) != PAPI_OK)
    {
        printf("Failed to terminate PAPI\n");
    }

    char *pn = getenv("PAPI_NAME");
    if (pn == NULL)
    {
        pn = "papi.csv";
    }

    BackendPapiFile = fopen(pn, "w");
    for (uint32_t i = 0; i < PapiEventCount; i++)
    {
        char stringData[PAPI_MAX_STR_LEN + 64];
        char tempData[PAPI_MAX_STR_LEN];
        int index = indeces[i];
        strncpy(tempData, BackendPapiNames + index * (PAPI_MAX_STR_LEN), PAPI_MAX_STR_LEN);
        sprintf(stringData, "%s,%lld\n", tempData, PapiData[i]);
        fputs(stringData, BackendPapiFile);
    }
    fclose(BackendPapiFile);

    free(BackendPapiNames);
    free(PapiData);
}

void StartPapi()
{
    PAPI_start(PapiEventSet);
    PapiRunning = true;
}

void StopPapi()
{
    if (PAPI_accum(PapiEventSet, PapiData) != PAPI_OK)
    {
        printf("Failed to stop event counter\n");
    }
    PapiRunning = false;
}

void CertifyPapiOn()
{
    if (!PapiRunning)
    {
        StartPapi();
    }
}

void CertifyPapiOff()
{
    if (PapiRunning)
    {
        StopPapi();
    }
}