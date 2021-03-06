#include <mpi.h>
#include "ap_config.h"
#include <sys/types.h>
#include <unistd.h>
#include <uuid/uuid.h>

/*====================================================*/
/* Collect and output data                            */
/*====================================================*/
void AP_CollectAndOutputAll() {

  /*--------------*/
  /* Get data     */
  /*--------------*/

  ap_settings_t *settings = AP_getSettings();

  ap_sysData_t sysData;
  AP_Sys_GetData(&sysData);

  ap_procData_t procData;
  AP_Proc_GetData(&procData);

  ap_hpmData_t hpmData;
  AP_HPM_GetData(&hpmData);

  ap_mpiData_t mpiData;
  AP_MPI_GetData(&mpiData);
  
  /*------------*/
  /* Open files */
  /*------------*/


  uuid_t jobId;
  if (procData.disabled == 0) {
    jobId = procData.jobId;
  } else {
    uuid_generate_time(&jobId);
    
  }

  int rank;
  if (mpiData.disabled == 0) {
    MPI_Comm_rank(MPI_COMM_WORLD, &rank); // TODO look at alternative to MPI call
  } else {
    rank = mpiData.myRank;
  }
  
  FILE *locFile = NULL;
  if (settings->flags.output_local != 0) {
    locFile = AP_openOutputFile(OUTPUT_LOCAL_DIR, 0, jobId, rank, settings->flags.debug_level);
  }

  FILE *sysFile = NULL;
  if (settings->flags.output_sys != 0) {
    sysFile = AP_openOutputFile(OUTPUT_SYS_DIR, 1, jobId, rank, settings->flags.debug_level);
  }

  /*-------------*/
  /* write data  */
  /*-------------*/

  AP_writeSettings(locFile, settings);
  AP_writeSettings(sysFile, settings);

  AP_writeSysData(locFile, "", &sysData);
  AP_writeSysData(sysFile, "", &sysData);

  AP_writeProcData(locFile, "", &procData);
  AP_writeProcData(sysFile, "", &procData);

  AP_writeHPMData(locFile, "", &hpmData);
  AP_writeHPMData(sysFile, "", &hpmData);

  AP_writeMPIData(locFile, "", &mpiData);
  AP_writeMPIData(sysFile, "", &mpiData);

  /*-------------*/
  /* close files */
  /*-------------*/

  if (locFile != NULL) fclose(locFile);
  if (sysFile != NULL) fclose(sysFile);


  return;
}
