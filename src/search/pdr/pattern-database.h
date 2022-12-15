#ifndef PDR_PDB_H
#define PDR_PDB_H

#include "heuristic.h"
#include "../pdbs/pattern_database.h"

namespace pdr_search
{

  class PatternDBPDRHeuristic : public PDRHeuristic
  {
    private:
    
    const pdbs::PatternDatabase *pattern_database;

    public:
    PatternDBPDRHeuristic(const pdbs::PatternDatabase &pdb);
    Layer initial_heuristic_layer(int i);
  };
}
#endif
