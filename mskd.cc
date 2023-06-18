#include "mskd.h"

#include <chrono>
#include <climits>
#include <cstdlib>
#include <fstream>
#include <iostream>

#include "externel/json.hpp"
#include "gurobi_c++.h"

// #define ENABLE_LOG
#define INIT_LOG                                                               \
  {                                                                            \
    FILE *flog = fopen("run.log", "w");                                        \
    fclose(flog);                                                              \
  }

#ifdef ENABLE_LOG
#define LOG(...)                                                               \
  {                                                                            \
    FILE *flog = fopen("run.log", "a");                                        \
    fprintf(flog, __VA_ARGS__);                                                \
    fclose(flog);                                                              \
  }
#else
#define LOG(...)                                                               \
  {                                                                            \
  }
#endif

#define min(x, y) ((x) < (y) ? (x) : (y))
#define max(x, y) ((x) > (y) ? (x) : (y))

struct SDC {
  int N;                   // #nodes
  int M;                   // #edges
  int source;              // source node
  int sink;                // sink node
  int from[MAX_EDGE_NUM];  // from array
  int to[MAX_EDGE_NUM];    // to array
  int delta[MAX_EDGE_NUM]; // delta
} sdc;

void print_skd(struct MSkdSolution *solution, bool legal = true) {
  LOG("schedule solution: latency = %d\n", solution->latency);
  for (int i = 0; i < solution->N; i++) {
    LOG("\t%d", solution->starttime[i]);
  }
  LOG("\n");
}

void print_sdc(struct SDC *sdc) {
  LOG("sdc: N = %d, M = %d\n", sdc->N, sdc->M);
  for (int i = 0; i < sdc->M; i++) {
    LOG("\t(%d -> %d: %d)", sdc->from[i], sdc->to[i], sdc->delta[i]);
  }
  LOG("\n");
}

void print_buff(struct BuffSolution *solution) {
  LOG("Buffer solution:\n");
  for (int i = 0; i < solution->ND; i++) {
    LOG("\t(%d -> %d: %d)", solution->from[i], solution->to[i],
        solution->length[i]);
  }
  LOG("\n");
}

int solve_delta(struct CycleT x, struct CycleT y, int *delta, char *name) {
  /*
   *  tx - ty <= min(fy[K] - fx[K])
   *  minimize fy[Ky] - fx[Kx]
   *    s.t. Ky = Kx (which is relaxed to Ky>=Kx, that is Kx-Ky<=0)
   */

  LOG("start retrieving delta: %s\n", name);
  char logFile[50], modelFile[50];
  sprintf(logFile, "logs/%s.log", name);
  sprintf(modelFile, "models/%s.lp", name);

  LOG("\tlog file is %s\n", logFile);
  LOG("\tmodel file is %s\n", modelFile);

  try {
    GRBEnv env = GRBEnv();
    env.set("LogFile", logFile);
    env.start();

    GRBModel model = GRBModel(env);

    int num_var = x.N + y.N;
    GRBVar *vars = new GRBVar[num_var];

    LOG("\tadd variables*%d:", num_var);
    GRBLinExpr obj = (double)(y.b - x.b);

    for (int i = 0; i < x.N; i++) {
      vars[i] = model.addVar(0, x.n[i] - 1, 0, GRB_INTEGER,
                             "k_x_" + std::to_string(i));
      obj += vars[i] * (-x.I[i]);
    }
    for (int i = 0; i < y.N; i++) {
      vars[x.N + i] = model.addVar(0, y.n[i] - 1, 0, GRB_INTEGER,
                                   "k_y_" + std::to_string(i));
      obj += vars[x.N + i] * (y.I[i]);
    }

    LOG("done\n");
    // std::cout << obj <<std::endl;
    model.setObjective(obj, GRB_MINIMIZE);

    LOG("\tadd constraints:");

    /* Add constraints Ky >= Kx */
    int iter_x[MAX_TIME_LEVEL], iter_y[MAX_TIME_LEVEL];
    iter_x[x.N - 1] = 1;
    for (int i = x.N - 2; i >= 0; i--)
      iter_x[i] = iter_x[i + 1] * x.n[i + 1];
    iter_y[y.N - 1] = 1;
    for (int i = y.N - 2; i >= 0; i--)
      iter_y[i] = iter_y[i + 1] * y.n[i + 1];

    GRBLinExpr lhs = 0;
    for (int i = 0; i < x.N; i++) {
      lhs += vars[i] * iter_x[i];
    }
    for (int i = 0; i < y.N; i++) {
      lhs += vars[i + x.N] * iter_y[i];
    }
    model.addConstr(lhs <= 0.0, "kx_lt_ky");
    LOG("done\n");

    // Optimize
    model.write(modelFile);
    model.optimize();
    int status = model.get(GRB_IntAttr_Status);
    if (status == GRB_UNBOUNDED) {
      std::cout << "The model cannot be solved "
                << "because it is unbounded" << std::endl;
      return 1;
    }
    if (status == GRB_OPTIMAL) {
      std::cout << "The optimal objective is "
                << model.get(GRB_DoubleAttr_ObjVal) << std::endl;
      for (int i = 0; i < num_var; i++) {
        LOG("\t%f", model.getVar(i).get(GRB_DoubleAttr_X));
      }
      LOG("\n");
      // return 0;
    } else if ((status != GRB_INF_OR_UNBD) && (status != GRB_INFEASIBLE)) {
      std::cout << "Optimization was stopped with status " << status
                << std::endl;
      return 1;
    }

    // Set delta_{y->x} = min{ fy[Ky] - fx[Kx] }
    *delta = (int)model.get(GRB_DoubleAttr_ObjVal);
    LOG("\tDelta_{y->x} is %d\n", *delta);

    delete[] vars;
  } catch (GRBException e) {
    std::cout << "Error code = " << e.getErrorCode() << std::endl;
    std::cout << e.getMessage() << std::endl;
  } catch (...) {
    std::cout << "Exception during optimization" << std::endl;
  }

  return 0;
}

int solve_starttime(struct SDC *sdc, struct MSkdSolution *solution) {
  print_sdc(sdc);
  solution->N = sdc->N; // (sdc->N-2) modules

  int *dist = new int[sdc->N];

  // predecessor array
  // size equal to the number of vertices of the graph g
  int *p = new int[sdc->N];

  // step 1: fill the distance array and predecessor array
  for (int i = 0; i < sdc->N; i++) {
    dist[i] = INT_MAX;
    p[i] = 0;
  }

  // mark the source vertex
  dist[sdc->source] = 0;

  // step 2: relax edges |V| - 1 times
  int u, v, w;
  for (int i = 1; i <= sdc->N - 1; i++) {
    for (int j = 0; j < sdc->M; j++) {
      // get the edge data
      u = sdc->from[j];
      v = sdc->to[j];
      w = sdc->delta[j];

      if (dist[u] != INT_MAX && dist[v] > dist[u] + w) {
        dist[v] = dist[u] + w;
        p[v] = u;
      }
    }
  }

  // step 3: detect negative cycle
  // if value changes then we have a negative cycle in the graph
  // and we cannot find the shortest distances
  for (int i = 0; i < sdc->M; i++) {
    u = sdc->from[i];
    v = sdc->to[i];
    w = sdc->delta[i];
    if (dist[u] != INT_MAX && dist[v] > dist[u] + w) {
      LOG("Negative weight cycle detected!\n");
      delete[] dist;
      delete[] p;
      return 1;
    }
  }

  // bia stattimes to non-negative
  int min_val = INT_MAX;
  for (int i = 0; i < sdc->N; i++) {
    min_val = min(min_val, dist[i]);
  }
  for (int i = 0; i < sdc->N; i++) {
    LOG("\tdist[%d]=%d", i, dist[i]);
  }
  LOG("\n");

  for (int i = 0; i < solution->N; i++) {
    solution->starttime[i] = dist[i] - min_val;
  }
  solution->latency = dist[sdc->sink] - min_val;

  delete[] p;
  delete[] dist;
  return 0;
}

int solve_buff(struct CycleT x, struct CycleT y, int tx, int ty, int *len,
               char *name) {
  /*
   *  minimize l
   *    s.t. tx + fx[i] < ty + fy[j] => i-j+1<=l
   *  [HARD TO SOLVE], give up...
   */
  /*
   *   maximize i - j + 1
   *     s.t. tx + fx[i] < ty + fy[j]
   */

  LOG("start calculating buff: %s\n", name);
  char logFile[50], modelFile[50];
  sprintf(logFile, "logs/%s.log", name);
  sprintf(modelFile, "models/%s.lp", name);

  // LOG("\tlog file is %s\n", logFile);
  // LOG("\tmodel file is %s\n", modelFile);

  LOG("\tlog file is %s\n", logFile);
  LOG("\tmodel file is %s\n", modelFile);

  try {
    GRBEnv env = GRBEnv();
    env.set("LogFile", logFile);
    env.start();

    GRBModel model = GRBModel(env);

    int iter_x[MAX_TIME_LEVEL], iter_y[MAX_TIME_LEVEL];
    iter_x[x.N - 1] = 1;
    for (int i = x.N - 2; i >= 0; i--)
      iter_x[i] = iter_x[i + 1] * x.n[i + 1];
    iter_y[y.N - 1] = 1;
    for (int i = y.N - 2; i >= 0; i--)
      iter_y[i] = iter_y[i + 1] * y.n[i + 1];

    GRBVar *kx = new GRBVar[x.N];
    GRBVar *ky = new GRBVar[y.N];

    GRBLinExpr obj = 1;

    LOG("\tadd variables:");
    for (int i = 0; i < x.N; i++) {
      kx[i] = model.addVar(0, x.n[i] - 1, 0, GRB_INTEGER,
                           "k_x_" + std::to_string(i));
      obj += kx[i] * x.n[i];
    }
    for (int i = 0; i < y.N; i++) {
      ky[i] = model.addVar(0, y.n[i] - 1, 0, GRB_INTEGER,
                           "k_y_" + std::to_string(i));
      obj -= ky[i] * y.n[i];
    }

    LOG("done\n");

    model.setObjective(obj, GRB_MAXIMIZE);

    LOG("\tadd constraints:");

    GRBLinExpr lhs = tx - ty + x.b - y.b;
    for (int i = 0; i < x.N; i++) {
      lhs += kx[i] * x.I[i];
    }
    for (int j = 0; j < y.N; j++) {
      lhs -= ky[j] * y.I[j];
    }
    model.addConstr(lhs, GRB_LESS_EQUAL, -1, "(tx+fx[i])-(ty+fy[j]) < 0");

    LOG("done\n");

    // Optimize
    model.write(modelFile);
    model.optimize();
    int status = model.get(GRB_IntAttr_Status);
    if (status == GRB_UNBOUNDED) {
      std::cout << "The model cannot be solved "
                << "because it is unbounded" << std::endl;
      return 1;
    }
    if (status == GRB_OPTIMAL) {
      std::cout << "The optimal objective is "
                << model.get(GRB_DoubleAttr_ObjVal) << std::endl;

      GRBVar *vars = model.getVars();
      for (int i = 0; i < x.N + y.N; i++) {
        LOG("\tvar %s = %f\n", vars[i].get(GRB_StringAttr_VarName).c_str(),
            vars[i].get(GRB_DoubleAttr_X));
      }
    } else if ((status != GRB_INF_OR_UNBD) && (status != GRB_INFEASIBLE)) {
      std::cout << "Optimization was stopped with status " << status
                << std::endl;
      return 1;
    }

    // Set delta_{y->x}
    *len = max(0, ((int)model.get(GRB_DoubleAttr_ObjVal)));
    LOG("\tBuffLen_{x->y} is %d\n", *len);
    delete[] kx;
    delete[] ky;
  } catch (GRBException e) {
    std::cout << "Error code = " << e.getErrorCode() << std::endl;
    std::cout << e.getMessage() << std::endl;
    return 1;
  } catch (...) {
    std::cout << "Exception during optimization" << std::endl;
    return 1;
  }

  return 0;
}

int mod_skd(struct ModSys *ms, struct MSkdSolution *solution) {
  // int delta[MAX_CONN_NUM]; // delta[i] is \Delta_{y->x}, where x=from[i],
  int delta;
                           // y=to[i];
  int ND = ms->ND;

  /* Calculate delta */
  for (int i = 0; i < ND; i++) {
    int from = ms->from[i];
    int to = ms->to[i];

    char name[40];
    sprintf(name, "delta_%d_%d", from, to);
    int status = solve_delta(ms->cycle[from], ms->cycle[to], &delta, name);

    if (status) {
      fprintf(stderr, "ERRORs when solving delta for %s\n", name);
      return 1;
    }

    int from_m = ms->mp[from];
    int to_m = ms->mp[to];
    int found = -1;
    for (int j = 0; j < sdc.M; j++) {
      if (sdc.to[j] == from_m && sdc.from[j] == to_m) {
        found = j;
      }
    }
    if (found == -1) {
      sdc.from[sdc.M] = to_m;
      sdc.to[sdc.M] = from_m;
      sdc.delta[sdc.M] = INT_MAX;
      found = sdc.M++;
    }
    sdc.delta[found] = min(sdc.delta[found], delta);
  }

  /* Build SDC */
  // struct SDC sdc;
  sdc.N = ms->N + 2; // ms->N modules + S/T node
  int S = ms->N, T = ms->N + 1;
  sdc.source = S;
  sdc.sink = T;
  
  for (int i = 0; i < ms->N; i++) {
    // sdc.from[sdc.M] = i;
    // sdc.to[sdc.M] = S;
    sdc.from[sdc.M] = S;
    sdc.to[sdc.M] = i;
    sdc.delta[sdc.M++] = 0;
  }
  for (int i = 0; i < ms->N; i++) {
    sdc.from[sdc.M] = T;
    sdc.to[sdc.M] = i;
    sdc.delta[sdc.M++] = -ms->latency[i];
  }
  sdc.from[sdc.M] = S;
  sdc.to[sdc.M] = T;
  sdc.delta[sdc.M++] = 0;

  int status = solve_starttime(&sdc, solution);
  if (status) {
    fprintf(stderr, "Negative loops detected when solving SDC\n");
    puts("GET HERE?");
    // exit(1);
    // return 1;
  }

  if (!status)
    print_skd(solution, !status);
  else {

  }

  return status;
  //TODO: *** stack smashing detected ***: terminated when NEGATIVE LOOP!!!
}


void print_cycle(struct CycleT *cycl, int starttime) {
  fprintf(stderr, "starttime: %d, bias: %d\n", starttime, cycl->b);
  for (int i=0; i<cycl->N; i++) {
    fprintf(stderr, "\t(%d,%d)", cycl->n[i], cycl->I[i]);
  }
  fprintf(stderr, "\n");
}

int buff_opt(struct ModSys *ms, struct MSkdSolution *skd,
             struct BuffSolution *solution) {
  solution->ND = ms->ND;
  int status = 0;
  /* Calculate buffers */
  for (int i = 0; i < ms->ND; i++) {
    int from = ms->from[i];
    int to = ms->to[i];
    int from_m = ms->mp[from];
    int to_m = ms->mp[to];
    char name[50];
    sprintf(name, "buff_%d_%d", from, to);
    solution->from[i] = from;
    solution->to[i] = to;
    int cur = solve_buff(ms->cycle[from], ms->cycle[to], skd->starttime[from_m],
                   skd->starttime[to_m], solution->length + i, name);
    status |= cur;
    if (cur) {
      fprintf(stderr, "ERRORs when solving buff between %d and %d\n", from, to);
      print_cycle(ms->cycle+from, skd->starttime[from_m]);
      print_cycle(ms->cycle+to, skd->starttime[to_m]);
    }
  }

  if (!status) {
    print_buff(solution);
  }
  return status;
}

#define STANDALONE
#define FROM_FILE

#ifdef STANDALONE

void ms_log(struct ModSys *ms) {

  LOG("N = %d, NP = %d, ND = %d\n", ms->N, ms->NP, ms->ND);
  LOG("Connections: \n");
  for (int i = 0; i < ms->ND; i++) {
    LOG("\t(%d, %d)", ms->from[i], ms->to[i]);
  }
  LOG("\n");
  LOG("Ports: \n")
  for (int i = 0; i < ms->NP; i++) {
    std::string strn = "{", strI = "{";
    for (int j = 0; j < ms->cycle[i].N; j++) {
      strn += std::to_string(ms->cycle[i].n[j]);
      strn += (j + 1 == ms->cycle[i].N) ? "}" : ",";
      strI += std::to_string(ms->cycle[i].I[j]);
      strI += (j + 1 == ms->cycle[i].N) ? "}" : ",";
    }
    LOG("[N=%d, b=%d, n=%s, I=%s]\n", ms->cycle[i].N, ms->cycle[i].b,
        strn.c_str(), strI.c_str());
  }
}

int main(int argc, char **argv) {
  INIT_LOG;

  struct ModSys ms;
  struct MSkdSolution skd;
  struct BuffSolution buff;

#ifdef FROM_FILE
  // Parse from JSON file
  std::string name;
  if (argc < 2) {
    puts("default input path is ./input.json");
    name = "input.json";
  } else {
    name = argv[1];
  }

  using json = nlohmann::json;
  std::ifstream f(name.c_str());
  json data = json::parse(f);

  ms.N = data["N"];
  ms.NP = data["NP"];
  ms.ND = data["ND"];
  for (int i = 0; i < ms.ND; i++) {
    ms.from[i] = data["from"][i];
    ms.to[i] = data["to"][i];
  }
  for (int i = 0; i < ms.NP; i++) {
    ms.mp[i] = data["mp"][i];
    CycleT cycle;
    cycle.N = data["cycle"][i]["N"];
    cycle.b = data["cycle"][i]["b"];
    for (int j = 0; j < cycle.N; j++) {
      cycle.n[j] = data["cycle"][i]["n"][j];
      cycle.I[j] = data["cycle"][i]["I"][j];
    }
    ms.cycle[i] = cycle;
  }
  for (int i = 0; i < ms.N; i++) {
    ms.latency[i] = data["latency"][i];
  }

#else
  /* {2, 4, 6, 8} -> {1, 3, 5, 7} */
  // ms.N = 2;
  // ms.NP = 2;
  // ms.ND = 1;
  // ms.from[0] = 0;
  // ms.to[0] = 1;

  // ms.mp[0] = 0;
  // ms.mp[1] = 1;
  // ms.latency[0] = 8;
  // ms.latency[1] = 8;
  // struct CycleT tx;
  // struct CycleT ty;
  // tx.N = 1;
  // tx.n[0] = 4;
  // tx.I[0] = 2;
  // tx.b = 2;
  // ty.N = 1;
  // ty.n[0] = 4;
  // ty.I[0] = 2;
  // ty.b = 1;
  // ms.cycle[0] = tx;
  // ms.cycle[1] = ty;

  /* {1, 3, 5, 7} -> {2, 4, 6, 8}  */
  ms.N = 2;
  ms.NP = 2;
  ms.ND = 1;
  ms.from[0] = 0;
  ms.to[0] = 1;
  ms.mp[0] = 0;
  ms.mp[1] = 1;
  ms.latency[0] = 8;
  ms.latency[1] = 8;
  struct CycleT tx;
  struct CycleT ty;
  tx.N = 1;
  tx.n[0] = 4;
  tx.I[0] = 2;
  tx.b = 1;
  ty.N = 1;
  ty.n[0] = 4;
  ty.I[0] = 2;
  ty.b = 2;
  ms.cycle[0] = tx;
  ms.cycle[1] = ty;
#endif

  ms_log(&ms);

  std::chrono::_V2::system_clock::time_point stamp0, stamp1, stamp2;

  stamp0 = std::chrono::high_resolution_clock::now();
  int status = mod_skd(&ms, &skd);
  puts("GET HERE");
  stamp1 = std::chrono::high_resolution_clock::now();
  if (!status) {
    buff_opt(&ms, &skd, &buff);
  }

  stamp2 = std::chrono::high_resolution_clock::now();

  std::string outputFile;
  if (argc == 3) {
    outputFile = argv[2];
  } else {
    puts("default report path is report.json");
    outputFile = "report.json";
  }

  std::ofstream ofs(outputFile);
  json output_data;
  output_data["N"] = data["N"];
  output_data["NP"] = data["NP"];
  int sum_latency = 0, sum_depth = 0;
  for (int i = 0; i < ms.N; i++) {
    sum_latency += ms.latency[i];
  }
  for (int i = 0; i < ms.NP; i++) {
    sum_depth += ms.cycle[i].N;
  }
  output_data["sum_latency"] = sum_latency;
  output_data["sum_depth"] = sum_depth;
  output_data["mskd_duration"] =
      std::chrono::duration_cast<std::chrono::milliseconds>(stamp1 - stamp0)
          .count();
  if (!status)
    output_data["buff_duration"] =
        std::chrono::duration_cast<std::chrono::milliseconds>(stamp2 - stamp1)
            .count();
  else
    output_data["buff_duration"] = -1;
  ofs << std::setw(4) << output_data << std::endl;

  return 0;
}
#endif