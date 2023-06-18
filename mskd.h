
#define MAX_MODULE_NUM 1024
#define MAX_PORT_NUM (MAX_MODULE_NUM*4)
#define MAX_CONN_NUM (MAX_PORT_NUM/2)
#define MAX_EDGE_NUM MAX_CONN_NUM
#define MAX_TIME_LEVEL 8

struct CycleT {
  int N;
  int b;
  int n[MAX_TIME_LEVEL];
  int I[MAX_TIME_LEVEL];
};

struct ModSys {
  int N;    // #modules
  int NP;    // #ports
  int ND;   // #connections
  int latency[MAX_MODULE_NUM];  // module latency
  int from[MAX_CONN_NUM];   // from array 
  int to[MAX_CONN_NUM];     // to array 
  int mp[MAX_PORT_NUM];     // which module the ith port belongs to

  struct CycleT cycle[MAX_PORT_NUM];  // CycleT for fp[k] calculation
};


struct MSkdSolution
{
  int N; 
  int starttime[MAX_MODULE_NUM];
  int latency;
};

struct BuffSolution
{
  int ND;  // #connections
  int from[MAX_CONN_NUM];   // from array 
  int to[MAX_CONN_NUM];     // to array 
  int length[MAX_CONN_NUM];   // buffer length
};

int mod_skd(struct ModSys *ms, struct MSkdSolution *solution);
int buff_opt(struct ModSys *ms, struct MSkdSolution *skd, struct BuffSolution *solution);


