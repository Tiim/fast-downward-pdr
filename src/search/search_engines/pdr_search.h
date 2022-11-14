#ifndef SEARCH_ENGINES_PDR_SEARCH_H
#define SEARCH_ENGINES_PDR_SEARCH_H

#include "../search_engine.h"

#include <memory>
#include <vector>

namespace options {
class OptionParser;
class Options;
}

namespace pdr_search {
class PDRSearch : public SearchEngine {

protected:
    virtual void initialize() override;
    virtual SearchStatus step() override;

public:
    PDRSearch(const options::Options &opts);
    ~PDRSearch() = default;

    virtual void print_statistics() const override;

    void dump_search_space() const;
};

extern void add_options_to_parser(options::OptionParser &parser);
}

#endif
