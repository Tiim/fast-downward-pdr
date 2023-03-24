#include "heuristic.h"
#include "../plugin.h"
#include <memory>

namespace pdr_search
{
  PDRHeuristic::PDRHeuristic(const options::Options &opts) : task(opts.get<std::shared_ptr<AbstractTask>>("transform")),
                                                             task_proxy(*task)
  {
  }

  pdr_search::PDRHeuristic::~PDRHeuristic()
  {
  }

  void PDRHeuristic::add_options_to_parser(OptionParser &parser)
  {
    parser.add_option<std::shared_ptr<AbstractTask>>(
        "transform",
        "Optional task transformation for the heuristic."
        " Currently, adapt_costs() and no_transform() are available.",
        "no_transform()");
  }

  NoopPDRHeuristic::NoopPDRHeuristic(const options::Options &opts) : PDRHeuristic(opts)
  {
  }

  NoopPDRHeuristic::~NoopPDRHeuristic()
  {
  }

  void NoopPDRHeuristic::initial_heuristic_layer(int, std::shared_ptr<Layer> layer)
  {
  }

  std::shared_ptr<PDRHeuristic> NoopPDRHeuristic::parse(OptionParser &parser)
  {
    parser.document_synopsis("noop for pdr search", "");

    PDRHeuristic::add_options_to_parser(parser);

    Options opts = parser.parse();

    std::shared_ptr<NoopPDRHeuristic> heuristic;
    if (!parser.dry_run())
    {
      heuristic = std::make_shared<NoopPDRHeuristic>(opts);
    }

    return heuristic;
  }

  static PluginTypePlugin<PDRHeuristic> _type_plugin("pdr-heuristic", "");
  static Plugin<PDRHeuristic> _pluginNoop("pdr-noop", NoopPDRHeuristic::parse);
}
