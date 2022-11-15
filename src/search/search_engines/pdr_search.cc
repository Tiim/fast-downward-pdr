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

    Clause Clause::FromFact(FactProxy fp)
    {
        auto factPair = fp.get_pair();
        std::cout << "- name \"" << fp.get_name() << "\" var: " << factPair.var << " value: " << factPair.value << std::endl;
        return Clause(factPair.var, 1);
    }

    bool Clause::operator<(const Clause &b) const
    {
        return (this->value ? 1 : -1) * this->variable < (b.value ? 1 : -1) * b.variable;
    }

    Layer::Layer()
    {
        this->clauses_set = std::set<Clause>();
    }

    Layer::Layer(const Layer &l)
    {
        this->clauses_set = std::set<Clause>(l.clauses_set);
    }

    Layer::~Layer()
    {
    }

    void Layer::add_clause(Clause c)
    {
        this->clauses_set.insert(c);
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
        s.unpack();
        std::cout << "InitialState s.get_unpacked_values()=" << s.get_unpacked_values() << std::endl;
        return false;
    }

    bool Layer::extend(State s, State *successor)
    {
        // Pseudocode 3
        Layer Ls;
        for (auto clause : this->clauses_set) {
        }

        // Pseudocode 4
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
            l0.add_clause(Clause::FromFact(g[i]));
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
        int k = iteration;
        iteration += 1;

        auto s_i = this->task_proxy.get_initial_state();

        if (layers[k].models(s_i))
        {
            std::cout << "Layer " << k << " models initial state" << std::endl;
        }

        return SearchStatus::FAILED;
    }

    void add_options_to_parser(OptionParser &parser)
    {
        SearchEngine::add_options_to_parser(parser);
    }
}
