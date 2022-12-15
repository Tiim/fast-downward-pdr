#ifndef SEARCH_ENGINES_PDR_SEARCH_H
#define SEARCH_ENGINES_PDR_SEARCH_H

#include "../search_engine.h"
#include "search_common.h"

#include "../pdr/data-structures.h"

#include <vector>
#include <set>
#include <memory>
#include <optional>
#include <queue>

namespace options
{
    class OptionParser;
    class Options;
}

namespace pdr_search
{

    class PDRSearch : public SearchEngine
    {
        std::vector<Layer> layers;
        int iteration = 0;

        Layer *get_layer(long unsigned int i);

        // Returns (t, true) where t is successor state
        // or (r, false) where r is reason
        std::pair<LiteralSet, bool> extend(LiteralSet s, Layer L);

    protected:
        virtual void initialize() override;
        virtual SearchStatus step() override;

    public:
        PDRSearch(const options::Options &opts);
        ~PDRSearch() = default;

        virtual void print_statistics() const override;

        void dump_search_space() const;

        // Coverts a state to a literal set as a cube
        // Same as the Lits(s) function in the paper
        LiteralSet from_state(const State &s) const;

        LiteralSet from_precondition(const PreconditionsProxy &pc) const;
        LiteralSet from_effect(const EffectsProxy &ep) const;

        LiteralSet all_variables() const;
    };

    extern void add_options_to_parser(options::OptionParser &parser);
    std::ostream &operator<<(std::ostream &os, const std::set<SetOfLiteralSets> &s);
}

#endif
