#include <petscsys.h>        /*I  "petscsys.h"   I*/
#include <../src/sys/objects/pthread/pthreadimpl.h>

PetscInt *pVal_none;

typedef void* (*pfunc)(void*);
typedef struct {
  pfunc* funcArr;
  PetscInt   nthreads;
  void** pdata;
  pthread_t* ThreadId;
}sjob_none;

sjob_none job_none;

/* 
   -----------------------------
     'NO' THREAD POOL FUNCTION 
   -----------------------------
*/
void* PetscThreadFunc_None(void* arg)
{
  PetscInt iVal;

  iVal = *(PetscInt*)arg;
  pthread_setspecific(rankkey,&threadranks[iVal+1]);

#if defined(PETSC_HAVE_SCHED_CPU_SET_T)
  DoCoreAffinity();
#endif

  job_none.funcArr[iVal+PetscMainThreadShareWork](job_none.pdata[iVal+PetscMainThreadShareWork]);

  pthread_setspecific(rankkey,NULL);
  return NULL;
}
  
void* PetscThreadsWait_None(void* arg)
{
  PetscInt            nthreads;
  PetscInt       i;
  void*          joinstatus;

  nthreads = *(PetscInt*)arg;
  for (i=0; i<nthreads; i++) {
    pthread_join(PetscThreadPoint[i], &joinstatus);
  }
  return(0);
}

#undef __FUNCT__
#define __FUNCT__ "PetscThreadsRunKernel_None"
PetscErrorCode PetscThreadsRunKernel_None(void* (*pFunc)(void*),void** data,PetscInt n,PetscInt* cpu_affinity) 
{
  PetscInt i;
  PetscInt Nnew_threads=n-PetscMainThreadShareWork;
  PetscErrorCode ierr;

  PetscFunctionBegin;
  ierr = PetscMalloc(Nnew_threads*sizeof(PetscInt),&pVal_none);CHKERRQ(ierr);
  ierr = PetscMalloc(Nnew_threads*sizeof(pthread_t),&PetscThreadPoint);CHKERRQ(ierr);
  ierr = PetscMalloc(n*sizeof(pfunc),&(job_none.funcArr));CHKERRQ(ierr);
  ierr = PetscMalloc(n*sizeof(void*),&(job_none.pdata));CHKERRQ(ierr);

  threadranks[0] = 0;
  pthread_setspecific(rankkey,&threadranks[0]);
  for(i=0;i< Nnew_threads;i++) {
    pVal_none[i] = i;
    threadranks[i+1] = i+1;
    ThreadCoreAffinity[i] = cpu_affinity[i+PetscMainThreadShareWork];
    job_none.funcArr[i+PetscMainThreadShareWork] = pFunc;
    job_none.pdata[i+PetscMainThreadShareWork]  = data[i+PetscMainThreadShareWork];
    pthread_create(&PetscThreadPoint[i],NULL,PetscThreadFunc_None,&pVal_none[i]);
  }
  if(PetscMainThreadShareWork) pFunc(data[0]);

  PetscThreadsWait(&Nnew_threads);

  ierr = PetscFree(PetscThreadPoint);CHKERRQ(ierr);
  ierr = PetscFree(job_none.funcArr);CHKERRQ(ierr);
  ierr = PetscFree(job_none.pdata);CHKERRQ(ierr);
  ierr = PetscFree(pVal_none);CHKERRQ(ierr);

  PetscFunctionReturn(0);
}
