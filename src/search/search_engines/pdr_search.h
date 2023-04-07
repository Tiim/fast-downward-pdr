#ifndef SEARCH_ENGINES_PDR_SEARCH_H
#define SEARCH_ENGINES_PDR_SEARCH_H

#include "../search_engine.h"
#include "search_common.h"

#include "../pdr/data-structures.h"
#include "../pdr/heuristic.h"

#include <cstddef>
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

    void printLayers(std::vector<std::shared_ptr<Layer>> layers);
    
    class PDRSearch : public SearchEngine
    {
        bool enable_obligation_rescheduling = true;
        bool enable_layer_simplification = false;

        std::shared_ptr<PDRHeuristic> heuristic;
        std::vector<std::shared_ptr<Layer>> layers;
        std::vector<std::size_t> seeded_layers_size;
        utils::Timer seeding_time = utils::Timer(false);
        utils::Timer extend_time = utils::Timer(false);
        utils::Timer path_construction_time = utils::Timer(false);
        utils::Timer clause_propagation_time = utils::Timer(false);
        std::size_t obligation_expansions = 0;


        int iteration = 0;

        std::shared_ptr<Layer> get_layer(long unsigned int i);

        // Returns (t, true) where t is successor state
        // or (r, false) where r is reason
        std::pair<LiteralSet, bool> extend(LiteralSet s, Layer L);

    protected:
        virtual void initialize() override;
        virtual SearchStatus step() override;

        void extract_path(const std::shared_ptr<Obligation> goal_obligation, const LiteralSet initialState);

    public:
        PDRSearch(const options::Options &opts);
        ~PDRSearch();

        virtual void print_statistics() const override;

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
