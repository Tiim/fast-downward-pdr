#include "pattern-database.h"
#include "../plugin.h"

namespace pdr_search
{
  PatternDBPDRHeuristic::PatternDBPDRHeuristic(const options::Options &opts) : PDRHeuristic(opts)
  {

    std::shared_ptr<pdbs::PatternGenerator> pattern_generator =
        opts.get<std::shared_ptr<pdbs::PatternGenerator>>("pattern");
    

    pdbs::PatternInformation pattern_info = pattern_generator->generate(task);

    pattern_database = pattern_info.get_pdb();
    
    auto pattern = pattern_database->get_pattern();
    auto variables = TaskProxy(*task).get_variables();

    std::cout << "Pattern size: " << pattern.size() << ", Variables: " << variables.size() << std::endl;
  }

  Layer pdr_search::PatternDBPDRHeuristic::initial_heuristic_layer(int i)
  {
    auto task_proxy = TaskProxy(*task);
    auto pattern = pattern_database->get_pattern();
    auto variables = task_proxy.get_variables();
    std::set<LiteralSet> states;
    std::vector<int> current_state(variables.size()); // is initialized with 0 values

    // first variable in pattern with -1 so we can increment it to 0 in the next loop
    current_state[pattern[0]] = -1;

    while (true)
    {
      // increment first pattern value
      current_state[pattern[0]] += 1;
      bool done = false;
      // propagate value to next pattern values if it overflows.
      for (size_t j = 0; j < pattern.size() - 1; j += 1)
      {
        if (current_state[pattern[j]] >= variables[pattern[j]].get_domain_size())
        {
          current_state[pattern[j]] = 0;
          current_state[pattern[j + 1]] += 1;
          // if we reached the last variable in the pattern and it is already bigger than the domain
          if (j == pattern.size() - 2 && pattern[pattern.size() - 1] >= variables[pattern.size() - 1].get_domain_size())
          {
            done = true;
            break;
          }
        }
        else
        {
          break;
        }
      }
      if (done)
      {
        break;
      }

      // Get the heuristic distance.
      // Since the heuristic is admissible, the heuristic distance is always smaller or equal to the 
      // real distance.
      int dist = pattern_database->get_value(current_state);
      // If the heuristic distance is <= than the current layer number i,
      // the goal can be possibly reached from current_state in i steps.
      if (dist > i)
      {
        // Strengthen the layer such that the abstract state of 'current_state'
        // can not model the layer.
        LiteralSet ls = LiteralSet(SetType::CLAUSE);

        for (size_t j = 0; j < pattern.size(); j += 1)
        {
          Literal l = Literal::from_fact(FactProxy(*task, pattern[j], current_state[pattern[j]]));
          
          // TODO: Layers should consist of non negative literals only. (Why?)
          ls.add_literal(l.invert());
        }

        states.insert(states.end(), ls);
      }
    }
    return Layer(std::set<LiteralSet>(states));
  }

  std::shared_ptr<PDRHeuristic> PatternDBPDRHeuristic::parse(OptionParser &parser)
  {
    parser.document_synopsis("pattern database heuristic for pdr search", "");
    parser.add_option<std::shared_ptr<pdbs::PatternGenerator>>(
        "pattern",
        "pattern generation method",
        "greedy()");
    PDRHeuristic::add_options_to_parser(parser);
    
    Options opts = parser.parse();

    std::shared_ptr<PatternDBPDRHeuristic> heuristic;
    if (!parser.dry_run())
    {
      heuristic = std::make_shared<PatternDBPDRHeuristic>(opts);
    }

    return heuristic;
  }

  static Plugin<PDRHeuristic> _pluginPDB("pdr-pdb", PatternDBPDRHeuristic::parse);

} // namespace pdr_search