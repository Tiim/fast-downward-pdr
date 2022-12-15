#include "pattern-database.h"

namespace pdr_search
{
  PatternDBPDRHeuristic::PatternDBPDRHeuristic(const pdbs::PatternDatabase &pdb) : pattern_database(&pdb)
  {
  }

  Layer pdr_search::PatternDBPDRHeuristic::initial_heuristic_layer(int i)
  {

    //TODO:
    // - find all abstract states with h(s) > i
    // - convert them to clauses
    // - initialize layer with those clauses

    return Layer();
  }

} // namespace pdr_search