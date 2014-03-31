/* -*- mode: C; mode: folding; fill-column: 70; -*- */
/* Copyright 2010,  Georgia Institute of Technology, USA. */
/* See COPYING for license. */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

/* getopt should be in unistd.h */
#if HAVE_UNISTD_H
#include <unistd.h>
#else
#if !defined(__MTA__)
#include <getopt.h>
#endif
#endif
#include "options.hpp"
#include "utility.hpp"

using namespace std;

void get_options (int argc, char **argv, problemctx *pcfg, clusterctx *ctx, experiments *expt){ 

  extern char *optarg;
  int c, err = 0;

  strads_msg(INF, "get options is called\n");  
  while ((c = getopt (argc, argv, "V?hd:o:m:t:c:s:p:f:u:v:r:a:b:e:g:i:")) != -1)
    
    switch (c) {

    case 'h':

    case '?':
      printf ("Usuage: exetuable -dX.txt -oY.txt\n");      
      exit (EXIT_SUCCESS);
      break;

      // common options for apps that use typical X matrix and Y as input data       
      // If you do not need them, just fill them with NULL
      // and define your data structures in expt data structure in userdefined.hpp 
      // TODO: later move this to userdefined.hpp 
    case 'd':
      pcfg->xfile  = strdup (optarg);
      if (!pcfg->xfile) {
	fprintf (stderr, "can not copy x file name.\n");
	err = 1;
      }
      break;

    case 'o':
      pcfg->yfile  = strdup (optarg);
      if (!pcfg->yfile) {
	fprintf (stderr, "can not copy y file name\n");
	err = 1;
      }
      break;

    case 'u':
      pcfg->stdflag = strtol (optarg, NULL, 10);
      if (!(pcfg->stdflag == 0 || pcfg->stdflag == 1)) {
	fprintf (stderr, "-u standardization flag should be 0 or 1\n");
	err = 1;
      }
      break;

    case 'v':
      pcfg->cmflag = strtol (optarg, NULL, 10);
      if (!(pcfg->cmflag == 0 || pcfg->cmflag == 1)) {
	fprintf (stderr, "-v make cm (column majored one) should be 0 or 1\n");
	err = 1;
      }
      break;


    case 'g':
      expt->oocpartitions = strtol (optarg, NULL, 10);
      if (expt->oocpartitions < 0) {
	fprintf (stderr, "-g OOC partitions should be >= 1 (1: no ooc)\n");
	err = 1;
      }
      break;


    case 'i':
      expt->init_sweepoutrate = strtod (optarg, NULL);
      if (expt->init_sweepoutrate < 0) {
	fprintf (stderr, "-i init_sweepoutrate >= 1.2 \n");
	err = 1;
      }
      break;


      // Common configurations for machine use 
      // specify the number of worker machines, the number of threads per w machine 
      //         the number of sched machines, the number of threads per sched machine
      //         the number of app sched machine 
    case 'm':
      ctx->wmachines  = strtol (optarg, NULL, 10);
      if (ctx->wmachines < 1) {
	fprintf (stderr, "machine must be larger than 1\n");
	err = 1;
      }
      break;

    case 't':
      ctx->worker_per_mach  = strtol (optarg, NULL, 10);
      if (ctx->worker_per_mach < 1) {
	fprintf (stderr, "thread per machine must be larger than 1\n");
	err = 1;
      }
      break;


    case 'a':
      pcfg->parameters  = strtol (optarg, NULL, 10);
      if (pcfg->parameters < 1) {
	fprintf (stderr, "# of parameters must be larger than 1\n");
	err = 1;
      }
      break;


    case 'r':
      expt->desired = strtol (optarg, NULL, 10);
      if (expt->desired < 1) {
	fprintf (stderr, "# of parameters must be larger than 1\n");
	err = 1;
      }
      break;


    case 'b':
      expt->afterdesired = strtol (optarg, NULL, 10);
      if (expt->afterdesired < 1) {
	fprintf (stderr, "# of parameters must be larger than 1\n");
	err = 1;
      }
      break;


      
    case 's':
      ctx->schedmachines  = strtol (optarg, NULL, 10);

      if (ctx->schedmachines < 1) {
	fprintf (stderr, "scheduler machin must be equal to or larger than 1\n");
	err = 1;
      }
      break;


    case 'e':
      expt->oocfreq  = strtol (optarg, NULL, 10);

      if (expt->oocfreq < 1) {
	fprintf (stderr, "expt->oocfreq must be equal to or larger than 1\n");
	err = 1;
      }
      break;

    case 'p':
      ctx->schedthrd_per_mach  = strtol (optarg, NULL, 10);
      if (ctx->schedthrd_per_mach < 1) {
	fprintf (stderr, "thread per scheduler machine must be equal to or larger than 1\n");
	err = 1;
      }
      break;

      // you can define your own parameters in expt data structure in userdefined.hpp in your app directoy. 
      // If so, your app code should be able to parse the conf file and fill out the expt data structure 
      // by parsing the conf file. 
    case 'c':
      expt->userconfigfile = strdup (optarg);
      if (!expt->userconfigfile) {
	fprintf (stderr, "can not copy user configuration file name\n");
	err = 1;
      }
      break;
   
      // IP addresss of all machines you are going to run 
      // the number of entry should be equal to the machine configuration as above. 
      // otherwise, unexpected behavior will happen or asserted.      
    case 'f':
      ctx->ipfile = (char *)calloc(strlen(optarg)+10, sizeof(char));
      if (ctx->ipfile == NULL) {
	fprintf (stderr, "fatal: memory alloc for ipfile parameter failed\n");
	err = 1;
      }
      strcpy(ctx->ipfile, optarg);
      break;

    default:
      fprintf (stderr, "Unrecognized option [%c]\n", c);
      err = -1;
    }

  // don't touch below 
  ctx->totalschedthrd = ctx->schedmachines*ctx->schedthrd_per_mach;
  ctx->totalworkers =   ctx->wmachines*ctx->worker_per_mach;
  ctx->totalmachines = ctx->schedmachines + ctx->wmachines;
  ctx->numainfo = NULL;

  if(err)
    exit (EXIT_FAILURE);
}
