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
    Clause::Clause(int variable, int value) : variable(variable), value(value)
    {
    }

    bool Clause::operator<(const Clause &b) const
    {
        if (variable == b.variable)
        {
            if (value == b.value)
            {
                return positive < b.positive;
            }
            return value < b.value;
        }
        return variable < b.variable;
    }

    Clause Clause::invert() const
    {
        Clause cNew = Clause(variable, value);
        cNew.positive = !positive;
        return cNew;
    }

    Clause Clause::from_fact(FactProxy fp)
    {
        auto factPair = fp.get_pair();
        std::cout << "- name \"" << fp.get_name() << "\" var: " << factPair.var << " value: " << factPair.value << std::endl;
        return Clause(factPair.var, 1);
    }

    std::set<Clause> Clause::from_state(const State &s)
    {
        s.unpack();
        int i = 0;
        std::set<Clause> result;
        for (auto value : s.get_unpacked_values())
        {
            i += 1;
            Clause c = Clause(i, value);
            result.insert(c);
        }
        return result;
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
        this->clauses_set = std::set<Clause>();
    }

    Layer::Layer(const Layer &l)
    {
        this->clauses_set = std::set<Clause>(l.clauses_set);
    }

    Layer::Layer(const std::set<Clause> clauses) : clauses_set(clauses) {}

    Layer::~Layer()
    {
    }

    void Layer::add_clause(Clause c)
    {
        this->clauses_set.insert(c);
    }

    void Layer::apply_clause(Clause c)
    {
        if (contains_clause(c.invert()))
        {
            remove_clause(c.invert());
        }
        add_clause(c);
    }

    void Layer::remove_clause(Clause c)
    {
        clauses_set.erase(c);
    }

    bool Layer::contains_clause(Clause c) const
    {
        auto res = this->clauses_set.find(c);
        return res != this->clauses_set.end();
    }

    bool Layer::is_subset_eq_of(Layer l) const {
        if (this->size() > l.size()) {
            return false;
        }
        for(Clause c : this->clauses_set) {
            if (!l.contains_clause(c)) {
                return false;
            }
        }
        return true;
    }

    std::set<Clause> Layer::get_clauses()
    {
        return clauses_set;
    }

    Layer Layer::without(Layer *l)
    {
        std::set<Clause> result;
        set_difference(
            this->clauses_set.begin(), this->clauses_set.end(),
            l->clauses_set.begin(), l->clauses_set.end(),
            inserter(result, result.end()));

        Layer lnew;
        for (auto clause : result)
        {
            lnew.add_clause(clause);
        }
        return lnew;
    }

    bool Layer::models(State s)
    {
        for (Clause c : Clause::from_state(s))
        {
            if (contains_clause(c.invert()))
            {
                return false;
            }
        }
        return true;
    }

    Layer PDRSearch::extend(State s, Layer L) 
    {
        auto A = this->task_proxy.get_operators();

        Layer s_layer = Layer(Clause::from_state(s));
        // TODO
        //  Pseudocode 3

        // line 2
        Layer Ls;
        Layer Rnoop;

        std::vector<Layer> Reasons;
        for (Clause c : L.get_clauses())
        {
            if (s_layer.contains_clause(c.invert()))
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
            Layer pre_sa;
            for (auto fact : a.get_preconditions())
            {
                Clause pre = Clause::from_fact(fact);
                // line 8
                if (s_layer.contains_clause(pre.invert()))
                {
                    pre_sa.add_clause(pre);
                }
            }
            // line 9
            Layer t_layer = Layer(s_layer);
            for (auto effect : a.get_effects())
            {
                FactProxy fact = effect.get_fact();
                t_layer.apply_clause(Clause::from_fact(fact));
            }

            // line 10
            Layer Lt;
            for (Clause c : L.get_clauses())
            {
                if (t_layer.contains_clause(c.invert()))
                {
                    Lt.add_clause(c);
                }
            }

            // line 11 & 12
            if (pre_sa.size() == 0 && Lt.size() == 0) {
                return t_layer;
            }
            // line 13 & 14
            else if (Ls.is_subset_eq_of(Lt)) {
                continue;
            } 
            // line 15
            else {
                // line 16
                // QUESTION: the arrow should be pointing left right?
                
            }
        }

        //  Pseudocode 4
    }

    size_t Layer::size() const
    {
        return clauses_set.size();
    }

    PDRSearch::PDRSearch(const Options &opts) : SearchEngine(opts)
    {
    }

    void PDRSearch::initialize()
    {
        // Initialize L_0
        Layer l0;
        this->layers.insert(this->layers.begin(), l0);

        auto g = this->task_proxy.get_goals();
        for (size_t i = 0; i < g.size(); i++)
        {
            l0.add_clause(Clause::from_fact(g[i]));
        }
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
        if (layers[k].models(s_i))
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
                    return SearchStatus::SOLVED;
                }
            }
        }

        return SearchStatus::FAILED;
    }

    void add_options_to_parser(OptionParser &parser)
    {
        SearchEngine::add_options_to_parser(parser);
    }
}
