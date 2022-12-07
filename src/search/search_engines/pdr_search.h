#ifndef SEARCH_ENGINES_PDR_SEARCH_H
#define SEARCH_ENGINES_PDR_SEARCH_H

#include "../search_engine.h"
#include "search_common.h"

#include <vector>
#include <set>
#include <memory>
#include <optional>
#include <queue>

#define COLOR_NONE "\033[0m"
#define COLOR_RED "\033[0;31m"
#define COLOR_GREEN "\033[0;32m"
#define COLOR_CYAN "\033[0;36m"

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
    class SetOfLiteralSets;
    class Layer;

    enum SetType {
        CUBE,
        CLAUSE,
    };

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
        Literal neg() const;
        Literal pos() const;
        static Literal from_fact(FactProxy fp);
    };

    // A set of literals,
    // can represent a clause (disjunction ∨)
    // or a cube (conjunction ∧)
    class LiteralSet
    {
    private:
        SetType set_type;
        std::set<Literal> literals;

    public:
        LiteralSet(SetType type);
        LiteralSet(Literal v, SetType type);
        LiteralSet(const LiteralSet &s);
        LiteralSet(std::set<Literal> init_literals, SetType type);
        LiteralSet& operator=(const LiteralSet &s);
        bool operator==(const LiteralSet &s) const;
        bool operator<(const LiteralSet &s) const;
        friend std::ostream &operator<<(std::ostream &os, const LiteralSet &ls);
        std::set<Literal> get_literals() const;
        SetType get_set_type() const;

        LiteralSet invert() const;
        LiteralSet pos() const;
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
        LiteralSet set_minus(const LiteralSet &s) const;

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
        // Parent pointer to recover witnessing path.
        // Defined in SUDA 3.2
        std::shared_ptr<Obligation> parent;
        LiteralSet state;
        int priority;

    public:
        Obligation(LiteralSet s, int priority, std::shared_ptr<Obligation> parent);
        Obligation(const Obligation &o);
        Obligation& operator=(const Obligation &o);
        friend std::ostream &operator<<(std::ostream &os, const Obligation &o);
        int get_priority() const;
        LiteralSet get_state() const;
        bool operator<(const Obligation &o) const;
        const std::shared_ptr<Obligation> get_parent() const;
    };

    class SetOfLiteralSets
    {
    protected:
        
        const SetType set_type;
        std::set<LiteralSet> sets;

    public:
        SetOfLiteralSets();
        SetOfLiteralSets(SetType type);
        SetOfLiteralSets(const SetOfLiteralSets &s);
        SetOfLiteralSets(const std::set<LiteralSet> sets, SetType type);
        virtual ~SetOfLiteralSets();
        SetOfLiteralSets operator=(const SetOfLiteralSets &l) const;
        bool operator==(const SetOfLiteralSets &s) const;
        bool operator<(const SetOfLiteralSets &s) const;
        friend std::ostream &operator<<(std::ostream &os, const SetOfLiteralSets &l);
        size_t size() const;
        const std::set<LiteralSet> get_sets() const;
        virtual void add_set(LiteralSet s);
        bool contains_set(LiteralSet s) const;
        bool is_subset_eq_of(const SetOfLiteralSets &s) const;
        SetOfLiteralSets set_minus(const SetOfLiteralSets &s) const;
    };


    class Layer : public SetOfLiteralSets
    {
    private:
    public:
        Layer();
        Layer(const Layer &l);
        Layer(const std::set<LiteralSet> clauses);
        Layer operator=(const Layer &l) const;
        friend std::ostream &operator<<(std::ostream &os, const Layer &l);
        void add_set(LiteralSet c);
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

        LiteralSet from_precondition(const PreconditionsProxy &pc) const;
        LiteralSet from_effect(const EffectsProxy &ep) const;

        LiteralSet all_variables() const;
    };

    extern void add_options_to_parser(options::OptionParser &parser);
    std::ostream &operator<<(std::ostream &os, const std::set<SetOfLiteralSets> &s);
}

#endif
