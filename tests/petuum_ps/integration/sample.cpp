// Copyright (c) 2014, Sailing Lab
// All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are met:
//
// 1. Redistributions of source code must retain the above copyright notice,
// this list of conditions and the following disclaimer.
//
// 2. Redistributions in binary form must reproduce the above copyright
// notice, this list of conditions and the following disclaimer in the
// documentation and/or other materials provided with the distribution.
//
// 3. Neither the name of the <ORGANIZATION> nor the names of its contributors
// may be used to endorse or promote products derived from this software
// without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
// AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
// IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
// ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
// LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
// CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
// SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
// INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
// CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
// ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
// POSSIBILITY OF SUCH DAMAGE.
/*
 * This is a trivial program used to demonstrate the usage of petuum_ps.
 * author: Jinliang
 *
 * Description:
 * This program creates nthrs number of threads. Each thread runs niters number 
 * of iterations. All threads share one Table. An Algo instance contains the 
 * computation to be done.
 * In each iteration, a thread does the following things in pseudocode:
 * 1) call Algo::GetNumUpdates() to get the number of cells to update
 * (n_updates);
 * 
 * Do 2), 3) and 4) for n_updates number of iterations
 *
 * 2) call Algo::GetCellIDs(), which returns a vector containing 
 * the row ID and column ID of the cell to be updated;
 * 3) call Algo::GetUpdate(), wihch returns the update to be applied;
 * 4) Apply the update
 *
 * 5) call Algo::GetSummaryCellIDs() to get a cell ID
 * 6) value of the cell got in last step is used to call Algo::Update() 
 * to update Algo instance
 *
 * Compilation note:
 * 1) Include "<ROOT_DIR>/src" in your include path
 * 2) Using pthread, but boost thread or any other thread library implemented 
 * based on pthread should work.
 *  
 */

#include <petuum_ps/petuum_ps.hpp>
#include <vector>
#include <pthread.h>

// Petuum PS supports arbitrary POD type for its value
typedef double ValueType; 

// Petuum PS supports arbitrary POD types for table ID
typedef int32_t TableID;

/* implementation of Algo goes somewhere else */
class Algo {
private:

  /* private members of Algo*/
  int num;
public:
  Algo(int32_t _thrid);
  // Get the number of updates to be applied per iteration
  int32_t GetNumUpdates(); 
  std::vector<int32> GetCellIDs();
  double GetUpdate();
  std::vector<int32> GetSummaryCellIDs();
  int32_t Update(double _v);
};

struct ThrInfo{
  int32_t thrid_;
  int32_t niters_; 

  petuum::TableGroup<TableID, ValueType> &tg_; 
  // it's better to pass the Table object around as reference type
  // or pointer
  petuum::Table<ValueType> &tb_; 
 
};

void *pthread_main(void *_thrinfo){
  int32_t niters = ((ThrInfo *) thrinfo)->niters_;
  int i, j;
  petuum::Table<ValueType> &tb = ((ThrInfo *) thrinfo)->tb_;
  petuum::TableGroup<TableID, ValueType> &tg = ((ThrInfo *) thrinfo)->tg_;

  Algo algo(((ThrInfo *) thrinfo)->thrid_);

  for(i = 0; i < niters; ++i){
    int32_t nupdates = algo.GetNumUpdates();
    for(j = 0; j < nupdates; ++j){
      std::vector<int32_t> ids = algo.GetCellIDs();
      double update = algo.GetUpdate();
      tb.Inc(ids[0], ids[1], update);
    }
    std::vector<int32> ids = algo.GetSummaryCellIDs();
    double v = tb.Get(ids[0], ids[1]);
    algo.Update(v);    
    tg.Iterate();
  }
  return 0;
}


int main(int argc, char *argv[]){
  
  int32_t nthrs;
  int32_t niters;
  
  /* some code to initialize nthrs and niters */
  
  // right now, use default constructor for TableGroupConfig
  // is good enough
  petuum::TableGroupConfig tgconfig; 

  petuum::TableGroup<TableID, ValueType> tg 
    = petuum::TableGroup<TableID, ValueType>::GreateTableGroup(tgconfig);

  petuum::TableConfig tbconfig;
  //TODO(Jinliang/Wei Dai): set staleness in TableConfig
  
  petuum::Table<ValueType> tb = tg.CreateTable(tbconfig);

  pthread_t thrs = new pthread_t[nthrs];
  ThrInfo thrinfo = new ThrInfo[nthrs];

  /* some code to initialize thrinfo objects */

  int32_t i;
  for(i = 0; i < nthrs; ++i){
    pthread_create(thrs + i, NULL, pthread_main, thrinfo + i);
  }
  
  for(i = 0; i < nthrs; ++i){
    pthread_join(thrs[i], NULL);
  }

  delete[] thrs;
  delete[] thrinfo;
  
  return 0;
}
