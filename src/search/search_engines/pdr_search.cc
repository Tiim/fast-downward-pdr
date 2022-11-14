#include "pdr_search.h"

#include "../option_parser.h"

#include "../utils/logging.h"

#include <cassert>
#include <cstdlib>
#include <memory>
#include <optional.hh>
#include <set>

using namespace std;

namespace pdr_search {

PDRSearch::PDRSearch(const Options &opts): SearchEngine(opts) {
}

void PDRSearch::initialize() {
    log << "Initializing PDR Search";
}

void PDRSearch::print_statistics() const {
    statistics.print_detailed_statistics();
    search_space.print_statistics();
}

SearchStatus PDRSearch::step() {
    log << "Step of PDR search";

    return SearchStatus::FAILED;
}

void add_options_to_parser(OptionParser &parser) {
    SearchEngine::add_options_to_parser(parser);
}
}
