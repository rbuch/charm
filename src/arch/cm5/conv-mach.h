/***************************************************************************
 * RCS INFORMATION:
 *
 *   $RCSfile$
 *   $Author$       $Locker$        $State$
 *   $Revision$     $Date$
 *
 ***************************************************************************
 *
 * $Log$
 * Revision 2.18  1996-07-16 05:20:41  milind
 * Added CMK_VECTOR_SEND
 *
 * Revision 2.17  1996/07/15  20:58:27  jyelon
 * Flags now use #if, not #ifdef.  Also cleaned up a lot.
 *
 *
 **************************************************************************/

#ifndef _CONV_MACH_H
#define _CONV_MACH_H

#define CMK_ASYNC_DOESNT_WORK_USE_TIMER_INSTEAD            0
#define CMK_ASYNC_NOT_NEEDED                               1
#define CMK_ASYNC_USE_FIOASYNC_AND_FIOSETOWN               0
#define CMK_ASYNC_USE_FIOASYNC_AND_SIOCSPGRP               0
#define CMK_ASYNC_USE_FIOSSAIOSTAT_AND_FIOSSAIOOWN         0
#define CMK_ASYNC_USE_F_SETFL_AND_F_SETOWN                 0

#define CMK_CMIDELIVERS_USE_COMMON_CODE                    1
#define CMK_CMIDELIVERS_USE_SPECIAL_CODE                   0

#define CMK_CMIMYPE_IS_A_BUILTIN                           0
#define CMK_CMIMYPE_IS_A_VARIABLE                          1
#define CMK_CMIMYPE_UNIPROCESSOR                           0

#define CMK_CMIPRINTF_IS_A_BUILTIN                         0
#define CMK_CMIPRINTF_IS_JUST_PRINTF                       1

#define CMK_COMMHANDLE_IS_AN_INTEGER                       1
#define CMK_COMMHANDLE_IS_A_POINTER                        0

#define CMK_CSDEXITSCHEDULER_IS_A_FUNCTION                 0
#define CMK_CSDEXITSCHEDULER_SET_CSDSTOPFLAG               1

#define CMK_CTHINIT_IS_IN_CONVERSEINIT                     1
#define CMK_CTHINIT_IS_IN_MAIN                             0

#define CMK_DEFAULT_MAIN_USES_COMMON_CODE                  1
#define CMK_DEFAULT_MAIN_USES_SIMULATOR_CODE               0

#define CMK_DGRAM_BUF_SIZE                                 0
#define CMK_DGRAM_MAX_SIZE                                 0
#define CMK_DGRAM_WINDOW_SIZE                              0

#define CMK_MACHINE_NAME                                   "cm5"

#define CMK_MALLOC_USE_GNU                                 0
#define CMK_MALLOC_USE_GNU_WITH_INTERRUPT_SUPPORT          1
#define CMK_MALLOC_USE_OS_BUILTIN                          0

#define CMK_MSG_HEADER_SIZE_BYTES                          4

#define CMK_PREPROCESSOR_CANNOT_DO_CONCATENATION           0
#define CMK_PREPROCESSOR_USES_ANSI_STANDARD_CONCATENATION  1

#define CMK_PROTOTYPES_FAIL                                0
#define CMK_PROTOTYPES_WORK                                1

#define CMK_RSH_IS_A_COMMAND                               1
#define CMK_RSH_NOT_NEEDED                                 0
#define CMK_RSH_USE_REMSH                                  0

#define CMK_SHARED_VARS_EXEMPLAR                           0
#define CMK_SHARED_VARS_UNAVAILABLE                        1
#define CMK_SHARED_VARS_UNIPROCESSOR                       0

#define CMK_SIGHOLD_IS_A_BUILTIN                           1
#define CMK_SIGHOLD_NOT_NEEDED                             0
#define CMK_SIGHOLD_USE_SIGMASK                            0

#define CMK_SIGNAL_IS_A_BUILTIN                            1
#define CMK_SIGNAL_NOT_NEEDED                              0
#define CMK_SIGNAL_USE_SIGACTION                           0
#define CMK_SIGNAL_USE_SIGACTION_WITH_RESTART              0

#define CMK_SIZE_T                                         long

#define CMK_STATIC_PROTO_FAILS                             0
#define CMK_STATIC_PROTO_WORKS                             1

#define CMK_STRERROR_IS_A_BUILTIN                          0
#define CMK_STRERROR_USE_SYS_ERRLIST                       1

#define CMK_STRINGS_USE_OWN_DECLARATIONS                   0
#define CMK_STRINGS_USE_STRINGS_H                          0
#define CMK_STRINGS_USE_STRING_H                           1

#define CMK_THREADS_UNAVAILABLE                            0
#define CMK_THREADS_USE_ALLOCA                             0
#define CMK_THREADS_USE_ALLOCA_WITH_HEADER_FILE            1
#define CMK_THREADS_USE_ALLOCA_WITH_PRAGMA                 0
#define CMK_THREADS_USE_JB_TWEAKING                        0

#define CMK_TIMER_USE_GETRUSAGE                            0
#define CMK_TIMER_USE_SPECIAL                              0
#define CMK_TIMER_USE_TIMES                                1

#define CMK_USE_GENERIC_VECTOR_SEND                        1
#define CMK_USE_SPECIAL_VECTOR_SEND                        0

#define CMK_WAIT_NOT_NEEDED                                0
#define CMK_WAIT_USES_SYS_WAIT_H                           1
#define CMK_WAIT_USES_WAITFLAGS_H                          0

#define CMK_WHEN_PROCESSOR_IDLE_BUSYWAIT                   1
#define CMK_WHEN_PROCESSOR_IDLE_USLEEP                     0

#endif

