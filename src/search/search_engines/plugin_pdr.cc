#include "pdr_search.h"
#include "search_common.h"

#include "../option_parser.h"
#include "../plugin.h"

using namespace std;

namespace plugin_pdr {
static shared_ptr<SearchEngine> _parse(OptionParser &parser) {
    parser.document_synopsis("property-directed reachability search", "");

    pdr_search::add_options_to_parser(parser);
    Options opts = parser.parse();

    shared_ptr<pdr_search::PDRSearch> engine;
    if (!parser.dry_run()) {
        engine = make_shared<pdr_search::PDRSearch>(opts);
    }

    return engine;
}

static Plugin<SearchEngine> _plugin("pdr", _parse);
}
