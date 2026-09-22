
#include <stdio.h>
#include <math.h>
#include <string.h>
#include "nd_quant.h"
static float sigmoidf_(float x){ if (x>=0.0f){ float e=nd_expf(-x); return 1.0f/(1.0f+e);} { float e=nd_expf(x); return e/(1.0f+e);} }
static inline void sigmoidf_pair(float x0,float x1,float*y0,float*y1){
  if((x0>=0.0f)==(x1>=0.0f)){ float e0,e1;
    nd_expf_pair(x0>=0.0f?-x0:x0, x1>=0.0f?-x1:x1,&e0,&e1);
    if(x0>=0.0f){*y0=1.0f/(1.0f+e0);*y1=1.0f/(1.0f+e1);} else {*y0=e0/(1.0f+e0);*y1=e1/(1.0f+e1);} }
  else {*y0=sigmoidf_(x0);*y1=sigmoidf_(x1);} }
static int bits(float f){unsigned u;memcpy(&u,&f,4);return (int)u;}
int main(void){
  long bad=0,n=0; double worst=0;
  /* dense sweep over the range SiLU/attention gates actually produce, plus edges */
  for(long i=-400000;i<=400000;i++){
    float x0=(float)i*0.001f, x1=x0+0.37f;
    float a,b; sigmoidf_pair(x0,x1,&a,&b);
    float e0=sigmoidf_(x0), e1=sigmoidf_(x1);
    n+=2;
    if(bits(a)!=bits(e0)||bits(b)!=bits(e1)){ bad++; if(bad<4) printf("MISMATCH x0=%g x1=%g got %g/%g want %g/%g\n",x0,x1,a,b,e0,e1); }
    double d=fabs((double)a-e0); if(d>worst) worst=d;
  }
  /* exact branch boundary and non-finite/edge arguments */
  float edge[]={0.0f,-0.0f,1e-45f,-1e-45f,88.0f,-88.0f,87.99f,-88.01f,1e30f,-1e30f};
  const unsigned NEDGE = sizeof(edge)/sizeof(edge[0]);
  for(unsigned i=0;i<NEDGE;i++) for(unsigned j=0;j<NEDGE;j++){
    float a,b; sigmoidf_pair(edge[i],edge[j],&a,&b);
    float e0=sigmoidf_(edge[i]), e1=sigmoidf_(edge[j]);
    n+=2; if(memcmp(&a,&e0,4)||memcmp(&b,&e1,4)){bad++;printf("EDGE MISMATCH %g %g -> %g/%g want %g/%g\n",edge[i],edge[j],a,b,e0,e1);} }
  printf("comparisons=%ld bit_mismatches=%ld max_abs=%.3e\n",n,bad,worst);
  return bad!=0;
}
