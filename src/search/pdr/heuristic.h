#ifndef PDR_HEURISTIC_H
#define PDR_HEURISTIC_H

#include "data-structures.h"

namespace pdr_search
{

class PDRHeuristic
{
  public:
  virtual ~PDRHeuristic();
  virtual Layer initial_heuristic_layer(int i) = 0;
};

class NoopPDRHeuristic : public PDRHeuristic
{
  public:
  ~NoopPDRHeuristic();
  Layer initial_heuristic_layer(int i);
};


}
#endif
