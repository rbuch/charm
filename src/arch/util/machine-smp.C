/** @file
 * @brief Common function reimplementation for SMP machines
 * @ingroup Machine
 *
 * OS Threads
 *
 * This version of converse is for multiple-processor workstations,
 * and we assume that the OS provides threads to gain access to those
 * multiple processors.  This section contains an interface layer for
 * the OS specific threads package.  It contains routines to start
 * the threads, routines to access their thread-specific state, and
 * routines to control mutual exclusion between them.
 *
 * In addition, we wish to support nonthreaded operation.  To do this,
 * we provide a version of these functions that uses the main/only thread
 * as a single PE, and simulates a communication thread using interrupts.
 *
 *
 * CmiStartThreads()
 *
 *    Allocates one CmiState structure per PE.  Initializes all of
 *    the CmiState structures using the function CmiStateInit.
 *    Starts processor threads 1..N (not 0, that's the one
 *    that calls CmiStartThreads), as well as the communication
 *    thread.  Each processor thread (other than 0) must call ConverseInitPE
 *    followed by Cmi_startfn.  The communication thread must be an infinite
 *    loop that calls the function CommunicationServer over and over.
 *
 * CmiGetState()
 *
 *    When called by a PE-thread, returns the processor-specific state
 *    structure for that PE.
 *
 * CmiGetStateN(int n)
 *
 *    returns processor-specific state structure for the PE of rank n.
 *
 * CmiMemLock() and CmiMemUnlock()
 *
 *    The memory module calls these functions to obtain mutual exclusion
 *    in the memory routines, and to keep interrupts from reentering malloc.
 *
 * CmiCommLock() and CmiCommUnlock()
 *
 *    These functions lock a mutex that insures mutual exclusion in the
 *    communication routines.
 *
 * CmiMyPe() and CmiMyRank()
 *
 *    The usual.  Implemented here, since a highly-optimized version
 *    is possible in the nonthreaded case.
 *

  
  FIXME: There is horrible duplication of code (e.g. locking code)
   both here and in converse.h.  It could be much shorter.  OSL 9/9/2000

 *****************************************************************************/

/**
 * \addtogroup Machine
 * @{
 */

/*
for SMP versions:

CmiStateInit
CmiNodeStateInit
CmiGetState
CmiGetStateN
CmiYield
CmiStartThreads

CmiIdleLock_init
CmiIdleLock_sleep
CmiIdleLock_addMessage
CmiIdleLock_checkMessage
*/

#include "machine-smp.h"
#include "sockRoutines.h"

#if CMK_AMD64
#  include <immintrin.h>
#endif
#include <atomic>
#include <thread>

void CmiStateInit(int pe, int rank, CmiState state);
void CommunicationServerInit(void);

static struct CmiStateStruct Cmi_default_state; /* State structure to return during startup */

/************************ Win32 kernel SMP threads **************/

#if CMK_SHARED_VARS_NT_THREADS

CmiNodeLock CmiMemLock_lock; // used by CmiMemLock during heap interception (charmc -memory)
CmiNodeLock cmiMemoryLock; // used by CmiMemoryAtomic*/ReadFence/WriteFence and CMK_PCQUEUE_LOCK
static CmiNodeLock comm_mutex;
#define CmiCommLockOrElse(x) /*empty*/
#define CmiCommLock() (CmiLock(comm_mutex))
#define CmiCommUnlock() (CmiUnlock(comm_mutex))

static DWORD Cmi_state_key = 0xFFFFFFFF;
static CmiState     Cmi_state_vector = 0;

#if 0
#  define CmiGetState() ((CmiState)TlsGetValue(Cmi_state_key))
#else
CmiState CmiGetState(void)
{
  CmiState result;
  result = (CmiState)TlsGetValue(Cmi_state_key);
  if(result == 0) {
  	return &Cmi_default_state;
  	/* PerrorExit("CmiGetState: TlsGetValue");*/
  }
  return result;
}
#endif

void CmiYield(void) 
{ 
  Sleep(0);
}

#define CmiGetStateN(n) (Cmi_state_vector+(n))

extern std::atomic<int> _cleanUp;
void StartInteropScheduler(void);
void CommunicationServerThread(int sleepTime);

/*
static DWORD WINAPI comm_thread(LPVOID dummy)
{  
  if (Cmi_charmrun_fd!=-1)
    while (1) CommunicationServerThread(5);
  return 0;
}

static DWORD WINAPI call_startfn(LPVOID vindex)
{
  int index = (int)vindex;
 
  CmiState state = Cmi_state_vector + index;
  if(Cmi_state_key == 0xFFFFFFFF) PerrorExit("TlsAlloc");
  if(TlsSetValue(Cmi_state_key, (LPVOID)state) == 0) PerrorExit("TlsSetValue");

  ConverseRunPE(0);
  return 0;
}
*/

static DWORD WINAPI call_startfn(LPVOID vindex)
{
  int index = (int)(intptr_t)vindex;
 
  CmiState state = Cmi_state_vector + index;
  if(Cmi_state_key == 0xFFFFFFFF) PerrorExit("TlsAlloc");
  if(TlsSetValue(Cmi_state_key, (LPVOID)state) == 0) PerrorExit("TlsSetValue");

  ConverseRunPE(0);

  if(CharmLibInterOperate) {
    while(!_cleanUp.load()) {
      StartInteropScheduler();
      CmiNodeAllBarrier();
    }

    if (CmiMyRank() == CmiMyNodeSize()) {
      while (ckExitComplete.load() == 0) { CommunicationServerThread(5); }
    } else {
      CsdScheduler(-1);
      CmiNodeAllBarrier();
    }
  }

#if 0
  if (index<_Cmi_mynodesize)
	  ConverseRunPE(0); /*Regular worker thread*/
  else { /*Communication thread*/
	  CommunicationServerInit();
	  if (Cmi_charmrun_fd!=-1)
		  while (1) CommunicationServerThread(5);
  } 
#endif
  return 0;
}

static void CmiStartThreads(char **argv)
{
  int     i,tocreate;
  DWORD   threadID;
  HANDLE  thr;

  CmiMemLock_lock=CmiCreateLock();
  comm_mutex = CmiCreateLock();
  cmiMemoryLock = CmiCreateLock();

  Cmi_state_key = TlsAlloc();
  if(Cmi_state_key == 0xFFFFFFFF) PerrorExit("TlsAlloc main");
  
  Cmi_state_vector =
    (CmiState)calloc(_Cmi_mynodesize+1, sizeof(struct CmiStateStruct));
  
  for (i=0; i<_Cmi_mynodesize; i++)
    CmiStateInit(i+Cmi_nodestart, i, CmiGetStateN(i));
  /*Create a fake state structure for the comm. thread*/
/*  CmiStateInit(-1,_Cmi_mynodesize,CmiGetStateN(_Cmi_mynodesize)); */
  CmiStateInit(_Cmi_mynode+CmiNumPes(),_Cmi_mynodesize,CmiGetStateN(_Cmi_mynodesize));
  
#if CMK_MULTICORE || CMK_SMP_NO_COMMTHD
  if (!Cmi_commthread)
    tocreate = _Cmi_mynodesize-1;
  else
#endif
  tocreate = _Cmi_mynodesize;
  for (i=1; i<=tocreate; i++) {
    if((thr = CreateThread(NULL, 0, call_startfn, (LPVOID)(intptr_t)i, 0, &threadID)) 
       == NULL) PerrorExit("CreateThread");
    CloseHandle(thr);
  }
  
  if(TlsSetValue(Cmi_state_key, (LPVOID)Cmi_state_vector) == 0) 
    PerrorExit("TlsSetValue");
}

static void CmiDestroyLocks(void)
{
  CmiDestroyLock(comm_mutex);
  comm_mutex = 0;
  CmiDestroyLock(CmiMemLock_lock);
  CmiMemLock_lock = 0;
  CmiDestroyLock(cmiMemoryLock);
}

/***************** Pthreads kernel SMP threads ******************/
#elif CMK_SHARED_VARS_POSIX_THREADS_SMP

CmiNodeLock CmiMemLock_lock; // used by CmiMemLock during heap interception (charmc -memory)
CmiNodeLock cmiMemoryLock; // used by CmiMemoryAtomic*/ReadFence/WriteFence and CMK_PCQUEUE_LOCK
int _Cmi_sleepOnIdle=0;
int _Cmi_forceSpinOnIdle=0;
extern std::atomic<int> _cleanUp;
extern void CharmScheduler(void);

#if CMK_HAS_TLS_VARIABLES && !CMK_NOT_USE_TLS_THREAD
static CMK_THREADLOCAL struct CmiStateStruct     Cmi_mystate;
static CmiState     *Cmi_state_vector;

CmiState CmiGetState(void) {
	return &Cmi_mystate;
}
#define CmiGetStateN(n) Cmi_state_vector[n]

#else

static pthread_key_t Cmi_state_key=(pthread_key_t)(-1);
static CmiState     Cmi_state_vector;

#if 0
#define CmiGetState() ((CmiState)pthread_getspecific(Cmi_state_key))
#else
CmiState CmiGetState(void) {
	CmiState ret;
	if (Cmi_state_key == (pthread_key_t)(-1)) return &Cmi_default_state;
	ret=(CmiState)pthread_getspecific(Cmi_state_key);
	return (ret==NULL)? &Cmi_default_state : ret;
}

#endif
#define CmiGetStateN(n) (Cmi_state_vector+(n))
#endif


#if !CMK_USE_LRTS
#if CMK_HAS_SPINLOCK && CMK_USE_SPINLOCK
CmiNodeLock CmiCreateLock(void)
{
  CmiNodeLock lk = (CmiNodeLock)malloc(sizeof(pthread_spinlock_t));
  _MEMCHECK(lk);
  pthread_spin_init(lk, 0);
  return lk;
}

void CmiDestroyLock(CmiNodeLock lk)
{
  pthread_spin_destroy(lk);
  free((void*)lk);
}
#else
CmiNodeLock CmiCreateLock(void)
{
  CmiNodeLock lk = (CmiNodeLock)malloc(sizeof(pthread_mutex_t));
  _MEMCHECK(lk);
  pthread_mutex_init(lk,(pthread_mutexattr_t *)0);
  return lk;
}

void CmiDestroyLock(CmiNodeLock lk)
{
  pthread_mutex_destroy(lk);
  free(lk);
}
#endif
#endif //CMK_USE_LRTS

void CmiYield(void) { sched_yield(); }

static CmiNodeLock comm_mutex;

#define CmiCommLockOrElse(x) /*empty*/

#if 1
/*Regular comm. lock*/
#  define CmiCommLock() CmiLock(comm_mutex)
#  define CmiCommUnlock() CmiUnlock(comm_mutex)
#else
/*Verbose debugging comm. lock*/
static int comm_mutex_isLocked=0;
void CmiCommLock(void) {
	if (comm_mutex_isLocked) 
		CmiAbort("CommLock: already locked!\n");
	CmiLock(comm_mutex);
	comm_mutex_isLocked=1;
}
void CmiCommUnlock(void) {
	if (!comm_mutex_isLocked)
		CmiAbort("CommUnlock: double unlock!\n");
	comm_mutex_isLocked=0;
	CmiUnlock(comm_mutex);
}
#endif

/*
static void comm_thread(void)
{
  while (1) CommunicationServer(5);
}

static void *call_startfn(void *vindex)
{
  int index = (int)vindex;
  CmiState state = Cmi_state_vector + index;
  pthread_setspecific(Cmi_state_key, state);
  ConverseRunPE(0);
  return 0;
}
*/

void StartInteropScheduler(void);
void CommunicationServerThread(int sleepTime);

static void *call_startfn(void *vindex)
{
  size_t index = (size_t)vindex;
#if CMK_HAS_TLS_VARIABLES && !CMK_NOT_USE_TLS_THREAD
  if (index<_Cmi_mynodesize) 
    CmiStateInit(index+Cmi_nodestart, index, &Cmi_mystate);
  else
    CmiStateInit(_Cmi_mynode+CmiNumPes(),_Cmi_mynodesize,&Cmi_mystate);
  Cmi_state_vector[index] = &Cmi_mystate;
#else
  CmiState state = Cmi_state_vector + index;
  pthread_setspecific(Cmi_state_key, state);
#endif

  ConverseRunPE(0);

  if(CharmLibInterOperate) {
    while(!_cleanUp.load()) {
      StartInteropScheduler();
      CmiNodeAllBarrier();
    }

    if (CmiMyRank() == CmiMyNodeSize()) {
      while (ckExitComplete.load() == 0) { CommunicationServerThread(5); }
    } else {
      CsdScheduler(-1);
      CmiNodeAllBarrier();
    }
  }

#if 0
  if (index<_Cmi_mynodesize) 
	  ConverseRunPE(0); /*Regular worker thread*/
  else 
  { /*Communication thread*/
	  CommunicationServerInit();
	  if (Cmi_charmrun_fd!=-1)
		  while (1) CommunicationServer(5,COM_SERVER_FROM_SMP);
  }
#endif  
  return 0;
}

#if CMK_BLUEGENEQ && !CMK_USE_LRTS
/* pami/machine.C defines its own version of this: */
void PerrorExit(const char*);
#endif

#if CMK_CONVERSE_PAMI
// Array used by the 'rank 0' thread to wait for other threads using pthread_join
pthread_t *_Cmi_mypidlist;
#endif

static void CmiStartThreads(char **argv)
{
  pthread_t pid;
  size_t i;
  int ok, tocreate;
  pthread_attr_t attr;
  int start, end;

  MACHSTATE(4,"CmiStartThreads")
  CmiMemLock_lock=CmiCreateLock();
  _smp_mutex = CmiCreateLock();
  cmiMemoryLock = CmiCreateLock();

#if ! (CMK_HAS_TLS_VARIABLES && !CMK_NOT_USE_TLS_THREAD)
  pthread_key_create(&Cmi_state_key, 0);
  Cmi_state_vector =
    (CmiState)calloc(_Cmi_mynodesize+1, sizeof(struct CmiStateStruct));
  for (i=0; i<_Cmi_mynodesize; i++)
    CmiStateInit(i+Cmi_nodestart, i, CmiGetStateN(i));
  /*Create a fake state structure for the comm. thread*/
/*  CmiStateInit(-1,_Cmi_mynodesize,CmiGetStateN(_Cmi_mynodesize)); */
  CmiStateInit(_Cmi_mynode+CmiNumPes(),_Cmi_mynodesize,CmiGetStateN(_Cmi_mynodesize));
#else
    /* for main thread */
  Cmi_state_vector = (CmiState *)calloc(_Cmi_mynodesize+1, sizeof(CmiState));
#if CMK_CONVERSE_MPI
      /* main thread is communication thread */
  if(!CharmLibInterOperate) {
    CmiStateInit(_Cmi_mynode+CmiNumPes(), _Cmi_mynodesize, &Cmi_mystate);
    Cmi_state_vector[_Cmi_mynodesize] = &Cmi_mystate;
  } else 
#endif
  {
    /* main thread is of rank 0 */
    CmiStateInit(Cmi_nodestart, 0, &Cmi_mystate);
    Cmi_state_vector[0] = &Cmi_mystate;
  }
#endif

#if CMK_MULTICORE || CMK_SMP_NO_COMMTHD
  if (!Cmi_commthread)
    tocreate = _Cmi_mynodesize-1;
  else
#endif
  tocreate = _Cmi_mynodesize;
#if CMK_CONVERSE_MPI
  if(!CharmLibInterOperate) {
    start = 0;
    end = tocreate - 1;                    /* skip comm thread */
  } else 
#endif
  {
    start = 1;
    end = tocreate;                       /* skip rank 0 main thread */
  }

#if CMK_CONVERSE_PAMI
  // allocate space for the pids
  _Cmi_mypidlist = (pthread_t *)malloc(sizeof(pthread_t)*(end - start +1));
  int numThreads = 0;
#endif

  for (i=start; i<=end; i++) {        
    pthread_attr_init(&attr);
    pthread_attr_setscope(&attr, PTHREAD_SCOPE_SYSTEM);
    ok = pthread_create(&pid, &attr, call_startfn, (void *)i);
    if (ok!=0){
      CmiPrintf("CmiStartThreads: %s(%d)\n", strerror(errno), errno);
      PerrorExit("pthread_create");
    }
#if CMK_CONVERSE_PAMI
    _Cmi_mypidlist[numThreads++] = pid; // store the pid in the array
#endif
    pthread_attr_destroy(&attr);
  }
#if ! (CMK_HAS_TLS_VARIABLES && !CMK_NOT_USE_TLS_THREAD)
#if CMK_CONVERSE_MPI
  if(!CharmLibInterOperate)
    pthread_setspecific(Cmi_state_key, Cmi_state_vector+_Cmi_mynodesize);
  else 
#endif
    pthread_setspecific(Cmi_state_key, Cmi_state_vector);
#endif

  MACHSTATE(4,"CmiStartThreads done")
}

static void CmiDestroyLocks(void)
{
  CmiDestroyLock(comm_mutex);
  comm_mutex = 0;
  CmiDestroyLock(CmiMemLock_lock);
  CmiMemLock_lock = 0;
  CmiDestroyLock(cmiMemoryLock);
}

#endif

#if !CMK_SHARED_VARS_UNAVAILABLE

class Barrier
{
public:
  Barrier(const Barrier&) = delete;
  Barrier& operator=(const Barrier&) = delete;

  explicit Barrier(unsigned int count)
      : curCount(count), barrierCount(count), curSense(true)
  {
  }

  void arrive_and_wait()
  {
    const bool sense = curSense.load(std::memory_order_relaxed);

    // If we're last, reset the count and flip the sense.
    // Using acq_rel here effectively makes this read-modify-write a fence.
    // Note: we compare against 1 because fetch_sub returns the previous value.
    if (curCount.fetch_sub(1, std::memory_order_acq_rel) == 1)
    {
      curCount.store(barrierCount, std::memory_order_relaxed);
      curSense.store(!sense, std::memory_order_release);
#  ifdef __cpp_lib_atomic_wait
      curSense.notify_all();
#  endif
      return;
    }

    // Use C++20's atomic wait/notify if it's available
#  ifdef __cpp_lib_atomic_wait
    // Waits while sense == curSense, cannot return spuriously, so no need for extra check
    curSense.wait(sense, std::memory_order_acquire);
#  else
    // Otherwise we're not the last, so start the progressive spin sequence:
    // Define how many iterations to spend in each phase. These are arbitrary constants,
    // they can be tuned for performance, but this should be generally alright.
    constexpr auto spinIters = 5, pauseIters = 10, yieldIters = 1000;

    // Straight up spin at first.
    for (int i = 0; i < spinIters; i++)
    {
      if (sense != curSense.load(std::memory_order_acquire))
        return;
    }

    // If that's taking too long, then start pausing during each spin.
    for (int i = 0; i < pauseIters; i++)
    {
      if (sense != curSense.load(std::memory_order_acquire))
        return;
      pause();
    }

    // If we're still in the barrier, then it's time to lay off a bit
    // and start yielding this thread at the OS level. Without this,
    // oversubscribed scenarios become extremely costly.
    while (true)
    {
      for (int i = 0; i < yieldIters; i++)
      {
        if (sense != curSense.load(std::memory_order_acquire))
          return;
        repeat_pause<10>();
      }
      std::this_thread::yield();
    }
#  endif
  }

private:
  alignas(CMI_CACHE_LINE_SIZE) std::atomic<bool> curSense;
  alignas(CMI_CACHE_LINE_SIZE) std::atomic<unsigned int> curCount;
  const unsigned int barrierCount;

  CMI_FORCE_INLINE static void pause()
  {
#  if CMK_AMD64
    // From the Intel intrinsics guide: "Provide a hint to the processor that the code
    // sequence is a spin-wait loop. This can help improve the performance and power
    // consumption of spin-wait loops."
    // Accomplishes two things: 1.) Relinquishes resources, saving power and improving the
    // performance of other threads 2.) Prevents a false memory order violation from being
    // detected when the barrier is reached, causing a flush penalty.
    // Note: While this usually takes from 0 to a low double digits number of cycles, it
    // can take up to 140 cycles (on Skylake), so it can be costly.
    _mm_pause();  // MSVC's support for assembly is iffy, so use intrinsic
#  elif CMK_ARM
    // Notifies the hardware that the current task can be swapped out, recommended for use
    // when spinning by ARM.
    // https://developer.arm.com/documentation/dui0489/i/arm-and-thumb-instructions/yield?lang=en
    asm volatile("yield");
#  else
    // For other platforms, do nothing.
#  endif
  }

  // This template rigmarole is to ensure that the compiler generates a sequences of
  // pauses without anything else in between, using a regular loop doesn't do that on icc,
  // for example
  template <size_t N>
  CMI_FORCE_INLINE static void repeat_pause()
  {
    pause();
    repeat_pause<N - 1>();
  }
};

template <>
CMI_FORCE_INLINE void Barrier::repeat_pause<0>()
{
}

/* Wait for all worker threads */
void CmiNodeBarrier(void) {
  static Barrier nodeBarrier(CmiMyNodeSize());
  nodeBarrier.arrive_and_wait();
}

/* Wait for all worker threads as well as comm. thread */
/* unfortunately this could also be called in a seemingly non smp version
   net-win32, which actually is implemented as smp with comm. thread */
void CmiNodeAllBarrier(void) {
#if CMK_MULTICORE || CMK_SMP_NO_COMMTHD
  if (!Cmi_commthread) {
    static Barrier nodeBarrier(CmiMyNodeSize());
    nodeBarrier.arrive_and_wait();
    return;
  }
#endif
  static Barrier nodeAllBarrier(CmiMyNodeSize() + 1);
  nodeAllBarrier.arrive_and_wait();
}

#endif

/***********************************************************
 * SMP Idle Locking
 *   In an SMP system, idle processors need to sleep on a
 * lock so that if a message for them arrives, they can be
 * woken up.
 **********************************************************/

static int CmiIdleLock_hasMessage(CmiState cs) {
  return cs->idle.hasMessages;
}

#if CMK_SHARED_VARS_NT_THREADS

static void CmiIdleLock_init(CmiIdleLock *l) {
  l->hasMessages=0;
  l->isSleeping=0;
  l->sem=CreateSemaphore(NULL,0,1, NULL);
}

static void CmiIdleLock_sleep(CmiIdleLock *l,int msTimeout) {
  if (l->hasMessages) return;
  l->isSleeping=1;
  MACHSTATE(4,"Processor going to sleep {")
  WaitForSingleObject(l->sem,msTimeout);
  MACHSTATE(4,"} Processor awake again")
  l->isSleeping=0;
}

static void CmiIdleLock_addMessage(CmiIdleLock *l) {
  l->hasMessages=1;
  if (l->isSleeping) { /*The PE is sleeping on this lock-- wake him*/  
    MACHSTATE(4,"Waking sleeping processor")
    ReleaseSemaphore(l->sem,1,NULL);
  }
}
static void CmiIdleLock_checkMessage(CmiIdleLock *l) {
  l->hasMessages=0;
}

#elif CMK_SHARED_VARS_POSIX_THREADS_SMP

static void CmiIdleLock_init(CmiIdleLock *l) {
  l->hasMessages=0;
  l->isSleeping=0;
  pthread_mutex_init(&l->mutex,NULL);
  pthread_cond_init(&l->cond,NULL);
}

#include <sys/time.h>

static void getTimespec(int msFromNow,struct timespec *dest) {
  struct timeval cur;
  int secFromNow;
  /*Get the current time*/
  gettimeofday(&cur,NULL);
  dest->tv_sec=cur.tv_sec;
  dest->tv_nsec=cur.tv_usec*1000;
  /*Add in the wait time*/
  secFromNow=msFromNow/1000;
  msFromNow-=secFromNow*1000;
  dest->tv_sec+=secFromNow;
  dest->tv_nsec+=1000*1000*msFromNow;
  /*Wrap around if we overflowed the nsec field*/
  while (dest->tv_nsec>=1000000000ul) {
    dest->tv_nsec-=1000000000ul;
    dest->tv_sec++;
  }
}

static void CmiIdleLock_sleep(CmiIdleLock *l,int msTimeout) {
  struct timespec wakeup;

  if (l->hasMessages) return;
  l->isSleeping=1;
  MACHSTATE(4,"Processor going to sleep {")
  pthread_mutex_lock(&l->mutex);
  getTimespec(msTimeout,&wakeup);
  while (!l->hasMessages)
    if (ETIMEDOUT==pthread_cond_timedwait(&l->cond,&l->mutex,&wakeup))
      break;
  pthread_mutex_unlock(&l->mutex);
  MACHSTATE(4,"} Processor awake again")
  l->isSleeping=0;
}

static void CmiIdleLock_wakeup(CmiIdleLock *l) {
  l->hasMessages=1; 
  MACHSTATE(4,"Waking sleeping processor")
  /*The PE is sleeping on this condition variable-- wake him*/
  pthread_mutex_lock(&l->mutex);
  pthread_cond_signal(&l->cond);
  pthread_mutex_unlock(&l->mutex);
}

static void CmiIdleLock_addMessage(CmiIdleLock *l) {
  if (l->isSleeping) CmiIdleLock_wakeup(l);
  l->hasMessages=1;
}
static void CmiIdleLock_checkMessage(CmiIdleLock *l) {
  l->hasMessages=0;
}
#else
#define CmiIdleLock_sleep(x, y) /*empty*/

static void CmiIdleLock_init(CmiIdleLock *l) {
  l->hasMessages=0;
}
static void CmiIdleLock_addMessage(CmiIdleLock *l) {
  l->hasMessages=1;
}
static void CmiIdleLock_checkMessage(CmiIdleLock *l) {
  l->hasMessages=0;
}
#endif

void CmiStateInit(int pe, int rank, CmiState state)
{
#if CMK_SMP_MULTIQ
  int i;
#endif

  MACHSTATE(4,"StateInit")
  state->pe = pe;
  state->rank = rank;
  if (rank==CmiMyNodeSize()) return; /* Communications thread */
#if !CMK_SMP_MULTIQ
  state->recv = CMIQueueCreate();
#else
  for(i=0; i<MULTIQ_GRPSIZE; i++) state->recv[i]=CMIQueueCreate();
  state->myGrpIdx = rank % MULTIQ_GRPSIZE;
  state->curPolledIdx = 0;
#endif
  state->localqueue = CdsFifo_Create();
  CmiIdleLock_init(&state->idle);
}

void CmiNodeStateInit(CmiNodeState *nodeState)
{
  MACHSTATE1(4,"NodeStateInit %p", nodeState)
#if CMK_IMMEDIATE_MSG
  nodeState->immSendLock = CmiCreateLock();
  nodeState->immRecvLock = CmiCreateLock();
  nodeState->immQ = CMIQueueCreate();
  nodeState->delayedImmQ = CMIQueueCreate();
#endif
#if CMK_NODE_QUEUE_AVAILABLE
  nodeState->CmiNodeRecvLock = CmiCreateLock();
#if CMK_LOCKLESS_QUEUE
  nodeState->NodeRecv = MPMCQueueCreate();
#else
  nodeState->NodeRecv = CMIQueueCreate();
#endif
#endif
  MACHSTATE(4,"NodeStateInit done")
}

/*@}*/
