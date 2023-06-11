/*
* README

  This code is based on aca_ch2_cs2.c used in case study 2 and modified so that it can work with Vtune out of the box.
  Since the original code aca_ch2_cs2.c used in case study 2 is designed to run on Microsoft Visual C++. 
  This code also inherits the feature to run on on Microsoft Visual C++. And Microsoft Visual Studio is 
  preferred for easy management/compilation of the code and the supported easy integration of Vtune. 
  
  1. Usage of this program:
     This program accepts two  command line inputs: <input data set (Byte)> and <stride (Byte)>.
	 E.g. aca_ch2_cs2_vtune 32768 64 

  2. Setting up Vtune
     Use a WIN32 project. 
     A free evaluation version of Vtune can be downloaded from http://software.intel.com/en-us/articles/intel-vtune-amplifier-xe/  
	 Install Vtune (a pre-installed Visual Studio is  preferred for easy management and compilation of the code)
	 Briefly go over the tutorial and set up Vtune following the instructions in "Intel® VTune™ Amplifier XE 2011 Getting Started Tutorials for Windows* OS" 
	 (Tutorial can be downloaded from http://software.intel.com/sites/products/documentation/hpc/amplifierxe/en-us/win/start/getting_started_amplifier_xe_windows.pdf )
     Important Vtune setup from Intel tutorial: The build configuration may initially be set to Debug, which is typically used for development. 
	 When analyzing performance with the VTune, Release build with normal optimizations should be used, so that VTune can analyze the realistic performance.

  3. Enable special Vtune function
     Special Vtune functions have been inserted to exclude initialization and loop overhead during the performance analysis process. 
	 The code works with Vtune without tuning on these functions, but Vtune will analyze the whole program including the initialization and loop overhead 
	 if the functions are not enabled.

	 These function need to be enabled by using compiler/linker flags: -DVTUNE, -I<vtune install_dir>\include, 
	 -L<vtune install_dir>\include, and -littnotify
	 a. In visual studio -DVTUNE can be done by adding the key word of VTUNE
		to project->properties-> Configuration properties ->C/C++ ->preprocessor ->preprocessor definitions.
	 b. In visual studio -I<vtune install_dir>\include can be done by adding <vtune install_dir>\include, 
	    to project->properties-> Configuration properties -> general ->Additional Include Directories. 
	 c. In visual studio -L<vtune install_dir>\include can be done by adding <vtune install_dir>\include, 
	    to project->properties-> Configuration properties -> general ->Additional Include Directories.
     d. In visual studio -littnotify can be done by adding libittnotify.lib to project->properties-> 
	    Configuration properties -> Additional Dependencies.

   4. Analyze the program using Vtune
      The default Vtune analysis set is not sufficient for the experiments, please create a new analysis set.
	  a. Create a new analysis set: Go to Vtune->New analysis -->New hardware Event-based Sampling Analysis
	  b. Including the following 4 groups of performance counters:
         Total instructions and cycles: INST_RETIRED.ANY by Package	INST_RETIRED.TOTAL_CYCLES by Package	
		 L1 statistics: MEM_INST_RETIRED.LOADS by Package MEM_INST_RETIRED.STORES by Package	MEM_LOAD_RETIRED.L1D_HIT by Package
		 Number of L2 Misses: L2_RQSTS.LD_MISS by Package
		 Number of L3 Misses: LONGEST_LAT_CACHE.MISS by Package	
	  c. Collect the statistics on these performance counters from Vtune to finish the exercises

    Author: Sheng Li sheng.li4@hp.com 
	Date:   May 2011
*/

//#include "stdafx.h"
#include <Windows.h>   
#include <errno.h>   
#include <tchar.h> 
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#ifdef VTUNE
#include <ittnotify.h>
#endif
#define ARRAY_MIN (1024) /* 1/4 smallest cache */
#define ARRAY_MAX (8192*8192) /* 1/4 largest cache */
int x[ARRAY_MAX]; /* array going to stride through, single array for all input size */

double get_seconds() { /* routine to read time in seconds */
	__time64_t ltime;
	_time64( &ltime );
	return (double) ltime;
}
int label(int i) {/* generate text labels */
	if (i<1e3) printf("%1dB,",i);
	else if (i<1e6) printf("%1dK,",i/1024);
	else if (i<1e9) printf("%1dM,",i/1048576);
	else printf("%1dG,",i/1073741824);
	return 0;
}

void print_usage(const _TCHAR * argv0)
{
    printf("How to use this program:\n");
    printf("./aca_ch2_cs2_vtune <input data set (Byte)>  < stride (Byte) >\n");
	printf("E.g. %s 32768 64 \n\n", argv0);
	printf("Press any key to exit \n ");
	getchar(); 
    exit(1);
}
int _tmain(int argc, _TCHAR* argv[]) {
    
	int register nextstep, i, index, stride;
	int csize;
	double steps, tsteps;
	double loadtime, lastsec, sec0, sec1, sec; /* timing variables */
	int input_size, stride_start, stride_end;

	#ifdef VTUNE
    __itt_pause();
    #endif

	if (argc != 3)
		print_usage(argv[0]);
	else{

	/* Initialize output */

	input_size   = _ttoi(argv[1])/sizeof(int);
    stride_start = _ttoi(argv[2])/sizeof(int);
	stride_end   = stride_start; //_ttoi(argv[ 3]);
	printf("Input Dataset Size = ");
	label(input_size*sizeof(int));
	printf("   Stride = %1d Byte,\n",stride_start*sizeof(int));

	/* Main loop for each configuration */
	for (csize=input_size; csize <= input_size; csize=csize*2) { //input size (csize) from MIN to MAX
		label(csize*sizeof(int)); /* print cache size this loop */
        for (stride=stride_start; stride <= stride_end; stride=stride*2) {
			/* Lay out path of memory references in array */
			for (index=0; index < csize; index=index+stride)
				x[index] = index + stride; /* pointer to next */
			x[index-stride] = 0; /* loop back to beginning */

			/* Wait for timer to roll over */
			lastsec = get_seconds();
			do sec0 = get_seconds(); while (sec0 == lastsec);

			/* Walk through path in array for twenty seconds */
			/* This gives 5% accuracy with second resolution */
			steps = 0.0; /* number of steps taken */
			nextstep = 0; /* start at beginning of path */
			sec0 = get_seconds(); /* start timer */
            
            #ifdef VTUNE
			__itt_resume();
            #endif

			do { /* repeat until collect 20 seconds; this 20 second loop needs to be done for each stride value as in line 38, this is why the time is so long */
				for (i=stride;i!=0;i=i-1) { /* keep samples same */
					nextstep = 0;
					do {
						nextstep = x[nextstep]; /* dependency, continue walk through the array using the stide set @line 41 */
					}
					while (nextstep != 0);
				}
				steps = steps + 1.0; /* count loop iterations */
				sec1 = get_seconds(); /* end timer */
			} while ((sec1 - sec0) < 20.0); /* collect 20 seconds */
            #ifdef VTUNE
			__itt_pause();
            #endif
			sec = sec1 - sec0;

			/* Repeat empty loop to loop subtract overhead; the overhead is the loop control flow */
			tsteps = 0.0; /* used to match no. while iterations */
			sec0 = get_seconds(); /* start timer */
			do { /* repeat until same no. iterations as above */
				for (i=stride;i!=0;i=i-1) { /* keep samples same */
					index = 0;
					do index = index + stride; //fake operation to subtract loop control flow overhead; so no real stride into array
					while (index < csize);
				}
				tsteps = tsteps + 1.0;
				sec1 = get_seconds(); /* - overhead */
			} while (tsteps<steps); /* until = no. iterations */
			sec = sec - (sec1 - sec0);
			loadtime = (sec*1e9)/(steps*csize);
			/* write out results of real time spend cache stress test as in line 57 in .csv format for Excel */
			printf("%4.1f,", (loadtime<0.1) ? 0.1 : loadtime);
		}; /* end of inner for loop */
		printf("\n");
	}; /* end of outer for loop */
	return 0;
	}
}


