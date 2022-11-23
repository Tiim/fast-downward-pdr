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
    class Literal;
    class LiteralSet;
    class Obligation;
    class Layer;

    class Literal
    {
    private:
        const int variable;
        const int value;
        const bool positive = true;

    public:
        Literal(int variable, int value);
        Literal(int variable, int value, bool positive);
        Literal(const Literal &l);
        Literal operator=(const Literal &l) const;
        bool operator==(const Literal &l) const;
        bool operator<(const Literal &b) const;
        friend std::ostream &operator<<(std::ostream &os, const Literal &l);
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
        LiteralSet(const LiteralSet &s);
        LiteralSet(std::set<Literal> init_literals, bool is_clause);
        LiteralSet operator=(const LiteralSet &s) const;
        bool operator==(const LiteralSet &s) const;
        bool operator<(const LiteralSet &s) const;
        friend std::ostream &operator<<(std::ostream &os, const LiteralSet &ls);
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
        bool is_subset_eq_of(const LiteralSet &ls) const;
        LiteralSet set_union(const LiteralSet &s) const;
        LiteralSet set_intersect(const LiteralSet &s) const;

        // Returns true if this LiteralSet is a model
        // of s.
        //
        // should only be called for state s and clause c
        // with "s ⊧ c".
        bool models(const LiteralSet &c) const;
        // returns true if this literal set models every clause in the layer
        bool models(const Layer &l) const;
    };

    class Obligation
    {
    private:
        const LiteralSet state;
        const int priority;

    public:
        Obligation(LiteralSet s, int priority);
        Obligation(const Obligation &o);
        Obligation operator=(const Obligation &o) const;
        friend std::ostream &operator<<(std::ostream &os, const Obligation &o);
        int get_priority() const;
        LiteralSet get_state() const;
        bool operator<(const Obligation &o) const;
    };

    class Layer
    {
    private:
        std::set<LiteralSet> clauses;

    public:
        Layer();
        Layer(const Layer &l);
        Layer(const std::set<LiteralSet> clauses);
        bool operator==(const Layer &l) const;
        bool operator<(const Layer &l) const;
        friend std::ostream &operator<<(std::ostream &os, const Layer &l);
        size_t size() const;
        const std::set<LiteralSet> get_clauses() const;

        // L ← L ∪ {c}
        void add_clause(LiteralSet c);
        // L ← L ∖ {c}
        void remove_clause(LiteralSet c);
        bool contains_clause(LiteralSet c) const;
        bool is_subset_eq_of(Layer l) const;
        // Lₜₕᵢₛ ∖ Lₗ
        Layer set_minus(const Layer &l) const;
    };

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
    };

    extern void add_options_to_parser(options::OptionParser &parser);
}

#endif
