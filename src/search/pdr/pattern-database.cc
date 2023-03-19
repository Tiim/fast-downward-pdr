#include "pattern-database.h"
#include "../plugin.h"
#include <memory>

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

  std::shared_ptr<Layer> pdr_search::PatternDBPDRHeuristic::initial_heuristic_layer(int i, std::shared_ptr<Layer> parent)
  {
    auto task_proxy = TaskProxy(*task);
    auto pattern = pattern_database->get_pattern();
    auto variables = task_proxy.get_variables();
    std::set<LiteralSet> clauses;
    std::vector<int> current_state(variables.size()); // is initialized with 0 values

    // first variable in pattern with -1 so we can increment it to 0 in the loop
    current_state[pattern[0]] = -1;
    bool done = false;

    while (!done)
    {
      // increment first pattern value
      current_state[pattern[0]] += 1;
      // propagate value to next pattern values if it overflows.
      for (size_t ptrnidx = 0; ptrnidx < pattern.size(); ptrnidx += 1)
      {
        if (current_state[pattern[ptrnidx]] == variables[pattern[ptrnidx]].get_domain_size())
        {
          if (ptrnidx >= pattern.size() -1) {
            done = true;
            break;
          }
          current_state[pattern[ptrnidx]] = 0;
          current_state[pattern[ptrnidx + 1]] += 1;
        }
        else
        {
          break;
        }
      }
      if (done) {
        break;
      }

      //std::cout << "State: ";
      //for (size_t i=0; i < current_state.size(); i+=1) {
      //  std::cout << "("<< i << "," << current_state[i] << ")";
      //}
      //std::cout << std::endl;

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
        LiteralSet ls = from_projected_state(pattern, current_state);
        clauses.insert(ls.invert());
      }
    }
    return std::shared_ptr<Layer>(new Layer(std::set<LiteralSet>(clauses), parent));
  }

  LiteralSet PatternDBPDRHeuristic::from_projected_state(pdbs::Pattern pattern, std::vector<int> state)
  {
    LiteralSet positives = LiteralSet(SetType::CUBE);
    LiteralSet negatives = LiteralSet(SetType::CUBE);
    for (int varidx : pattern)
    {
      assert(varidx >= 0);
      assert(state[varidx] < task_proxy.get_variables()[varidx].get_domain_size());
      Literal l = Literal::from_fact(FactProxy(*task, varidx, state[varidx]));
      positives.add_literal(l);
    }
    auto vars = this->task_proxy.get_variables();
    for (auto varidx : pattern)
    {
      int dom_size = vars[varidx].get_domain_size();
      for (int i = 0; i < dom_size; i++)
      {
        Literal l = Literal::from_fact(FactProxy(*task, varidx, i));
        if (!positives.contains_literal(l))
        {
          negatives.add_literal(l.invert());
        }
      }
    }
    return negatives;
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
