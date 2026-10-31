#include <stdio.h>
#include "Pre.h"

Fa* PreStar(Fa* fa, Upds* upds){
  UpdsStatus status;
  Fa* result = SaturatePre(fa, upds, UPDS_DEFAULT_CAP, &status);
  if(status == UPDS_OVER)
    fprintf(stderr, "PreStar: widening used, the result is an over-approximation of pre*\n");
  return result;
}
