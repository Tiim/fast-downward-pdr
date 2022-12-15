#include "heuristic.h"

namespace pdr_search
{
  pdr_search::PDRHeuristic::~PDRHeuristic()
  {
  }

  NoopPDRHeuristic::~NoopPDRHeuristic()
  {
  }

  Layer NoopPDRHeuristic::initial_heuristic_layer(int)
  {
    return Layer();
  }

}