#include "pdr_search.h"

#include "../option_parser.h"

#include "../utils/logging.h"

#include <cassert>
#include <cstdlib>
#include <memory>
#include <optional.hh>
#include <set>
#include <algorithm>
#include <iostream>
#include <vector>

namespace pdr_search
{
    Literal::Literal(int var, int val) : variable(var), value(val)
    {
    }
    bool Literal::operator<(const Literal &o) const
    {
        if (variable == o.variable)
        {
            if (value == o.value)
            {
                return positive < o.positive;
            }
            return value < o.value;
        }
        return variable < o.variable;
    }

    Literal Literal::invert() const
    {
        Literal n = Literal(variable, value);
        n.positive = !positive;
        return n;
    }

    Literal Literal::from_fact(FactProxy fp)
    {
        auto factPair = fp.get_pair();
        return Literal(factPair.var, factPair.value);
    }

    LiteralSet::LiteralSet()
    {
    }
    LiteralSet::LiteralSet(Literal v)
    {
        literals.insert(literals.begin(), v);
    }
    LiteralSet::LiteralSet(int variable, int value)
    {
        literals.insert(literals.begin(), Literal(variable, value));
    }

    bool LiteralSet::operator<(const LiteralSet &b) const
    {
        if (!(literals < b.literals) && !(b.literals < literals))
        {
            return clause < b.clause;
        }
        return literals < b.literals;
    }

    std::set<Literal> LiteralSet::get_literals() const
    {
        return literals;
    }

    LiteralSet LiteralSet::invert() const
    {
        assert(is_unit());
        Literal l = *(literals.begin());
        LiteralSet new_set = LiteralSet(l.invert());
        new_set.clause = !clause;
        return new_set;
    }

    size_t LiteralSet::size() const 
    {
        return literals.size();
    }

    bool LiteralSet::is_unit() const
    {
        return literals.size() == 1;
    }

    bool LiteralSet::is_clause() const
    {
        return clause;
    }

    bool LiteralSet::is_cube() const
    {
        return !clause;
    }

    void LiteralSet::add_literal(Literal l)
    {
        literals.insert(literals.begin(), l);
    }

    void LiteralSet::remove_literal(Literal l)
    {
        literals.erase(l);
    }

    void LiteralSet::apply_literal(Literal l)
    {
        remove_literal(l.invert());
        add_literal(l);
    }

    bool LiteralSet::contains_literal(Literal l) const
    {
        return literals.find(l) != literals.end();
    }

    bool LiteralSet::is_subset_eq_of(LiteralSet ls) const
    {
        if (size() > ls.size())
        {
            return false;
        }
        for (Literal l : literals)
        {
            if (!ls.contains_literal(l))
            {
                return false;
            }
        }
        return true;
    }

    // ⊧ (\models)
    bool LiteralSet::models(LiteralSet s) const
    {
        for (Literal l : s.get_literals())
        {
            if (contains_literal(l.invert()))
            {
                return false;
            }
        }
        return true;
    }

    LiteralSet LiteralSet::from_state(const State &s)
    {
        s.unpack();
        int i = 0;
        std::set<Literal> result;
        for (auto value : s.get_unpacked_values())
        {
            i += 1;
            Literal v = Literal(i, value);
            result.insert(result.begin(), v);
        }
        LiteralSet c = LiteralSet();
        c.literals = result;
        c.clause = false;
        return c;
    }

    Obligation::Obligation(State s, int p) : state(s), priority(p)
    {
    }

    int Obligation::get_priority() const
    {
        return priority;
    }

    State Obligation::get_state() const
    {
        return state;
    }

    bool Obligation::operator<(const Obligation &o) const
    {
        // We want an inverted priority queue. Smallest first
        return priority > o.priority;
    }

    Layer::Layer()
    {
        this->clauses = std::set<LiteralSet>();
    }

    Layer::Layer(const Layer &l)
    {
        this->clauses = std::set<LiteralSet>(l.clauses);
    }

    Layer::Layer(const std::set<LiteralSet> c) : clauses(c) 
    {
        for (LiteralSet ls : c)
        {
            assert(ls.is_clause());
            assert(ls.is_unit());
        }
    }

    size_t Layer::size() const
    {
        return clauses.size();
    }

    const std::set<LiteralSet> Layer::get_clauses() const
    {
        return clauses;
    }

    void Layer::add_clause(LiteralSet c)
    {
        assert(c.is_clause());
        assert(c.is_unit());
        this->clauses.insert(c);
    }

    void Layer::remove_clause(LiteralSet c)
    {
        clauses.erase(c);
    }

    bool Layer::contains_clause(LiteralSet c) const
    {
        auto res = this->clauses.find(c);
        return res != this->clauses.end();
    }

    bool Layer::is_subset_eq_of(Layer l) const
    {
        if (this->size() > l.size())
        {
            return false;
        }
        for (LiteralSet c : this->clauses)
        {
            if (!l.contains_clause(c))
            {
                return false;
            }
        }
        return true;
    }

    Layer Layer::without(Layer *l)
    {
        std::set<LiteralSet> result;
        set_difference(
            this->clauses.begin(), this->clauses.end(),
            l->clauses.begin(), l->clauses.end(),
            inserter(result, result.end()));

        Layer lnew;
        for (auto clause : result)
        {
            lnew.add_clause(clause);
        }
        return lnew;
    }

    bool Layer::modeled_by(State s)
    {
        for (Literal v : LiteralSet::from_state(s).get_literals())
        {
            if (contains_clause(LiteralSet(v.invert())))
            {
                return false;
            }
        }
        return true;
    }

    LiteralSet PDRSearch::extend(State state, Layer L)
    {
        auto A = this->task_proxy.get_operators();

        LiteralSet s = LiteralSet::from_state(state);
        //  Pseudocode 3

        // line 2
        Layer Ls;
        Layer Rnoop;

        std::vector<Layer> Reasons;
        for (LiteralSet c : L.get_clauses())
        {
            if (!s.models(c))
            {
                Ls.add_clause(c);
                // line 3
                Rnoop.add_clause(c.invert());
            }
        }
        // line 4
        assert(Rnoop.size() > 0);
        // line 5
        Reasons.insert(Reasons.begin(), Rnoop);
        // line 7
        for (auto a : A)
        {
            LiteralSet pre_sa;
            for (auto fact : a.get_preconditions())
            {
                Literal l = Literal::from_fact(fact);
                // line 8
                if (s.models(LiteralSet(l)))
                {
                    pre_sa.add_literal(l);
                }
            }
            // line 9
            LiteralSet t = LiteralSet(s);
            for (auto effect : a.get_effects())
            {
                FactProxy fact = effect.get_fact();
                t.apply_literal(Literal::from_fact(fact));
            }

            // line 10
            Layer Lt;
            for (LiteralSet c : L.get_clauses())
            {
                if (!t.models(c))
                {
                    Lt.add_clause(c);
                }
            }

            // line 11 & 12
            if (pre_sa.size() == 0 && Lt.size() == 0)
            {
                return t;
            }
            // line 13 & 14
            else if (Ls.is_subset_eq_of(Lt))
            {
                continue;
            }
            // line 15
            else
            {
                // line 16
                // QUESTION: the arrow should be pointing left right?
            }
        }

        //  Pseudocode 4
    }

    PDRSearch::PDRSearch(const Options &opts) : SearchEngine(opts)
    {
    }

    Layer PDRSearch::get_layer(int i) 
    {
        if (layers.size() > i)
        {
            return layers[i];
        } else if (i == 0) 
        {
            Layer l0;
            auto g = this->task_proxy.get_goals();
            for (size_t i = 0; i < g.size(); i++)
            {
                l0.add_clause(LiteralSet(Literal::from_fact(g[i])));
            }
            this->layers.insert(this->layers.begin(), l0);
            return l0;
        } else {
            Layer l_i;
            this->layers.insert(this->layers.begin(), l_i);
            
            // TODO: initialize layer with heuristic here

            return l_i;
        }
    }

    void PDRSearch::initialize()
    {
        get_layer(0);
    }

    void PDRSearch::print_statistics() const
    {
        statistics.print_detailed_statistics();
        search_space.print_statistics();
    }

    SearchStatus PDRSearch::step()
    {
        std::cout << "Step " << iteration << " of PDR search" << std::endl;
        // line 3
        int k = iteration;
        iteration += 1;

        // line 5
        auto s_i = this->task_proxy.get_initial_state();
        if (get_layer(k).modeled_by(s_i))
        {
            // line 6
            std::priority_queue<Obligation> Q;
            Q.push(Obligation(s_i, k));

            // line 7
            while (!Q.empty())
            {

                // line 8
                Obligation si = Q.top();
                Q.pop();

                // line 9
                if (si.get_priority() == 0)
                {
                    // line 10
                    return SearchStatus::SOLVED;
                }

                // line 11 TODO
                //if extend(s, L_i-1) returns a successor state t
                State *t = nullptr;

                //line 12
                Q.push(si);
                Q.push(Obligation(*t, si.get_priority()-1));

                // line 13 TOOD
                // else
                LiteralSet r;

                for(int j = 0; j <= si.get_priority(); j++)
                {

                }

                // line 14
            }
        }

        return SearchStatus::FAILED;
    }

    void add_options_to_parser(OptionParser &parser)
    {
        SearchEngine::add_options_to_parser(parser);
    }
}
