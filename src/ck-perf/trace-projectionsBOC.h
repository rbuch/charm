#include "envelope.h"
#include "register.h"
#include "trace-common.h"
#include "ckcallback-ccs.h"

#ifndef PROJ_ANALYSIS
// NOTE: Needed to handle the automatically-generated method so 
//   trace-projections would build correctly while ignoring any of the 
//   BOC-based definitions generated by parsing trace-projections.ci.
//   Hence, we do not include TraceProjections.decl.h in this version.
//
//   This version of trace-projections would not permit any form of
//   end-of-run operations and NONE of the definitions found in 
//   trace-projections.ci would be visible to the rest of the code
//   (trace-projections.C), so some care would be needed to ensure 
//   PROJ_ANALYSIS encloses the correct code-fragments in 
//   trace-projections.C
void _registerTraceProjections() {
  // faked call that does nothing.
}
#else
#include "TraceProjections.decl.h"

class KMeansStatsMessage : public CMessage_KMeansStatsMessage {
 public:
  int numMetrics;
  int numKPos;
  int numStats;
  bool *filter;
  double *kSeedsPos;
  double *stats;
};

class KSeedsMessage : public CMessage_KSeedsMessage {
 public:
  int numKPos;
  double *kSeedsPos;
};

class TraceProjectionsInit : public Chare {
 public:
  TraceProjectionsInit(CkArgMsg *msg);
 TraceProjectionsInit(CkMigrateMessage *m):Chare(m) {}
};

class KSelectionMessage : public CMessage_KSelectionMessage {
 public:
  int numKMinIDs;
  int numKMaxIDs;
  int *minIDs;
  int *maxIDs;
};

class KMeansBOC : public CBase_KMeansBOC {
 private:
  // commandline parameters
  bool autoCompute;
  int numK;
  int peNumKeep;
  double entryThreshold;
  bool usePhases;

  int numKReported;

  // variables for correct data gathering across phases
  bool markedBegin;
  bool markedIdle;
  double beginBlockTime;
  double beginIdleBlockTime;
  int lastBeginEPIdx;
  int numSelectionIter;
  bool selected;

  int currentPhase;
  int lastPhaseIdx;
  double *currentExecTimes;

  // kMeans outlier structures - ALL processors will host this data
  int numEntryMethods;
  int numMetrics;
  int phaseIter;

  bool *keepMetric;
  double *incKSeeds;
  double minDistance; // distance to the closest seed
  int lastMinK;
  int minK; // the seed closest to the processor

  // ONLY processor 0 will host this data, the location vector of K seeds
  // This is actually a 2-D array, we need it to be contiguous for
  //   communication purposes.
  double *kSeeds; 
  int *kNumMembers;

  int *exemplarChoicesLeft;
  int *outlierChoicesLeft;

 public:
 KMeansBOC(bool outlierAutomatic, int numKSeeds, int _peNumKeep,
	   double _entryThreshold, bool outlierUsePhases) :
  autoCompute(outlierAutomatic), numK(numKSeeds), 
    peNumKeep(_peNumKeep), entryThreshold(_entryThreshold),
    usePhases(outlierUsePhases) {};
 KMeansBOC(CkMigrateMessage *m):CBase_KMeansBOC(m) {};
  
  void startKMeansAnalysis();
  void flushCheck(CkReductionMsg *msg);
  void flushCheckDone();
  void getNextPhaseMetrics();
  void collectKMeansData(); // C++ method
  void globalMetricRefinement(CkReductionMsg *msg);
  void initKSeeds(); // C++ method
  void findInitialClusters(KMeansStatsMessage *msg);
  void updateKSeeds(CkReductionMsg *msg);
  double calculateDistance(int k); // C++ method
  void updateSeedMembership(KSeedsMessage *msg);
  void findRepresentatives(); // C++ method
  void collectDistances(KSelectionMessage *msg);
  void findNextMinMax(CkReductionMsg *msg);
  void phaseDone();

  /*
  void calculateWeights(KMeansStatsMessage *);
  void determineOutliers(OutlierWeightMessage *);
  void setOutliers(OutlierThresholdMessage *);
  */
};

class TraceProjectionsBOC : public CBase_TraceProjectionsBOC {
 private:
  bool findOutliers;

  int parModulesRemaining;

  double dummy;
  double endTime;
  double analysisStartTime;
 public:
 TraceProjectionsBOC(bool _findOutliers) : findOutliers(_findOutliers) {};
 TraceProjectionsBOC(CkMigrateMessage *m):CBase_TraceProjectionsBOC(m) {};

  void traceProjectionsParallelShutdown();
  void startEndTimeAnalysis();
  void endTimeDone(CkReductionMsg *);
  void kMeansDone(CkReductionMsg *);
  void kMeansDone(void);
  void finalize(void);
  void shutdownAnalysis(void);
  void closingTraces(void);
  void closeParallelShutdown(CkReductionMsg *);

  void ccsOutlierRequest(CkCcsRequestMsg *);
};
#endif
