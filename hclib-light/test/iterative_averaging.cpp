#include<iostream>
#include<algorithm>
#include<stdio.h>
#include<string.h>
#include<cmath>
#include<sys/time.h>

#include "hclib.hpp"

/*
 * Ported from HJlib
 *
 * Author: Vivek Kumar
 *
 */

#define SIZE 25165824
#define ITERATIONS 64
#define THRESHOLD 4096//1024//2048//786432//1572864//3145728//786432//3145728//2048

// double* myNew, *myVal;
// double* myNew_s, *myVal_s;
double* myNew, *myVal, *initialOutput;
int n;

long get_usecs () {
  struct timeval t;
  gettimeofday(&t,NULL);
  return t.tv_sec*1000000+t.tv_usec;
}

void recurse(uint64_t low, uint64_t high) {
  if((high - low) > THRESHOLD) {
    uint64_t mid = (high+low)/2;
    hclib::finish([=]() {
        hclib::async([=]( ){
	   /* An async task */
	   recurse(low, mid);
	});
        recurse(mid, high);
    });

  } else {
    for(uint64_t j=low; j<high; j++) {
      myNew[j] = (myVal[j - 1] + myVal[j + 1]) / 2.0;
    }
  }
}

void runParallel() {
  for(int i=0; i<ITERATIONS; i++) {
    #ifdef USE_TRACE_AND_REPLAY
      printf("============================================\n");
      printf("Iteration%d\n", i+1);
      printf("============================================\n");
      hclib::start_tracing();
      recurse(1, SIZE+1);
      double* temp = myNew;
      myNew = myVal;
      myVal = temp;
      hclib::stop_tracing();
    #else
      recurse(1, SIZE+1);
      double* temp = myNew;
      myNew = myVal;
      myVal = temp;
    #endif
  }
}

void validateOutput() {
  for (int i = 0; i < SIZE + 2; i++) {
    double init = initialOutput[i];
    double curr = myVal[i];
    double diff = std::abs(init - curr);
    if (diff > 1e-20) {
      printf("ERROR: validation failed!\n");
      printf("Diff: myVal[%d]=%.3f != initialOutput[%d]=%.3f",i,curr,i,init);
      break;
    }
  }
  printf("after validation\n");
}

void runSequential() {
  for (int iter = 0; iter < ITERATIONS; iter++) {
    for(int j=1; j<=SIZE; j++) {
        myNew[j] = (initialOutput[j - 1] + initialOutput[j + 1]) / 2.0;
    }
    double* temp = myNew;
    myNew = initialOutput;
    initialOutput = temp;
  }
}

int main(int argc, char** argv) {

  //initialize runtime
  hclib::init(argc, argv);

  myNew = new double[(SIZE + 2)];
  myVal = new double[(SIZE + 2)];
  initialOutput = new double[(SIZE + 2)];

  memset(myNew, 0, sizeof(double) * (SIZE + 2));
  memset(myVal, 0, sizeof(double) * (SIZE + 2));
  memset(initialOutput, 0, sizeof(double) * (SIZE + 2));

  initialOutput[SIZE + 1] = 1.0;
  myVal[SIZE + 1] = 1.0;

#ifdef VERIFY
  runSequential();
#endif

  long start = get_usecs();
  runParallel();
  long end = get_usecs();
  double dur = ((double)(end-start))/1000000;

#ifdef VERIFY
  validateOutput();
#endif

  printf("Time = %.3f\n",dur);
  delete(myNew);
  delete(myVal);
  delete(initialOutput);

  //finalize runtime
  hclib::finalize();

}

