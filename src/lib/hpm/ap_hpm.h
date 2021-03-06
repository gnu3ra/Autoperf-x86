#include <stdint.h>
#include "time/ap_time.h"

//#define  HPM_NUM_P_EVENTS 6
//#define  HPM_NUM_L2_EVENTS 2 //used to be 4
//#define  HPM_NUM_NW_EVENTS 0 //used to be 2
//#define  HPM_NUM_EVENTS (HPM_NUM_P_EVENTS+HPM_NUM_L2_EVENTS+HPM_NUM_NW_EVENTS)
#define PAPI_NUM_EVENTS 4

static int pEventList[PAPI_NUM_EVENTS] =   {
               //PAPI_L1_DCM,
						     //  PAPI_L1_ICM,
						      PAPI_L2_DCM,
						      PAPI_L2_ICM,
						      PAPI_TOT_CYC,
						      PAPI_BR_MSP};


typedef struct {
  int disabled;
  ap_cycle_t startCycle, stopCycle, elapsedCycles;
  double elapsedTime;
  long long counts[PAPI_NUM_EVENTS];
  int ids[PAPI_NUM_EVENTS];
  int pFirstIndex, pLastIndex;
  int L2FirstIndex, L2LastIndex;
  int NWFirstIndex, NWLastIndex;
} ap_hpmData_t;


int AP_HPM_Init(int);
int AP_HPM_Start(void);
int AP_HPM_Stop(void);
int AP_HPM_Finalize(void);
int AP_HPM_GetData(ap_hpmData_t *);
void AP_HPM_GetEventName(int, char *);
