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

    class Literal
    {
    private:
        const int variable;
        const int value;
        bool positive = true;

    public:
        Literal(int variable, int value);
        bool operator<(const Literal &b) const;
        Literal invert() const;
        static Literal from_fact(FactProxy fp);
    };

    // A set of literals,
    // can represent a clause (disjunction ∨)
    // or a cube (conjunction ∧)
    class LiteralSet
    {
    private:
        const bool clause = true;    
        std::set<Literal> literals;

    public:
        LiteralSet();
        LiteralSet(Literal v);
        LiteralSet(int variable, int value);
        LiteralSet(std::set<Literal> init_literals, bool is_clause);
        bool operator<(const LiteralSet &b) const;
        std::set<Literal> get_literals() const;
        
        LiteralSet invert() const;
        size_t size() const;
        bool is_unit() const;
        bool is_clause() const;
        bool is_cube() const;

        void add_literal(Literal l);
        void remove_literal(Literal l);
        // Remove ¬l, add l
        void apply_literal(Literal l);
        bool contains_literal(Literal l) const;
        bool is_subset_eq_of(LiteralSet ls) const;

        // Returns true if this LiteralSet is a model
        // of s.
        // This means it will return false if for any literal l in
        // s the inverse literal ¬l is element of this set.
        //
        // Equivalent to the ⊧ (models) operator
        // TODO: does this depend on if both sets are clauses or cubes?
        bool models(LiteralSet s) const;

    };

    class Obligation
    {
    private:
        State state;
        int priority;

    public:
        Obligation(State s, int priority);
        int get_priority() const;
        State get_state() const;
        bool operator<(const Obligation &o) const;
    };

    class Layer
    {
    private:
        const PDRSearch *search_task;
        std::set<LiteralSet> clauses;

    public:
        Layer(const PDRSearch *search_task);
        Layer(const PDRSearch *search_task, const Layer &l);
        Layer(const PDRSearch *search_task, const std::set<LiteralSet> clauses);
        size_t size() const;
        const std::set<LiteralSet> get_clauses() const;

        // L ← L ∪ {c}
        void add_clause(LiteralSet c);
        // L ← L ∖ {c}
        void remove_clause(LiteralSet c);
        bool contains_clause(LiteralSet c) const;
        bool is_subset_eq_of(Layer l) const;
        
        // Lₜₕᵢₛ ∖ Lₗ
        Layer without(Layer *l);
        bool modeled_by(State s);
    };

    class PDRSearch : public SearchEngine
    {
        std::vector<Layer> layers;
        int iteration = 0;

        Layer get_layer(int i);

    protected:
        virtual void initialize() override;
        virtual SearchStatus step() override;

    public:
        PDRSearch(const options::Options &opts);
        ~PDRSearch() = default;

        virtual void print_statistics() const override;

        LiteralSet extend(State s, Layer L);

        void dump_search_space() const;

        // Coverts a state to a literal set as a cube
        // Same as the Lits(s) function in the paper
        LiteralSet from_state(const State &s) const;
    };

    extern void add_options_to_parser(options::OptionParser &parser);
}

#endif
