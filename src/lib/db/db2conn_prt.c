
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <limits.h>
#include <sys/types.h>
#include <unistd.h>
#include <mpi.h>
#include <pwd.h>
//#include <mysql.h>

#define DB_ERROR -1

typedef struct {
    int num_columns;
    char **column;
} db_result;


/*
typedef struct {
  MYSQL_RES *result_set;
  db_result result;
  int *result_reference;
} result_info;
*/


struct eventstruct {
                     double tbeg;
                     double tend;
                     int taskid;
                     int eventid;
                     int src;
                     int dest;
                     int bytes;
                     int parent;
                     int grandparent;
                     int pad;
                   };

struct omp_summary_rec {
      int task_id;
      int count;
      double exclusive_time;
      double inclusive_time;
      double overhead;
      double master_time;
      double thread_time;
      double compute_time;
      double imbalance_pct;
      double avg_thread_time;
      double to_time;
      double to_barrier_time;
      double to_rtl_time;
      int region_label;
};




#define RESULTS_SIZE 20

//MYSQL *conn;

int auto_commit_count = 0;
static char row_count_query[512];
//static result_info query_results[RESULTS_SIZE];
char insert_str[1024];
char delete_str[1024];
FILE *fundo,*finput;
static int string_query_result=-1;
static char query[2048];
static int run_query_result = -1;
static int region_query_result = -1;
static int hpc_run_id;
static int hpc_task_id;
static int mysqlinit=0;
static int db_input=1;
static int job_id;

static char sql_emsg[512];

db_result *db_get_row(int result_handle);
int get_string_id (char *, char *);
void db_release_results(int result_handle);



void fprintf_input(char *input_str) {
//printf("input_str=%s\n",input_str);
   if(input_str==NULL)
	return;
   fprintf(finput,"%s\n",input_str);
   return;
}

void fprintf_undo(char *undo_str) {
//printf("undo_str=%s\n",undo_str);

   if(undo_str==NULL)
	return;

fprintf(fundo,"%s\n",undo_str);

return;

}

/******************************************************************************/
/* Connect to database                                                        */
/* Parameters:                                                                */
/*       host - IP address (seen by the compute nodes)                        */
/*       port - service port                                                  */
/*       user - Username with which the DB to be authenticated                */
/*       passwd - User password                                               */
/*       db - Database name                                                   */
/* Output:                                                                    */
/*       0 - success                                                          */
/*       -1 - Failure                                                         */
/******************************************************************************/
int sql_connect(const char *host, const int port, const char *user, const char *passwd, const char *db, int jobid, int procrank)
{
   char *dbhost,*dbportstr,*dbuser,*dbpasswd,*dbname;
   int dbport;
   char fname[512];
   char *dbpath;

   /* DB2 leaves it to OS to impose length of the password */
   char password_str[256]; /* On AIX max password length 256. Linux ??*/
   char *envvar;

   if(mysqlinit>0) {
        mysqlinit++;
        return 0;
   }

   job_id = getpid();


   if(procrank==0) {
     printf("Job ID is %d\n",job_id);
   }

//   dbhost=getenv("DBHOST"); if(dbhost==NULL) dbhost=host;
//   dbportstr=getenv("DBPORT"); if(dbportstr==NULL) dbport=port; else dbport=atoi(dbportstr);
   dbuser=getenv("DBUSER"); if(dbuser==NULL) dbuser=user;
   dbpasswd=getenv("DBPASSWD"); if(dbpasswd==NULL) dbpasswd=passwd;
   dbname=getenv("DBNAME"); if(dbname==NULL) dbname=db;
   dbpath=getenv("DBPATH"); 

   /* sql files */ /* TODO need to check if dir exists */
   if (dbpath==NULL)
      sprintf(fname,"perfdb_input_%d_%d.sql\0",jobid,procrank);
   else  
      sprintf(fname,"%s/perfdb_input_%d_%d.sql\0",dbpath,jobid,procrank);
   finput=fopen(fname,"w");
//printf("input file name=%s ",fname); if (finput==NULL) printf("finput==null\n");
   sprintf(insert_str,"-- At the Unix command prompt: db2 -td@ -vf %s",fname);
   fprintf_input(insert_str);
   sprintf(insert_str,"connect to %s user %s using %s@\nbegin",dbname,dbuser,dbpasswd);
   fprintf_input(insert_str);
   sprintf(insert_str,"declare runid int;\ndeclare taskid int;\ndeclare regionid int;\ndeclare stringid1 int;\ndeclare stringid2 int;\ndeclare hpmref int;\ndeclare count int;\ndeclare loginid int;");
   fprintf_input(insert_str);

   if (dbpath==NULL)
      sprintf(fname,"perfdb_undo_%d_%d.sql\0",jobid,procrank);
   else
      sprintf(fname,"%s/perfdb_undo_%d_%d.sql\0",dbpath,jobid,procrank);

   fundo=fopen(fname,"w");

//printf("output file name=%s ",fname); if (fundo==NULL) printf("fundo==null\n");
   sprintf(delete_str,"-- At the Unix command prompt: db2 -td@ -vf %s",fname);
   fprintf_undo(delete_str);
   sprintf(delete_str,"connect to %s user %s using %s@\nbegin",dbname,dbuser,dbpasswd);
   fprintf_undo(delete_str);
   sprintf(delete_str,"declare runid int;\ndeclare taskid int;\ndeclare regionid int;\ndeclare stringid1 int;\ndeclare stringid2 int;\ndeclare hpmref int;\ndeclare count int;\ndeclare loginid int;");
   fprintf_undo(delete_str);



   envvar=getenv("DBINPUT");
   if (envvar!=NULL && ((strcasecmp(envvar,"no")==0) || (strcasecmp(envvar,"n")==0))) {
	     db_input=0;
             if(procrank==0)
                printf("DBINPUT is set to be %s, the performance data will not be stored into the database directly\n",envvar);
             return 0;
   }

/*
   conn = mysql_init(NULL);

   if (mysql_real_connect(conn, dbhost,
         dbuser, dbpasswd, dbname, dbport, NULL, 0)==NULL) {
      fprintf(stderr, "%u %s\n", mysql_errno(conn), mysql_error(conn));
      db_input=0;
   }
   else 
      mysql_autocommit(conn,0);
*/



   mysqlinit=1;
   
   return 0;

}

/******************************************************************************/
/* Commit and close the database                                              */
/* Parameters:                                                                */
/*       Void                                                                 */
/* Output:                                                                    */
/******************************************************************************/
void db_close(int id)
{

   if(mysqlinit>1) {
	mysqlinit--;
	return;
   }
   else if (mysqlinit==0)
	return;

   sprintf(insert_str,"end@");
   fprintf_input(insert_str);
   sprintf(delete_str,"end@");
   fprintf_undo(delete_str);


   fclose(fundo);
   fclose(finput);

   if(db_input==0)
	return;


/*
   mysql_commit(conn);
   mysql_close(conn);
*/

   mysqlinit=0;
   return;
}


int db_add_run(int jobid, int *insert_id)
{
   int rc;

   char *login_s;
   int loginid;

   int tag;
   char *envvar;


       char *jobcmd;
       char *token;
       char *block=NULL;
       char *np_s=NULL;
       int np=-1;
       char *mapping=NULL;
       char *rpn_s=NULL;
       int rpn=1;
       char *ompnum_s=NULL;
       int ompnum=1;
       char args[4096];
       char *exe;
       char *jobsize=NULL;

  

//   passwd = getpwuid (getuid ());

 
//   loginid=get_string_id(passwd->pw_name); 

   login_s=getenv("USER");
   if(login_s!=NULL) {
        loginid=get_string_id(login_s,"loginid");
    }
    else {
        loginid=get_string_id("(null)","loginid");


  }


   envvar=getenv("DBTAG");
   if (envvar==NULL)
	tag=0; 
   else 
	tag=atoi(envvar);

   envvar=getenv("DBNOTE");
   if(envvar==NULL) 
	envvar=strdup("\0");

   jobsize=getenv("DBJOBSIZE");
   if (jobsize==NULL)
        jobsize=strdup("\0");

   mapping=getenv("RUNJOB_MAPPING");
   if (mapping==NULL)
	mapping=strdup("\0");
 
   args[0]='\0';
   jobcmd=getenv("DBJOBCMD");
        if(jobcmd!=NULL) {


                token=strtok(jobcmd," ");
                while(token) {
/*                        if(strcmp("--block",token)==0) {
                                block=strtok(NULL," ");
                                token=strtok(NULL," ");
                        }

                        else*/ if((strcmp("--nodecount",token)==0) ||  (strcmp("-n",token)==0)) {
                                np_s=strtok(NULL," ");
                                np=atoi(np_s);
                                token=strtok(NULL," ");
                        }
/*
                        else if(strcmp("--mapping",token)==0) {
                                mapping=strtok(NULL," ");
                                token=strtok(NULL," ");
                        } 
*/
                        else if(strcmp("--mode",token)==0) {
                                rpn_s=strtok(NULL," ");
				rpn_s++; // skip "c"
                                rpn=atoi(rpn_s);
                                token=strtok(NULL," ");
                        }
#if 0 // TODO.. parse the exe and args
                        else if(strcmp("--args", token)==0) {
                                token=strtok(NULL," ");
//printf("1:token=[%s]\n",token);
                                strcat(args,token);
                                 strcat(args," ");
//prtinf("1:args=[%s]\n",args);
                                  token=strtok(NULL," ");
                                  while((token!=NULL) && ((token[0]!='-') || (token[1]!='-'))) {
                                        strcat(args,token);
                                        strcat(args," ");
                                        token=strtok(NULL," ");
                                }
                        }
                        else if(strcmp("--exe",token)==0) {
                                exe=strtok(NULL," ");
                                token=strtok(NULL," ");
                        }
                        else if(token[0]==':') {
                                exe=strtok(NULL," "); // to get the exe
                                token=strtok(NULL," "); // to get first args
                                while(token!=NULL) {
                                        strcat(args,token);
                                        strcat(args," ");
                                        token=strtok(NULL," ");
                                }
                        }
#endif
                        else
                                token=strtok(NULL," ");

                }
        }
        else printf("DBJOBCMD is not set!\n");
   if(block==NULL) block=strdup("\0");
   if(mapping==NULL) mapping=strdup("\0");

   ompnum_s=getenv("OMP_NUM_THREADS");
   if(ompnum_s!=NULL)
        ompnum=atoi(ompnum_s);


//  sprintf(insert_str,"declare runid int;"); 
//  fprintf_input(insert_str);
   sprintf(insert_str, "set runid=(select hpc_run_id from final table (insert into hpc_run "
             "(hpc_run_id, hpc_run_date,job_id,login_id, tag, note, NP, RPN, OMP, mapping, block,args,exe,size) values(default, current_timestamp,%d,loginid,%d,'%s',%d,%d,%d,'%s','%s','%s','%s','%s')) order by hpc_run_id desc fetch first 1 rows only);",job_id,tag,envvar,np,rpn,ompnum,mapping,block,args,exe,jobsize);
   fprintf_input(insert_str);


   sprintf(delete_str, "delete from hpc_run where job_id=%d and login_id=loginid and tag=%d and note='%s' and NP=%d and RPN=%d and OMP=%d and mapping='%s' and block='%s' and args='%s' and exe='%s' and size='%s';", jobid, tag,envvar,np,rpn,ompnum,mapping,block,args,exe,jobsize);

   fprintf_undo(delete_str);


   if(db_input==0)
	return 0;


   if((rc = db_add_row(insert_str, &hpc_run_id)) != 0)
   {
      fprintf(stderr, "Add to hpc_run table failed\n");
      return -1;
   }
   if(insert_id)
      *insert_id = hpc_run_id;
//   mysql_commit(conn);

   return 0;
}




/******************************************************/
/* Add Rows to hpc_task table                         */
/* Return Values:                                     */
/* Success ==> 0                                      */
/* Failure ==> -1                                     */
/******************************************************/
int db_add_task(/*task_rec *task_info,*/int jobid, int procrank, int mpirank, int threadid, int *insert_id)
{
   int rc;
   int taskref;

   //hpc_run_id=get_run_id(job_id);
//   sprintf(insert_str,"declare taskid int;\ndeclare regionid int;\ndeclare stringid1 int;\ndeclare hpmref int;");
//   fprintf_input(insert_str);
   sprintf(insert_str, "set runid=(select hpc_run_id from hpc_run where job_id=%d);",job_id); 
   fprintf_input(insert_str);
   sprintf(insert_str, "set taskid=(select hpc_task_ref from final table (insert into hpc_task " 
         "(hpc_task_ref, hpc_run_id, hpc_world_id, hpc_task_id, hpc_thread_id, job_id) "
            "values(default, runid, %d, %d, %d,%d)) order by hpc_task_ref desc fetch first 1 rows only);",
            hpc_run_id, mpirank, procrank, threadid,job_id); // no threading currently
   fprintf_input(insert_str);


   sprintf(delete_str, "delete from hpc_task where job_id=%d and hpc_world_id=%d and hpc_task_id=%d and hpc_thread_id=%d;", job_id,  mpirank,procrank,threadid);

   fprintf_undo(delete_str);

   if(db_input==0)
	return 0;

   /* Add row to hpc_task_ref table */
   if((rc = db_add_row(insert_str, &taskref)) != 0)
   {
      fprintf(stderr, "Add to hpc_task table failed\n");
      return -1;
   }


//   mysql_commit(conn);

   if(insert_id)
	*insert_id=taskref;
   else
	hpc_task_id=taskref; // set as "default" for the process

   return 0;


}



/******************************************************/
/* Add Row to hpc_region_table                        */
/* Return Values:                                     */
/* Success ==> 0                                      */
/* Failure ==> -1                                     */
/******************************************************/
int db_add_region(int fileid, int funcid, int sline, int eline, int *insert_id)
{
   int hpc_region_id, rc;

   /* Entry not found in the in-memory table. Hence add to DB */
   sprintf(insert_str, "set regionid=(select hpc_region_id from final table (insert into hpc_region "
            "(hpc_region_id, hpc_source_file_id, hpc_function_id, \
               hpc_start_line, hpc_end_line) " 
            "values(default, %d, %d, %d, %d)) order by hpc_region_id desc fetch first 1 rows only);", fileid, funcid,
            sline, eline);
   fprintf_input(insert_str);


   /* TODO do we need this? */
   if(db_input==0)
	return 0;

   /* Add row to hpc_region_table */
   if((rc = db_add_row(insert_str, &hpc_region_id)) != 0)
   {
      fprintf(stderr, "Add row to hpc_region table failed\n");
      return -1;
   }

//   mysql_commit(conn);
   
   if(insert_id)
       *insert_id = hpc_region_id;
   return 0;
}

 
int get_region_id(char *filename, char *funcname, int sline, int eline)
{
   int fileid, funcid;
   int id,count;
   db_result *row;

  if(db_input==0)
	return -1;
/*
  if (region_query_result != -1) {
    db_release_results(region_query_result);
    region_query_result = -1;
  }
*/

//printf("filename=%s, funcname=%s\n",filename,funcname);

  fileid=get_string_id(filename,"stringid1");
  funcid=get_string_id(funcname,"stringid2");

  sprintf(query,"set count=(select count(*) from hpc_region where HPC_SOURCE_FILE_ID  = stringid1 and HPC_FUNCTION_ID = stringid2 and HPC_START_LINE = %d and HPC_END_LINE = %d);",sline,eline);
  fprintf_input(query);
  sprintf(query,"if (count=0) then");
  fprintf_input(query);
//  db_add_region(fileid,funcid,sline,eline,&id);
   sprintf(insert_str, "set regionid=(select hpc_region_id from final table (insert into hpc_region "
            "(hpc_region_id, hpc_source_file_id, hpc_function_id, \
               hpc_start_line, hpc_end_line) "
            "values(default, stringid1, stringid2, %d, %d)) order by hpc_region_id desc fetch first 1 rows only);", sline, eline);
   fprintf_input(insert_str);

  sprintf(query,"else set regionid=(select hpc_region_id from hpc_region where HPC_SOURCE_FILE_ID  = stringid1 and HPC_FUNCTION_ID = stringid2 and HPC_START_LINE = %d and HPC_END_LINE = %d fetch first 1 rows only);",sline,eline);
  fprintf_input(query);
  sprintf(query,"end if;");
  fprintf_input(query);

  sprintf(query,"set regionid=(select hpc_region_id from hpc_region where HPC_SOURCE_FILE_ID  = stringid1 and HPC_FUNCTION_ID = stringid2 and HPC_START_LINE = %d and HPC_END_LINE = %d fetch first 1 rows only);",sline,eline);
  fprintf_undo(query);

  return 0;

/*

  sprintf(query, "set regionid=(select hpc_region_id from hpc_region "
          "where HPC_SOURCE_FILE_ID  = %d and HPC_FUNCTION_ID = %d and HPC_START_LINE = %d and HPC_END_LINE = %d fetch first 1 rows only);", fileid, funcid, sline, eline);
  db_query_table(query, &region_query_result);

  if (region_query_result < 0) {
    return -1;
  }

  count=db_get_row_count(region_query_result);


  if (count==0) { // add the new string into the table 
    db_add_region( fileid, funcid, sline, eline, &id);

  }
  
  else {
    row = db_get_row(region_query_result);
    if (row == NULL) {
      db_release_results(region_query_result);
      region_query_result = -1;
      return -1;
    }
    id = atoi(row->column[0]);
    db_release_results(region_query_result);
    region_query_result = -1;
  }
  return id;

*/

}


/******************************************************************************/
/* Add rows under the database tables                                         */
/* Parameters:                                                                */
/*       insert_statement - An insert SQL query statement specifying tabele   */
/*                          name and it's values                              */
/*       *row_id - Pointer in which the auto_increment value is returned      */
/* Output:                                                                    */
/*       0 - success                                                          */
/*       -1 - Failure                                                         */
/******************************************************************************/
int db_add_row(char *insert_statement, int *row_id)
{

  int rc;
   if(insert_statement == NULL)
      return -1;
   *row_id=-1;
/*
   if((rc = mysql_query(conn, insert_statement)) != 0)
   {
      fprintf(stderr, "Insert into table failed: %s\n", mysql_error(conn));
      return -1;
   }

   if(row_id != NULL)
      *row_id = (int)mysql_insert_id(conn);  Check whether it's the right API 
   auto_commit_count++;
   if(!(auto_commit_count % 1000))
   {
      mysql_commit(conn);
      auto_commit_count = 0;
   }
*/
   return 0;

}

#if 0 /* TODO : need to check if still needed */
int db_get_row_count(int result_handle)
{
  if ((result_handle < 0) || (result_handle >= RESULTS_SIZE) ||
      (query_results[result_handle].result_set == NULL)) {
    return -1;
  }
  return mysql_num_rows(query_results[result_handle].result_set);

}


db_result *db_get_row(int result_handle)
{
  if ((result_handle < 0) || (result_handle >= RESULTS_SIZE) ||
      (query_results[result_handle].result_set == NULL)) {
    return NULL;
  }
  query_results[result_handle].result.num_columns =
        mysql_num_fields(query_results[result_handle].result_set);
  query_results[result_handle].result.column =
        mysql_fetch_row(query_results[result_handle].result_set);
  if ((query_results[result_handle].result.num_columns == 0) ||
      (query_results[result_handle].result.column == NULL)) {
    return NULL;
  }
  return &query_results[result_handle].result;
}



  /*
   * Issue the SQL query specified by query_string
   * Parameters:
   *   query_string: The SQL query to be issued
   *   reference: Pointer to result handle 
   */
int db_query_table(char *query_string, int *reference)
{
  int status;
  int i;
  MYSQL_RES *result_set;

  status = mysql_real_query(conn, query_string, strlen(query_string));
  if (status != 0) {
    return DB_ERROR;
  }
  result_set = mysql_store_result(conn);
  if (result_set == NULL) {
    return DB_ERROR;
  }
  for (i = 0; i < RESULTS_SIZE; i++) {
    if (query_results[i].result_set == NULL) {
      query_results[i].result_set = result_set;
      query_results[i].result_reference = reference;
      *reference = i;
      return;
    }
  }
  abort();
}
#endif


int db_add_string(char *string_value, int *insert_id, char *outname)
{
   int hpc_string_id, rc;

   if(!string_value)
      return -1;

   sprintf(insert_str, "set %s=(select hpc_string_id from final table (insert into hpc_string "
            "(hpc_string_id, hpc_string_value) "
            "values(default, \'%s\')));",outname,string_value);
   fprintf_input(insert_str);

   /* Add row to hpc_string table */
   if((rc = db_add_row(insert_str, &hpc_string_id)) != 0)
   {
      fprintf(stderr, "Add row to hpc_string table failed\n");
      return -1;
   }

   //mysql_commit(conn);

   if(insert_id)
      *insert_id = hpc_string_id;
   return 0;
}

#if 0
void db_release_results(int result_handle)
{
  if ((result_handle < 0) || (result_handle >= RESULTS_SIZE) ||
      (query_results[result_handle].result_set == NULL)) {
    return;
  }
  mysql_free_result(query_results[result_handle].result_set);
  query_results[result_handle].result_set = NULL;
  *query_results[result_handle].result_reference = -1;
}
#endif


int get_string_id( char *str,char *outname)
{
  db_result *row;
  int id,count;
  int rc;

  if(db_input==0)
	return -1;
/*
  if (string_query_result != -1) {
    db_release_results(string_query_result);
    string_query_result = -1;
  }
*/

//printf("str=%s,outname=%s\n",str,outname);
  if (str == NULL) {
    sprintf(query,"set %s=-1;",outname);
    fprintf_input(query);
    fprintf_undo(query);
    return -1;
  }
//  sprintf(query, "set stringid1=(select hpc_string_id from hpc_string where hpc_string_value = '%s');", str);

  sprintf(query,"set count=(select count(*) from hpc_string where hpc_string_value='%s');",str);
  fprintf_input(query);
  sprintf(query,"if (count=0) then");
  fprintf_input(query);
  db_add_string(str,&id,outname);
  sprintf(query,"else set %s=(select hpc_string_id from hpc_string where hpc_string_value='%s' fetch first 1 rows only);",outname,str);
  fprintf_input(query);
  sprintf(query,"end if;");
  fprintf_input(query);

  sprintf(query,"set %s=(select hpc_string_id from hpc_string where hpc_string_value='%s' fetch first 1 rows only);",outname,str);
  fprintf_undo(query);

  return 0;
/*
  rc=db_query_table(query, &string_query_result);
  if (string_query_result < 0) {
    return -1;
  }


  count=db_get_row_count(string_query_result);

  if (count==0) { // add the new string into the table 
    db_add_string( str, &id);

  }
  
  else {
    row = db_get_row(string_query_result);
    if (row == NULL) {
      db_release_results(string_query_result);
      string_query_result = -1;
      return -1;
    }
    id = atoi(row->column[0]);
    db_release_results(string_query_result);
    string_query_result = -1;
  }
  return id;
*/
}



/* TODO need to check */

#if 0
char *get_string(int id)
{
  db_result *row;
  char *p;

  if (string_query_result != -1) {
    db_release_results(string_query_result);
    string_query_result = -1;
  }
    /*
     * Check if string index indicates no such string
     */
  if (id == -1) {
    return "";
  }
  //sprintf(query, "select hpc_string_value from hpc_string where hpc_string_id = %d", id);
  sprintf(query, "select hpc_string_value from hpc_string where hpc_string_id = stringid1");
  db_query_table(query, &string_query_result);
  if (string_query_result < 0) {
    return "";
  }
  row = db_get_row(string_query_result);
  if (row == NULL) {
    db_release_results(string_query_result);
    string_query_result = -1;
    return "";
  }
  p = strdup(row->column[0]);
  db_release_results(string_query_result);
  string_query_result = -1;
  return p;
}
#endif


  /*
   * Issue SQL query to determine set of data collection runs in database
   * Returns:
   *   Result handle
   *   -1: Error
   */
#if 0 //  TODO NO NEED?
int get_run_id(int jobid)
{
  int count;
  db_result *row;
  int id;

   struct passwd *passwd;
   int loginid;

   if(db_input==0)
	return -1;

   passwd = getpwuid (getuid ());

   loginid=get_string_id(passwd->pw_name);


  if (run_query_result != -1) {
    db_release_results(run_query_result);
    run_query_result = -1;
  }
  sprintf(query,"select hpc_run_id from hpc_run where job_id = %d",jobid);
  db_query_table(query, &run_query_result);
  if (run_query_result < 0) {
    return DB_ERROR;
  }

  count=db_get_row_count(run_query_result);

  if (count==0) { // add the new run id to the table
   
  db_add_run(jobid, &hpc_run_id); 

  }
  
  else { // found
    row = db_get_row(run_query_result);
    if (row == NULL) {
      db_release_results(run_query_result);
      run_query_result = -1;
      return -1;
    }
    id = atoi(row->column[0]);
    hpc_run_id=id;
    db_release_results(run_query_result);
    run_query_result = -1;
  }
  return id;
}
#endif


int db_add_mpi_summary(int func, int callcount, double calltime, double callbytes ,  int *insert_id)
{
   int hpc_row_id;
   int rc;


  sprintf(insert_str,
         "insert into hpc_mpi_summary " 
         "(hpc_row_id, hpc_task_ref, mpi_call_count, mpi_call_time, \
            mpi_call_bytes, mpi_func_label, mpi_parent,job_id) "
            "values(default, taskid, %d, %f, %f, stringid1, 0,%d );",
             callcount, calltime, callbytes, job_id);
   fprintf_input(insert_str);

   sprintf(delete_str, "delete from hpc_mpi_summary where job_id=%d and mpi_call_count=%d and mpi_call_time=%f and mpi_call_bytes=%f and mpi_func_label=stringid1 and mpi_parent=0;", job_id,  callcount, calltime,callbytes);
   fprintf_undo(delete_str);

   if(db_input==0)
	return 0;

   /* Add row to hpc_mpi_summary table */
   if((rc = db_add_row(insert_str, &hpc_row_id)) != 0)
   {
      fprintf(stderr, "Add to hpc_mpi_summary failed\n");
      return -1;
   }


   if(insert_id)
      *insert_id = hpc_row_id;


   return 0;
}


int db_add_mpi_callsite(char *filename, char *funcname, int lineno, long long eventcount, float totaltime, float totalbytes, long long parent, int *insert_id)
{
   int hpc_region_id, hpc_row_id;
   int rc;
   int parent_string_id, func_string_id;



   hpc_region_id=get_region_id(filename,funcname,lineno,lineno);
   func_string_id=get_string_id(funcname,"stringid1");


   sprintf(insert_str,
         "insert into hpc_mpi_callsite " 
         "(hpc_row_id, hpc_task_ref, hpc_region_id, mpi_call_count, \
            mpi_call_time, mpi_call_bytes, mpi_func_label, mpi_parent,job_id) "
            "values(default, taskid, regionid, %lld, %f, %f, stringid1, %d,%d );",
              eventcount, totaltime, totalbytes, parent,job_id);
   fprintf_input(insert_str);

   sprintf(delete_str, "delete from hpc_mpi_callsite where job_id=%d and hpc_region_id=regionid and mpi_call_count=%lld and mpi_call_time=%f and mpi_call_bytes=%f and mpi_func_label=stringid1 and mpi_parent=%d;", job_id, eventcount, totaltime, totalbytes,  parent);
   fprintf_undo(delete_str);

   if(db_input==0)
	return 0;

   /* Add row to hpc_mpi_callsite table */
   if((rc = db_add_row(insert_str, &hpc_row_id)) != 0)
   {
      fprintf(stderr, "Add to hpc_mpi_callsite failed:[%s]\n",insert_str);
      return -1;
   }


   if(insert_id)
      *insert_id = hpc_row_id;

   //mysql_commit(conn);
    return 0;
}



int db_add_mpi_trace(struct eventstruct *event, int func, int *insert_id)
{
   int hpc_row_id; 

   if(event == NULL)
      return -1;

   sprintf(insert_str, 
         "insert into hpc_mpi_trace (hpc_row_id, hpc_task_ref, mpi_call_id, mpi_start_time, mpi_stop_time, mpi_call_parent, mpi_call_grandparent, mpi_source_task, mpi_dest_task, mpi_byte_count, mpi_func_label,job_id) values(default, taskid, %d, %f, %f, %lld, %lld, %d, %d, %d, %d, %d);",
             event->eventid, event->tbeg,
            event->tend, event->parent, event->grandparent, event->src, 
            event->dest, event->bytes,func,job_id);
   fprintf_input(insert_str);

   sprintf(delete_str, "delete from hpc_mpi_trace where job_id=%d and mpi_call_id=%d and mpi_start_time=%f and mpi_stop_time=%f and mpi_call_parent=%lld and mpi_call_grandparent=%lld and mpi_source_task=%d and mpi_dest_task=%d and mpi_byte_count=%d and mpi_func_label=%d;", job_id, event->eventid, event->tbeg, event->tend, event->parent, event->grandparent, event->src, event->dest, event->bytes,func);
   fprintf_undo(delete_str);

   if(db_input==0)
	return 0;

   /* Add row to hpc_mpi_trace table */
   if(db_add_row(insert_str, &hpc_row_id) != 0)
   {
      fprintf(stderr, "Add to hpc_mpi_trace failed\n");
      return -1;
   }

   if(insert_id)
      *insert_id = hpc_row_id;
   return 0;
}

#if 0
int db_add_dumpi_trace(char *path, char *filename, int *insert_id)
{

    int hpc_row_id;

   if(filename == NULL)
      return -1;

  // TODO need to check if the file exists

   sprintf(insert_str,
         "insert into hpc_dumpi_trace (hpc_row_id, job_id, hpc_task_ref, filename, tracefile) values (default, %d, taskid, '%s', LOAD_FILE('%s/%s'))",
         job_id,  filename, path, filename);

   fprintf_input(insert_str);

   sprintf(delete_str, "delete from hpc_dumpi_trace where job_id=%d and filename='%s'", job_id, filename);
   fprintf_undo(delete_str);

   if(db_input==0)
        return 0;

   /* Add row to hpc_dumpi_trace table */
   if(db_add_row(insert_str, &hpc_row_id) != 0)
   {
      fprintf(stderr, "Add to hpc_dumpi_trace failed\n");
      return -1;
   }

   if(insert_id)
       *insert_id = hpc_row_id;
   return 0;
}
#endif





int db_add_hpm_counter(int regionid, int group, double time, int callcount, int *insert_id)
{

  int hpc_row_id;
  int rc;


   sprintf(insert_str, "set hpmref= (select hpc_row_id from final table (insert into hpc_hpm_counter " 
           "(hpc_row_id, hpc_task_ref, hpc_region_id, hpm_counter_group, hpm_exec_time, hpm_call_count,job_id) "
            "values(default, taskid, regionid, %d, %.0f, %d, %d)) order by hpc_row_id desc fetch first 1 rows only);",
              group, time, callcount, job_id);
   fprintf_input(insert_str);


   sprintf(delete_str, "delete from hpc_hpm_counter where job_id=%d and hpc_region_id=regionid and hpm_counter_group=%d and hpm_exec_time=%.0f and hpm_call_count=%d;", job_id,  group, time, callcount);
   fprintf_undo(delete_str);

   if(db_input==0)
	return 0;



   if((rc = db_add_row(insert_str, &hpc_row_id)) != 0)
   {

      fprintf(stderr, "Add to hpc_hpm_counter table failed\n");
      return -1;
   }


   if(insert_id)
      *insert_id = hpc_row_id;
   return 0;
}

int db_add_hpm_counter_data(int hpm_ref, char * label,  long long  value, int *insert_id) {

   int hpc_row_id;
   int string_id;
   int rc;

   string_id=get_string_id(label,"stringid1");

   sprintf(insert_str, "insert into hpc_hpm_counter_data "
            "(hpc_data_sequence, hpc_hpm_ref, hpc_data_id, hpc_data_value, job_id) "
            "values(default, hpmref, stringid1,  %lld, %d);",
             value, job_id);
   fprintf_input(insert_str);


   sprintf(delete_str, "delete from hpc_hpm_counter_data where job_id=%d and hpc_data_id=stringid1 and hpc_data_value=%lld;", job_id,  value);
   fprintf_undo(delete_str);

   if(db_input==0)
	return 0;

   if((rc = db_add_row(insert_str, &hpc_row_id)) != 0)
   {
      fprintf(stderr, "Add to hpc_hpm_counter_data table failed\n");
      return -1;
   }

   if(insert_id)
      *insert_id = hpc_row_id;
   return 0;
}

int db_add_hpm_derived_data(int hpm_ref, char * label,  double value, int *insert_id) {

   int hpc_row_id;
   int string_id;
   int rc;

   if(db_input==0)
	return 0;

   string_id=get_string_id(label,"stringid1");


      sprintf(insert_str, "insert into hpc_hpm_derived_data "
            "(hpc_data_sequence, hpc_hpm_ref, hpc_data_id, hpc_float_value,job_id) "
            "values(default, hpmref, stringid1,  %f, %d);",
             value, job_id);


    fprintf_input(insert_str);


    sprintf(delete_str, "delete from hpc_hpm_derived_data where job_id=%d and hpc_data_id=stringid1 and hpc_float_value=%f;", job_id,  value);
    fprintf_undo(delete_str);

    if(db_input==0)
	return 0;

   if((rc = db_add_row(insert_str, &hpc_row_id)) != 0)
   {
      fprintf(stderr, "Add to hpc_hpm_derived_data table failed\n");
      return -1;
   }


   if(insert_id)
      *insert_id = hpc_row_id;
   return 0;
}

//TODO omp OpenMP openmp 

/* OMP */ // TODO need to check

int db_add_omp_summary(struct omp_summary_rec *rec, int *insert_id)
{
   int hpc_row_id;
   int rc;

   if(rec == NULL)
      return -1;
   

  sprintf(insert_str,
         "insert into hpc_omp_summary " 
         "(hpc_row_id, hpc_task_ref, hpc_region_id, omp_count, omp_exclusive_time, \
            omp_inclusive_time, omp_overhead, omp_master_time, omp_thread_time, \
            omp_compute_time, omp_imbalance_pct, omp_avg_thread_time, omp_to_time, \
            omp_to_barrier_time, omp_to_rtl_time, omp_region_label, job_id) "
            "values(default, %d, %d, %d, %f, %f, %f, %f, %f, %f, %f, %f, %f, %f, %f, %d, %d );",
            rec->task_id, rec->region_label, rec->count, rec->exclusive_time, rec->inclusive_time, 
            rec->overhead, rec->master_time, rec->thread_time, rec->compute_time, 
            rec->imbalance_pct, rec->avg_thread_time, rec->to_time,
            rec->to_barrier_time, rec->to_rtl_time, -1, job_id);

   fprintf_input(insert_str);

   sprintf(delete_str, "delete from hpc_omp_summary where job_id=%d and hpc_region_id=%d and omp_count=%d and omp_exclusive_time=%f and omp_inclusive_time=%f and omp_overhead=%f and omp_master_time=%f and omp_thread_time=%f and omp_compute_time=%f and omp_imbalance_pct=%f and omp_avg_thread_time=%f and omp_to_time=%f and omp_to_barrier_time=%f and omp_to_rtl_time=%f and omp_region_label=%d;", job_id, rec->region_label, rec->count, rec->exclusive_time, rec->inclusive_time, rec->overhead, rec->master_time, rec->thread_time, rec->compute_time, rec->imbalance_pct, rec->avg_thread_time, rec->to_time, rec->to_barrier_time, rec->to_rtl_time, -1);
   fprintf_undo(delete_str);

   if(db_input==0)
	return 0;

   /* Add row to hpc_omp_summary table */
   if((rc = db_add_row(insert_str, &hpc_row_id)) != 0)
   {
      fprintf(stderr, "Add to hpc_omp_summary table failed\n");
      return -1;
   }


   if(insert_id)
      *insert_id = hpc_row_id;
   return 0;
}




