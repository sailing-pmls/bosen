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
#include "getcorr.hpp"
#include "./include/utility.hpp"
#include <unordered_map>

using namespace std;

// assume that rows in column vector is fully sorted on idx and has sorted vecotr of value corresponding to the sorted idx list
double getcorr_pair_sparse_vmat(spmat_vector &xcol, spmat_vector &ycol, uint64_t nsamples, double pxsum, double pxsqsum, double pysum, double pysqsum, int avail, uint64_t xcol_id, uint64_t ycol_id){

  strads_msg(INF, "sparse vmat with vector cals, xcol.id(%ld) size (%ld) ycol.id(%ld) size(%ld)\n", 
	     xcol_id, xcol.idx.size(), ycol_id, ycol.idx.size());

#if 0 
  for(uint64_t i=0; i < xcol.size(); i++ ){
    strads_msg(INF, " col (%ld) row(%ld) value (%lf)\n", 
	       xcol_id, xcol.idx[i], xcol.val[i]);
  }
  for(uint64_t i=0; i < ycol.size(); i++ ){
    strads_msg(INF, " col (%ld) row(%ld) value (%lf)\n", 
	       ycol_id, ycol.idx[i], ycol.val[i]);
  }
#endif 

  double xysum=0, xsum=0, xsqsum=0, ysum=0, ysqsum=0;
  double corrcoef=0;

  assert(xcol.idx.size() != 0); // there should be no column that has zero entries in rows 
  assert(ycol.idx.size() != 0); // If this is triggers, there might be error in 
  assert(xcol.val.size() == xcol.idx.size());
  assert(ycol.val.size() == ycol.idx.size());


  for(uint64_t id=0; id < xcol.idx.size(); id++){
    xsum += xcol.val[id];
    xsqsum += (xcol.val[id]*xcol.val[id]);
  }

  for(uint64_t id=0; id < ycol.idx.size(); id++){
    ysum += ycol.val[id];
    ysqsum += (ycol.val[id]*ycol.val[id]);
  }

  uint64_t xsize = xcol.idx.size();
  uint64_t ysize = ycol.idx.size();
  uint64_t xprogress = 0;
  uint64_t yprogress = 0;

  while(xprogress != xsize and yprogress != ysize){
    if(xcol.idx[xprogress] == ycol.idx[yprogress]){
      xysum += xcol.val[xprogress]*ycol.val[yprogress];
      xprogress++;
      yprogress++;

      if(xprogress == xsize)
	break;

      if(yprogress == ysize)
	break;
    }else{
      if(xcol.idx[xprogress] > ycol.idx[yprogress]){
	yprogress++;
	if(yprogress == ysize)
	  break;
      }else{
	xprogress++;
	if(xprogress == xsize)
	  break;
      }
    }
  }

#if 0 
  if(xysum != 0){
    strads_msg(INF, " col(%ld) col(%ld) hits \n", xcol_id, ycol_id);
  }
#endif 

  double divide = (sqrt(nsamples*xsqsum - (xsum)*(xsum))*(sqrt(nsamples*ysqsum - (ysum)*(ysum))));
  if(divide == 0.0){
    strads_msg(OUT, "##### nsamples(%ld) xsqsum(%2.20lf) xsum(%2.20lf) ysqsum(%2.20lf) ysum(%2.20lf)\n", 
	       nsamples, xsqsum, xsum, ysqsum, ysum);
    strads_msg(OUT, "##### nsamples*xsqsum(%2.20lf) xsum*xsum(%2.20lf) nsamples*ysqsum(%2.20lf) ysum*ysum(%2.20lf)\n", 
	       nsamples*xsqsum, xsum*xsum, nsamples*ysqsum, ysum*ysum);
    strads_msg(OUT, "##### sqrt(..) %2.20lf sqrt(..) %2.20lf\n", 
	       sqrt(nsamples*xsqsum - (xsum)*(xsum)),  
	       sqrt(nsamples*ysqsum - (ysum)*(ysum)));	           
    for(uint64_t i = 0; i < xcol.idx.size(); i++){
      strads_msg(OUT, " xcolid (%ld) Xrow[%ld] = %lf\n", xcol_id, xcol.idx[i], xcol.val[i]);
    }
    for(uint64_t i = 0; i < ycol.idx.size(); i++){
      strads_msg(OUT, " ycolid (%ld) Yrow[%ld] = %lf\n", ycol_id, ycol.idx[i], ycol.val[i]);
    }
  }

  assert(divide != 0.0);
  corrcoef = 1.0*(nsamples*xysum-(xsum*ysum))/divide;
  return fabs(corrcoef);
}

//double getcorr_pair_sparsem(unordered_map<long unsigned int, double> &xcol, unordered_map<long unsigned int, double> &ycol, uint64_t nsamples, double pxsum, double pxsqsum, double pysum, double pysqsum, int avail)
double getcorr_pair_sparsem(unordered_map<long unsigned int, double> &xcol, unordered_map<long unsigned int, double> &ycol, uint64_t nsamples, double pxsum, double pxsqsum, double pysum, double pysqsum, int avail){

  double xysum=0, xsum=0, xsqsum=0, ysum=0, ysqsum=0;
  double corrcoef=0;
  unordered_map<long unsigned int, double>::iterator  xiter = xcol.begin();
  unordered_map<long unsigned int, double>::iterator  yiter = ycol.begin();

  assert(xcol.size() != 0); // there should be no column that has zero entries in rows 
  assert(ycol.size() != 0); // If this is triggers, there might be error in 

#if 1
  for(; xiter != xcol.end(); xiter++){
    strads_msg(INF, "X[%ld] = %lf\n", xiter->first, xiter->second);
    xsum += xiter->second;
    xsqsum += ((xiter->second)*(xiter->second));
  }

  for(; yiter != ycol.end(); yiter++){
    strads_msg(INF, "Y[%ld] = %lf\n", yiter->first, yiter->second);
    ysum += yiter->second;
    ysqsum += ((yiter->second)*(yiter->second));
  }

  xiter = xcol.begin();
  yiter = ycol.begin();

  //  bool hitflag = false;
  while(xiter != xcol.end() && yiter != ycol.end()){
    if(xiter->first == yiter->first){
      xysum += ((xiter->second)*(yiter->second));
      //      strads_msg(ERR, "hit at %ld th row .... \n", xiter->first);
      //      hitflag = true;
      xiter++;
      yiter++;
      if(xiter == xcol.end())
	break;
      if(yiter == ycol.end())
	break;
    }else{
      if(xiter->first > yiter->first){
	yiter++;
	if(yiter == ycol.end())
	  break;
      }else{
	xiter++;
	if(xiter == xcol.end())
	  break;
      }
    }
  }
#endif 

  double divide = (sqrt(nsamples*xsqsum - (xsum)*(xsum))*(sqrt(nsamples*ysqsum - (ysum)*(ysum))));
  if(divide == 0.0){
    strads_msg(ERR, "##### nsamples(%ld) xsqsum(%2.20lf) xsum(%2.20lf) ysqsum(%2.20lf) ysum(%2.20lf)\n", 
	       nsamples, xsqsum, xsum, ysqsum, ysum);

    strads_msg(ERR, "##### nsamples*xsqsum(%2.20lf) xsum*xsum(%2.20lf) nsamples*ysqsum(%2.20lf) ysum*ysum(%2.20lf)\n", 
	       nsamples*xsqsum, xsum*xsum, nsamples*ysqsum, ysum*ysum);

    strads_msg(ERR, "##### sqrt(..) %2.20lf sqrt(..) %2.20lf\n", 
	       sqrt(nsamples*xsqsum - (xsum)*(xsum)),  
	       sqrt(nsamples*ysqsum - (ysum)*(ysum)));	          

    xiter = xcol.begin();
    yiter = ycol.begin();
  
    for(; xiter != xcol.end(); xiter++){
      strads_msg(ERR, "X[%ld] = %lf\n", xiter->first, xiter->second);
    }

    for(; yiter != ycol.end(); yiter++){
      strads_msg(ERR, "Y[%ld] = %lf\n", yiter->first, yiter->second);
    }
  }

  assert(divide != 0.0);
  corrcoef = 1.0*(nsamples*xysum-(xsum*ysum))/divide;

  return fabs(corrcoef);
}
