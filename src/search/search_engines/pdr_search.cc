#include "pdr_search.h"

#include "../option_parser.h"

#include "../utils/logging.h"
#include "../plan_manager.h"

#include "../pdr/pattern-database.h"
#include "../pdbs/pattern_generator_greedy.h"

#include <cassert>
#include <cstddef>
#include <cstdlib>
#include <memory>
#include <optional.hh>
#include <set>
#include <algorithm>
#include <iostream>
#include <vector>
#include <chrono>


namespace pdr_search
{

    std::pair<LiteralSet, bool> PDRSearch::extend(const LiteralSet &s, const Layer &L)
    {
        extend_time.resume();
        assert(!s.models(L));

        auto A = this->task_proxy.get_operators();

        SetOfLiteralSets Ls = SetOfLiteralSets(SetType::CLAUSE);
        SetOfLiteralSets Rnoop = SetOfLiteralSets(SetType::CUBE);

        std::unordered_set<SetOfLiteralSets, SetOfLiteralSetsHash> Reasons;
        auto sets = L.get_sets();
        for (auto c = sets->begin(); c != sets->end(); ++c) 
        { 
            if (!s.models(*c))
            {
                Ls.add_set(*c);
                Rnoop.add_set(c->invert());
            }
        }

        assert(Rnoop.size() > 0);
        Reasons.insert(Rnoop);

        for (size_t a_i = 0; a_i < A.size(); a_i++)
        {
            auto pre = from_precondition(A[a_i].get_preconditions());

            auto pre_sa = LiteralSet(SetType::CLAUSE);
            for (const auto &l : pre.get_literals())
            {
                if (!s.models(LiteralSet(l, SetType::CLAUSE)))
                {
                    pre_sa.add_literal(l);
                }
            }
            assert(pre_sa.is_subset_eq_of(pre));

            LiteralSet &eff_a = A_effect[a_i]; 
            LiteralSet t = LiteralSet(s);
            for (const auto &l : eff_a.get_literals())
            {
                // apply eff_a to t
                t.apply_literal(l);
            }

            SetOfLiteralSets Lt = SetOfLiteralSets(SetType::CLAUSE);
            for (auto c = sets->begin(); c != sets->end(); ++c)
            {
                if (!t.models(*c))
                {
                    Lt.add_set(*c);
                }
            }

            if (pre_sa.size() == 0 && Lt.size() == 0)
            {
                // output condition of successor
                assert(t.models(L));

                this->extend_time.stop();
                return std::pair<LiteralSet, bool>(t, true);
            }

            else if (Ls.is_subset_eq_of(Lt))
            {
                continue;
            }
            else
            {
                // Comment: In the pseudocode, the arrow should be pointing left (Suda)
                SetOfLiteralSets Lt0 = SetOfLiteralSets(SetType::CLAUSE);
                for (const auto &c : Lt.get_sets())
                {
                    if (c.set_intersect_size(pre_sa) == 0)
                    {
                        Lt0.add_set(c);
                    }
                }

                SetOfLiteralSets R_a = SetOfLiteralSets(SetType::CUBE);
                for (const auto &l : pre_sa.get_literals())
                {
                    R_a.add_set(LiteralSet(l.invert(), SetType::CUBE));
                }

                for (const auto &c : Lt0.get_sets())
                {
                    LiteralSet ls = LiteralSet(SetType::CUBE);
                    for (const auto &l : c.get_literals())
                    {
                        if (!eff_a.contains_literal(l.invert()))
                        {
                            ls.add_literal(l.invert());
                        }
                    }
                    R_a.add_set(ls);
                }

                Reasons.insert(R_a);
            }
        }

        LiteralSet r = LiteralSet(SetType::CUBE);

        std::vector<SetOfLiteralSets> R = std::vector<SetOfLiteralSets>(Reasons.begin(), Reasons.end());
        std::sort(R.begin(), R.end(), [](const SetOfLiteralSets &f, const SetOfLiteralSets &l)
                  { return f.size() < l.size(); });

        int i = 0;
        for (const auto &Ra : R)
        {
            LiteralSet ra = LiteralSet(SetType::CUBE);
            size_t ra_size = 0;
            for (const auto &ra_cur : Ra.get_sets())
            {
                if (ra_size == 0 || ra_size > r.set_union(ra_cur).size())
                {
                    ra = ra_cur;
                    ra_size = r.set_union(ra).size();
                }
            }
            r = r.set_union(ra);
            i += 1;
        }

        auto r_literals = r.get_literals();
        for (const auto &l : r_literals) {
            auto ls = LiteralSet(SetType::CUBE);
            ls.add_literal(l);
            auto r_minus_l = r.set_minus(ls);
            bool condition_is_met = true;
            for (const auto &Ra : R) {
                bool exists = false;
                for (const auto &ra : Ra.get_sets()) {
                    if (ra.is_subset_eq_of(r_minus_l)) {
                        exists = true;
                        break;
                    }
                }
                if (!exists) {
                   condition_is_met = false;
                   break;
                }
            }
            if (condition_is_met) {
                r = r_minus_l;
            }
        }

        assert(r.size() > 0);
        
        // output condition of reason.
        assert(r.is_subset_eq_of(s));
        this->extend_time.stop();
        return std::pair<LiteralSet, bool>(r, false);
    }

    std::shared_ptr<Layer> PDRSearch::get_layer(long unsigned int i)
    {
        assert(i <= layers.size() + 1);

        if (layers.size() > i)
        {
            return layers[i];
        }
        else if (i == 0)
        {
            this->seeding_time.resume();
            // no parent layer -> nullptr
            std::shared_ptr<Layer> l0 = std::shared_ptr<Layer>(new Layer(nullptr, nullptr));
            this->heuristic->initial_heuristic_layer(0, l0);
            size_t seeded_layer_size = l0->size();
            auto g = this->task_proxy.get_goals();
            for (size_t i = 0; i < g.size(); i++)
            {
                // no need to add the other literals as inverse,
                // the goal is only a partial assignment.
                l0->add_set(LiteralSet(Literal::from_fact(g[i]), SetType::CLAUSE));
            }
            
            this->seeding_time.stop();     
        
            this->layers.insert(this->layers.end(), l0);
            this->seeded_layers_size.insert(this->seeded_layers_size.end(), seeded_layer_size); 
            return layers[i];
        }
        else
        {
            this->seeding_time.resume();
            std::shared_ptr<Layer> parent = get_layer(i-1);
            std::shared_ptr<Layer> l_i = std::shared_ptr<Layer>(new Layer(nullptr, parent));
            parent->set_child(l_i);
            if (this->heuristic)
            {
                this->heuristic->initial_heuristic_layer(i, l_i);
            } 
            
            this->seeding_time.stop();
            this->layers.insert(this->layers.end(), l_i);
            this->seeded_layers_size.insert(this->seeded_layers_size.end(),l_i->size()); 
            return layers[i];
        }
    }

    void PDRSearch::initialize()
    {
        auto L0 = get_layer(0);
        for (const auto &a: task_proxy.get_operators()) {
           A_effect.insert(A_effect.end(), from_effect(a.get_effects()));
        }

    }

    void printLayers(std::vector<std::shared_ptr<Layer>> layers) {
        std::cout << "Printing all layers" << std::endl;
        for (size_t i = 0; i < layers.size(); ++i) {
            std::cout << "Layer " << i << ": " << std::endl;
            std::cout << layers[i] << std::endl;
        }
        std::cout << "Printing all deltas" << std::endl;
        for (size_t i = 0; i < layers.size(); ++i) {
            std::cout << "Delta " << i << ": " << std::endl;
            auto delta = *layers[i]->get_delta();
            for (const auto &d : delta) {
                std::cout << d << ", ";
            }
            std::cout << std::endl;
        }
        std::cout << "End printing layers" << std::endl;
    }

    void PDRSearch::print_statistics() const
    {
        for(size_t i = 0; i < this->layers.size(); ++i) {
            std::cout << "Layer size " << i << ": " <<  this->layers[i]->size() << std::endl;
            std::cout << "Layer size (literals) " << i << ": " ;
            size_t lits = 0; 
            auto sets = this->layers[i]->get_sets();
            for (auto ls = sets->begin(); ls != sets->end(); ++ls)
            {
                lits += ls->size();
            }
            std::cout << lits << std::endl;

            std::cout << "Seed layer size " << i << ": " << this->seeded_layers_size[i]<<std::endl;
        }
        std::cout << "Obligation expansions per layer: ";
        for(size_t i = 0; i < this->obligation_expansions_per_layer.size(); ++i) {
            std::cout << "("<<i<<","<<
                this->obligation_expansions_per_layer[i]<<"), ";
        }
        std::cout << std::endl;
        std::cout << "Total clause propagation time: " << this->clause_propagation_time << std::endl;
        std::cout << "Total extend time: " << this->extend_time << std::endl;
        std::cout << "Total path construction phase time: " << this->path_construction_time << std::endl;
        std::cout << "Total seeding time: " << this->seeding_time <<std::endl;
        std::cout << "Total expanded obligations: " << this->obligation_expansions << std::endl;
        std::cout << "Total inserted obligations: " << this->obligation_insertions << std::endl;

        statistics.print_detailed_statistics();
        search_space.print_statistics();
    }

    SearchStatus PDRSearch::step()
    {
        struct obligationSort
        {
            bool operator()(const std::shared_ptr<Obligation> l, const std::shared_ptr<Obligation> r) const
            {
                return l->get_priority() > r->get_priority();
            }
        };

        std::cout << "Step " << iteration << " of PDR search" << std::endl;

        for (size_t i = 0; i < this->layers.size() - 1; ++i)
        {
            assert(this->layers[i + 1]->is_subset_eq_of(*this->layers[i]));
        }

        auto X = all_variables();
        
        size_t obligation_expansions_this_iteration = 0;
        const int k = iteration;
        iteration += 1;

        this->path_construction_time.resume();
        auto s_i = from_state(this->task_proxy.get_initial_state());
        if (s_i.models(*get_layer(k)))
        {
            auto o = std::shared_ptr<Obligation>(new Obligation(s_i, k, nullptr));
            std::priority_queue<std::shared_ptr<Obligation>, std::vector<std::shared_ptr<Obligation>>, obligationSort> Q;
            Q.push(o);
            this->obligation_insertions += 1;

            while (!Q.empty())
            {
                auto si = Q.top();
                Q.pop();
                this->obligation_expansions += 1;
                obligation_expansions_this_iteration += 1;
                int i = si->get_priority();
                auto s = si->get_state();
                if (i == 0)
                {
                    extract_path(si, s_i);
                    this->path_construction_time.stop();
                    this->obligation_expansions_per_layer.insert(this->obligation_expansions_per_layer.end(), 
                            obligation_expansions_this_iteration);
                    return SearchStatus::SOLVED;
                }

                auto extended = extend(s, *get_layer(i - 1));
                if (extended.second)
                {
                    // extend returns a successor state t
                    LiteralSet &t = extended.first;
                    Q.push(si);
                    auto newObligation = std::shared_ptr<Obligation>(new Obligation(t, si->get_priority() - 1, si));
                    Q.push(newObligation);
                    this->obligation_insertions += 2;
                }
                else
                {
                    LiteralSet &r = extended.first;
                    // Only add to set L_i, because of layer delta encoding
                    auto L_i = get_layer(i);
                    L_i->add_set(r.invert());

                    if (enable_obligation_rescheduling && i < k)
                    {
                        auto newObligation = std::shared_ptr<Obligation>(new Obligation(s, i + 1, si->get_parent()));
                        Q.push(newObligation);
                        this->obligation_insertions += 1;
                    }
                }
                for (size_t i = 0; i < this->layers.size() - 1; ++i)
                {
                    assert(this->layers[i + 1]->is_subset_eq_of(*(this->layers[i])));
                }
            }
        }
        this->path_construction_time.stop();

        // Clause propagation
        this->clause_propagation_time.resume();
        auto A = this->task_proxy.get_operators();

        for (size_t j = 0; j < this->layers.size() - 1; ++j)
        {
            assert(this->layers[j + 1]->is_subset_eq_of(*(this->layers[j])));
        }
        for (int i = 1; i <= k + 1; i++)
        {
            std::shared_ptr<Layer> Li1 = get_layer(i - 1);
            // NOTE: this copy can not be prevented by dereferencing the 
            // pointer in the for loop. The layer Li-1 gets modified inside of the 
            // loop. Without copying the iterator gets invalidated and crashes on linux.
            auto delta = *Li1->get_delta();
            for (const auto c : delta)
            {
                LiteralSet s_c = LiteralSet(X.get_literals(), SetType::CUBE);
                for (const auto &p : c.get_literals())
                {
                    s_c.apply_literal(p.neg());
                }

                bool for_all_not_models = true;
                for (size_t a_i = 0; a_i < A.size(); a_i++)
                {
                    LiteralSet pre_a = from_precondition(A[a_i].get_preconditions());
                    LiteralSet applied = LiteralSet(s_c);
                    LiteralSet effect_a = A_effect[a_i];
                    for (const auto &l : effect_a.get_literals())
                    {
                        applied.apply_literal(l);
                    }
                    if (!(!s_c.models(pre_a) || !applied.models(*get_layer(i - 1))))
                    {
                        for_all_not_models = false;
                        break;
                    }
                }
                if (for_all_not_models)
                {
                    get_layer(i)->add_set(c);
                }
            }
            // Li-1 == Li
            if (get_layer(i-1)->get_delta()->empty())
            {
                this->clause_propagation_time.stop();
                this->obligation_expansions_per_layer.insert(this->obligation_expansions_per_layer.end(), 
                        obligation_expansions_this_iteration);
                return SearchStatus::FAILED;
            }
            for (size_t j = 0; j < this->layers.size() - 1; ++j)
            {
                assert(this->layers[j + 1]->is_subset_eq_of(*(this->layers[j])));
            }
        }
        this->clause_propagation_time.stop();
        this->obligation_expansions_per_layer.insert(this->obligation_expansions_per_layer.end(), 
                obligation_expansions_this_iteration);
        return SearchStatus::IN_PROGRESS;
    }

    void PDRSearch::extract_path(const std::shared_ptr<Obligation> goal_obligation, const LiteralSet initialState)
    {
        std::shared_ptr<Obligation> ob = goal_obligation;
        std::vector<LiteralSet> state_list = std::vector<LiteralSet>();
        do
        {
            state_list.insert(state_list.begin(), ob->get_state());
            ob = ob->get_parent();
        } while (ob->get_parent() != nullptr);
        state_list.insert(state_list.begin(), initialState);

        std::vector<OperatorID> plan;
        auto operators = task_proxy.get_operators();
        for (size_t i = 1; i < state_list.size(); i++)
        {
            OperatorID matched_op = OperatorID::no_operator;
            for (size_t a_i = 0; a_i < operators.size(); a_i++)
            {
                auto pre = from_precondition(operators[a_i].get_preconditions());
                auto state = state_list[i - 1];
                if (!state.models(pre))
                {
                    continue;
                }
                auto eff = A_effect[a_i];
                state.apply_cube(eff);
                if (state != state_list[i])
                {
                    continue;
                }
                matched_op = OperatorID(operators[a_i].get_id());
            }
            assert(matched_op != OperatorID::no_operator);
            plan.insert(plan.end(), matched_op);
        }
        assert(plan.size() == state_list.size() - 1);
        set_plan(plan);
    }

    LiteralSet PDRSearch::from_state(const State &s) const
    {
        s.unpack();

        int i = 0;
        std::unordered_set<Literal, LiteralHash> result;
        for (const auto &value : s.get_unpacked_values())
        {
            Literal v = Literal::from_fact(FactProxy(*task, i, value));
            result.insert(result.begin(), v);
            i += 1;
        }
        LiteralSet c = LiteralSet(result, SetType::CUBE);

        auto vars = this->task_proxy.get_variables();
        int variable_index = 0;
        for (const auto &var : vars)
        {
            int dom_size = var.get_domain_size();
            for (int i = 0; i < dom_size; i++)
            {
                Literal l = Literal::from_fact(FactProxy(*task, variable_index, i));
                if (!c.contains_literal(l))
                {
                    c.add_literal(l.invert());
                }
            }
            variable_index += 1;
        }
        return c;
    }

    LiteralSet PDRSearch::all_variables() const
    {
        LiteralSet c = LiteralSet(SetType::CUBE);
        auto vars = this->task_proxy.get_variables();
        int variable_index = 0;
        for (const auto &var : vars)
        {
            int dom_size = var.get_domain_size();
            for (int i = 0; i < dom_size; i++)
            {
                Literal l = Literal::from_fact(FactProxy(*task, variable_index, i));
                c.add_literal(l);
            }
            variable_index += 1;
        }
        return c;
    }

    LiteralSet PDRSearch::from_precondition(const PreconditionsProxy &pc) const
    {
        LiteralSet ls = SetType::CUBE;
        for (const auto &fact : pc)
        {
            Literal l = Literal::from_fact(fact);
            ls.add_literal(l);
        }
        return ls;
    }
    LiteralSet PDRSearch::from_effect(const EffectsProxy &ep) const
    {
        LiteralSet ls = SetType::CUBE;
        for (const auto &fact_prox : ep)
        {
            auto fact = fact_prox.get_fact();
            Literal l = Literal::from_fact(fact);
            ls.add_literal(l);
            // add other (variable, value) pairs inverted
            int variable = fact.get_pair().var;
            int dom_size = fact.get_variable().get_domain_size();
            for (int i = 0; i < dom_size; i++)
            {
                Literal l = Literal::from_fact(FactProxy(*task, variable, i));
                if (!ls.contains_literal(l))
                {
                    ls.add_literal(l.invert());
                }
            }
        }
        return ls;
    }

    PDRSearch::PDRSearch(const Options &opts) : SearchEngine(opts)
    {
        enable_obligation_rescheduling = opts.get<bool>("ob-resched");
        enable_layer_simplification = opts.get<bool>("s-layers");

        std::shared_ptr<PDRHeuristic> pdr_heuristic =
            opts.get<std::shared_ptr<PDRHeuristic>>("heuristic");

        heuristic = pdr_heuristic;
    }

    PDRSearch::~PDRSearch()
    {
    }

    void add_options_to_parser(OptionParser &parser)
    {
        SearchEngine::add_options_to_parser(parser);
        parser.add_option<std::shared_ptr<PDRHeuristic>>(
            "heuristic",
            "pdr heuristic",
            "pdr-noop()");
        parser.add_option<bool>("ob-resched", "enable obligation scheduling", "true");
        parser.add_option<bool>("s-layers", "enable layer simplification", "false");
    }

    // helper method to print sets of SetOfliteralSets
    std::ostream &operator<<(std::ostream &os, const std::set<SetOfLiteralSets> &s)
    {
        os << "std::set<>(";
        for (auto const &i : s)
        {
            os << i << ", ";
        }
        os << ")";
        return os;
    }
}
