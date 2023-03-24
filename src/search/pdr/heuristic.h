#ifndef PDR_HEURISTIC_H
#define PDR_HEURISTIC_H

#include "data-structures.h"
#include "../options/options.h"
#include "../option_parser.h"
#include <memory>

namespace options
{
  class OptionParser;
  class Options;
}

namespace pdr_search
{

  class PDRHeuristic
  {
  protected:
    // Hold a reference to the task implementation and pass it to objects that need it.
    const std::shared_ptr<AbstractTask> task;
    // Use task_proxy to access task information.
    TaskProxy task_proxy;

  public:
    PDRHeuristic(const options::Options &opts);
    virtual ~PDRHeuristic();
    virtual void initial_heuristic_layer(int i, std::shared_ptr<Layer> layer) = 0;

    static void add_options_to_parser(OptionParser &parser);
  };

  class NoopPDRHeuristic : public PDRHeuristic
  {
  public:
    NoopPDRHeuristic(const options::Options &opts);
    ~NoopPDRHeuristic();
    virtual void initial_heuristic_layer(int i, std::shared_ptr<Layer> layer);
    static std::shared_ptr<PDRHeuristic> parse(OptionParser &parser);
  };

}
#endif
