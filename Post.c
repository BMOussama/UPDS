#include <stdio.h>
#include "Post.h"

Fa* PostStar(Fa* fa, Upds* upds){
  UpdsStatus status;
  Fa* result = SaturatePost(fa, upds, UPDS_DEFAULT_CAP, &status);
  if(status == UPDS_OVER)
    fprintf(stderr, "PostStar: widening used, the result is an over-approximation of post*\n");
  return result;
}
