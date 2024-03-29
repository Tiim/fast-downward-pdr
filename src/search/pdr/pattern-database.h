#ifndef PDR_PDB_H
#define PDR_PDB_H

#include "heuristic.h"
#include "../pdbs/pattern_database.h"
#include "../pdbs/pattern_information.h"
#include "../pdbs/pattern_generator.h"
#include "../option_parser.h"
#include <memory>

namespace pdr_search
{

  class PatternDBPDRHeuristic : public PDRHeuristic
  {
    private:
      std::shared_ptr<pdbs::PatternDatabase> pattern_database;

    public:
      PatternDBPDRHeuristic(const options::Options &opts);
      void initial_heuristic_layer(int i, std::shared_ptr<Layer> layer);

      LiteralSet from_projected_state(pdbs::Pattern pattern, std::vector<int> state);

      static std::shared_ptr<PDRHeuristic> parse(OptionParser &parser);
  };
}
#endif
