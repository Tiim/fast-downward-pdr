#ifndef SEARCH_ENGINES_PDR_SEARCH_H
#define SEARCH_ENGINES_PDR_SEARCH_H

#include "../search_engine.h"
#include "search_common.h"

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

    class Variable
    {
        const int variable;
        const int value;
        bool positive = true;
    };


    // only represent unit clause for now
    class Clause
    {
    private:
        const int variable;
        const int value;
        bool positive = true;

    public:
        Clause(int variable, int value);
        bool operator<(const Clause &b) const;

        Clause invert() const;

        static Clause from_fact(FactProxy fp);
        static std::set<Clause> from_state(const State &s);
    };

    class Obligation
    {
    private:
        State state;
        int priority;

    public:
        int get_priority() const;
        State get_state() const;
        Obligation(State s, int priority);
        bool operator<(const Obligation &o) const;
    };

    class Layer
    {
    private:
        std::set<Clause> clauses_set;

    public:
        Layer(const std::set<Clause> clauses);
        Layer(const Layer &l);
        Layer();
        ~Layer();

        // Lᵢ ← Lᵢ ∪ {c}
        void add_clause(Clause c);
        // add the clause c to the set, but removes clause ¬c
        // Lᵢ ← (Lᵢ ∖ {¬ c}) ∪ {c}
        void apply_clause(Clause c);
        void remove_clause(Clause c);
        bool contains_clause(Clause c) const;
        bool is_subset_eq_of(Layer l) const;
        std::set<Clause> get_clauses();
        // Lᵢ ∖ Lₗ
        Layer without(Layer *l);
        bool models(State s);
        size_t size() const;
    };

    class PDRSearch : public SearchEngine
    {
        std::vector<Layer> layers;
        int iteration = 0;

    protected:
        virtual void initialize() override;
        virtual SearchStatus step() override;

    public:
        PDRSearch(const options::Options &opts);
        ~PDRSearch() = default;

        virtual void print_statistics() const override;

        Layer extend(State s, Layer L);
        void dump_search_space() const;
    };

    extern void add_options_to_parser(options::OptionParser &parser);
}

#endif
