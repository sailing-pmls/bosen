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
#include "utility.hpp"

#if 0 
#include <gsl/gsl_linalg.h>
#include <gsl/gsl_blas.h>
#include <gsl/gsl_math.h>
#include <gsl/gsl_eigen.h>
#endif 

#include <map>
#include "spmat.hpp"

using namespace std;

/* calculate Pearson correlation coeff of x, y coordinate of n samples. 
 * If avail flag == 1, Parameter xsum, xsqsum, double ysum, double ysqsum should contain 
 * right values
 * If avail flage != 1, the function calculate params on the fly 
 */
double getcorr_pair(double *x, double *y, int nsamples, double pxsum, double pxsqsum, double pysum, double pysqsum, int avail){
  int i;
  double xysum=0, xsum=0, xsqsum=0, ysum=0, ysqsum=0;
  double corrcoef=0;
  
  if(avail == 1){
    for(i=0; i<nsamples; i++){
      xysum  += x[i]*y[i];
    }    
    xsum = pxsum;
    ysum = pysum;
    xsqsum = pxsqsum;
    ysqsum = pysqsum;

  }else{    
    for(i=0; i<nsamples; i++){
      xysum  += x[i]*y[i];      
      xsqsum += x[i]*x[i];
      xsum   += x[i];
      ysqsum += y[i]*y[i];
      ysum   += y[i];
    }
  }

  corrcoef = 1.0*(nsamples*xysum-(xsum*ysum))/(sqrt(nsamples*xsqsum - (xsum)*(xsum))*(sqrt(nsamples*ysqsum - (ysum)*(ysum))));
  return fabs(corrcoef);
}


double getcorr_pair_sparsem(map<long unsigned int, double> &xcol, map<long unsigned int, double> &ycol, uint64_t nsamples, double pxsum, double pxsqsum, double pysum, double pysqsum, int avail){
  double xysum=0, xsum=0, xsqsum=0, ysum=0, ysqsum=0;
  double corrcoef=0;
  map<long unsigned int, double>::iterator  xiter = xcol.begin();
  map<long unsigned int, double>::iterator  yiter = ycol.begin();

  assert(xcol.size() != 0); // there should be no column that has zero entries in rows 
  assert(ycol.size() != 0); // If this is triggers, there might be error in 

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

  while(xiter != xcol.end() && yiter != ycol.end()){
    if(xiter->first == yiter->first){
      xysum += ((xiter->second)*(yiter->second));
      strads_msg(INF, "hit at %ld th row .... \n", xiter->first);
     
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

  double divide = (sqrt(nsamples*xsqsum - (xsum)*(xsum))*(sqrt(nsamples*ysqsum - (ysum)*(ysum))));
  if(divide == 0.0){
    strads_msg(ERR, "##### nsamples(%2.20ld) xsqsum(%2.20lf) xsum(%2.20lf) ysqsum(%2.20lf) ysum(%2.20lf)\n", 
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
