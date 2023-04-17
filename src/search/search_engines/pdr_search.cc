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
        // std::cout << "e1: Call to extend" << std::endl;
        // std::cout << "e1: s = " << s << std::endl;
        // std::cout << "e1: L = " << L << std::endl;
        // input condition
        assert(!s.models(L));

        auto A = this->task_proxy.get_operators();
        //  Pseudocode 3

        // line 2
        SetOfLiteralSets Ls = SetOfLiteralSets(SetType::CLAUSE);
        SetOfLiteralSets Rnoop = SetOfLiteralSets(SetType::CUBE);

        std::unordered_set<SetOfLiteralSets, SetOfLiteralSetsHash> Reasons;
        auto sets = L.get_sets();
        /* for (const LiteralSet &c : sets) */
        for (auto c = sets->begin(); c != sets->end(); ++c) 
        { 
            if (!s.models(*c))
            {
                Ls.add_set(*c);
                // line 3
                Rnoop.add_set(c->invert());
            }
        }
        // std::cout << "e2: Lˢ = " << Ls << std::endl;
        // std::cout << "e3: Rₙₒₒₚ = " << Rnoop << std::endl;

        // line 4
        assert(Rnoop.size() > 0);
        // line 5
        Reasons.insert(Rnoop);
        // std::cout << "e5: ℛ = " << Reasons << std::endl;
        // line 7
        for (size_t a_i = 0; a_i < A.size(); a_i++)
        {
            auto pre = from_precondition(A[a_i].get_preconditions());
            // std::cout << "e8: pre = " << pre << std::endl;
            // line 8
            auto pre_sa = LiteralSet(SetType::CLAUSE);
            for (const auto &l : pre.get_literals())
            {
                if (!s.models(LiteralSet(l, SetType::CLAUSE)))
                {
                    pre_sa.add_literal(l);
                }
            }
            assert(pre_sa.is_subset_eq_of(pre));
            // std::cout << "e8: preₛᵃ = " << pre_sa << std::endl;

            LiteralSet &eff_a = A_effect[a_i]; 
            // line 9
            LiteralSet t = LiteralSet(s);
            for (const auto &l : eff_a.get_literals())
            {
                // apply eff_a to t
                t.apply_literal(l);
            }
            // std::cout << "e9: effₐ = " << eff_a << std::endl;
            // std::cout << "e9: t = " << t << std::endl;
            // line 10
            SetOfLiteralSets Lt = SetOfLiteralSets(SetType::CLAUSE);
            /* for (const LiteralSet &c : sets) */
            for (auto c = sets->begin(); c != sets->end(); ++c)
            {
                if (!t.models(*c))
                {
                    Lt.add_set(*c);
                }
            }
            // std::cout << "e10: Lᵗ = " << Lt << std::endl;

            // line 11 & 12
            if (pre_sa.size() == 0 && Lt.size() == 0)
            {
                // std::cout << "e12: Successor t found: " << std::endl;
                // std::cout << "e12: t = " << t << std::endl;
                // output condition of successor
                assert(t.models(L));

                this->extend_time.stop();
                return std::pair<LiteralSet, bool>(t, true);
            }

            // line 13 & 14
            else if (Ls.is_subset_eq_of(Lt))
            {
                // std::cout << "e13: Lˢ ⊆ Lᵗ " << std::endl;
                // std::cout << "e13: Lˢ = " << Ls << std::endl;
                // std::cout << "e13: Lᵗ = " << Lt << std::endl;
                continue;
            }
            // line 15
            else
            {
                // line 16
                // Comment: In the pseudocode, the arrow should be pointing left.
                SetOfLiteralSets Lt0 = SetOfLiteralSets(SetType::CLAUSE);
                for (const auto &c : Lt.get_sets())
                {
                    if (c.set_intersect_size(pre_sa) == 0)
                    {
                        Lt0.add_set(c);
                    }
                }
                // std::cout << "e16: Lᵗ₀ = " << Lt0 << std::endl;
                // line 17
                SetOfLiteralSets R_a = SetOfLiteralSets(SetType::CUBE);
                for (const auto &l : pre_sa.get_literals())
                {
                    R_a.add_set(LiteralSet(l.invert(), SetType::CUBE));
                }
                // std::cout << "e17: Ra (left) = " << R_a << std::endl;
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
                // std::cout << "e17: Rₐ = " << R_a << std::endl;
                // line 18
                Reasons.insert(R_a);
            }
        }

        // line 20
        // std::cout << "e19: Stage two: compute an overall reason" << std::endl;
        LiteralSet r = LiteralSet(SetType::CUBE);
        // line 21
        std::vector<SetOfLiteralSets> R = std::vector<SetOfLiteralSets>(Reasons.begin(), Reasons.end());
        std::sort(R.begin(), R.end(), [](const SetOfLiteralSets &f, const SetOfLiteralSets &l)
                  { return f.size() < l.size(); });

        // std::cout << "e21: ℛ = " << R << std::endl;
        int i = 0;
        for (const auto &Ra : R)
        {
            // line 22
            // std::cout << "e22: Rₐ" << i << " = " << Ra << std::endl;

            LiteralSet ra = LiteralSet(SetType::CUBE);
            size_t ra_size = 0;
            for (const auto &ra_cur : Ra.get_sets())
            {
                if (ra_size == 0 || ra_size > r.set_union(ra_cur).size())
                {
                    // std::cout << "e22: candidate minimal rₐ = " << ra_cur << std::endl;
                    ra = ra_cur;
                    ra_size = r.set_union(ra).size();
                }
            }
            r = r.set_union(ra);
            i += 1;
        }
        // std::cout << "e23: r = " << r << std::endl;

        // line 25 - 27 optional
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

        // line 29
        assert(r.size() > 0);
        // std::cout << "e29: No successor t found, reason: r = " << r << std::endl;
        // std::cout << "e29: s=" << s << std::endl;
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
            // std::cout << "Layer 0 (goal): " << l0 << std::endl;
            /* std::cout << "Layer 0 (goal) size: " << l0->size() << " clauses" << std::endl; */
            
            this->seeding_time.stop();     
        
            //std::cout << "Initial Heuristic Layer " << 0 << ": " << l0 << std::endl;
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
                // asserts to make sure heuristic seed layer is valid.
                /* for (const auto &s : l_i->get_sets()) */
                /* { */
                /*     assert(s.is_clause()); */
                /*     for (const auto &l : s.get_literals()) */
                /*     { */
                /*         assert(l.is_positive()); */
                /*     } */
                /* } */

                // std::cout << "Initial Heuristic Layer " << i << ": " << l_i << std::endl;
            } 
            

            // std::cout << "Layer " << i << ": " << l_i << std::endl;
            /* std::cout << "Layer " << i << " size: " << l_i->size() << " clauses" << std::endl; */

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
            /* for (const LiteralSet &ls : sets) { */
            for (auto ls = sets->begin(); ls != sets->end(); ++ls)
            {
                lits += ls->size();
            }
            std::cout << lits << std::endl;

            std::cout << "Seed layer size " << i << ": " << this->seeded_layers_size[i]<<std::endl;
        }
        std::cout << "Total clause propagation time: " << this->clause_propagation_time << std::endl;
        std::cout << "Total extend time: " << this->extend_time << std::endl;
        std::cout << "Total path construction phase time: " << this->path_construction_time << std::endl;
        std::cout << "Total seeding time: " << this->seeding_time <<std::endl;
        std::cout << "Total expanded obligations: " << this->obligation_expansions << std::endl;

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

        // std::cout << "------------------" << std::endl;
        std::cout << "Step " << iteration << " of PDR search" << std::endl;
        // std::cout << "------------------" << std::endl;

        /* if (iteration == 4) { */
        /*     int i = 0; */
        /*     for (const auto &layer : this->layers) { */
        /*         std::cout<< "LAYER " << i++ << std::endl; */
        /*         std::cout<< *layer << std::endl; */
        /*         std::cout << "------" << std::endl; */
        /*     } */
        /* } */

        for (size_t i = 0; i < this->layers.size() - 1; ++i)
        {
            assert(this->layers[i + 1]->is_subset_eq_of(*this->layers[i]));
        }

        auto X = all_variables();

        // line 3
        const int k = iteration;
        iteration += 1;

        // std::cout << "4: Path construction" << std::endl;
        // line 5

        this->path_construction_time.resume();
        auto s_i = from_state(this->task_proxy.get_initial_state());
        if (s_i.models(*get_layer(k)))
        {
            // std::cout << "5: " << s_i << " ⊧ " << *get_layer(k) << std::endl;

            // line 6
            auto o = std::shared_ptr<Obligation>(new Obligation(s_i, k, nullptr));
            // std::cout
            // << "6: Initialize Q " << *o << std::endl;
            std::priority_queue<std::shared_ptr<Obligation>, std::vector<std::shared_ptr<Obligation>>, obligationSort> Q;
            Q.push(o);

            // line 7
            while (!Q.empty())
            {

                // line 8
                auto si = Q.top();
                Q.pop();
                this->obligation_expansions += 1;
                //std::cout << "8: Pop obligation from queue: (s, i) = " << *si << std::endl;
                if (si->get_parent()) {
                    //std::cout << "8: Parent = " << *(si->get_parent()) << std::endl;
                }
                int i = si->get_priority();
                auto s = si->get_state();
                // line 9
                if (i == 0)
                {
                    // line 10
                    // std::cout << "10: Path found" << std::endl;
                    extract_path(si, s_i);
                    // std::cout << "Step: i" << s_i << std::endl;
                    // std::cout << "Plan: " << std::endl;
                    this->path_construction_time.stop();
                    return SearchStatus::SOLVED;
                }

                // line 11
                // if extend(s, L_i-1) returns a successor state t
                auto extended = extend(s, *get_layer(i - 1));
                if (extended.second)
                {
                    // extend returns a successor state t
                    LiteralSet &t = extended.first;
                    // line 12
                    Q.push(si);
                    // std::cout << "12: Add obligation to queue: " << *si << std::endl;
                    auto newObligation = std::shared_ptr<Obligation>(new Obligation(t, si->get_priority() - 1, si));
                    Q.push(newObligation);
                    // std::cout << "12: Add obligation to queue: " << *newObligation << std::endl;
                }
                else
                {
                    // line 14 extend returns a reason r
                    LiteralSet &r = extended.first;
                    // line 15
                    // Only add to set L_i, because of layer delta encoding
                    auto L_i = get_layer(i);
                    L_i->add_set(r.invert());
                    //for (int j = 0; j <= i; j++)
                    //{
                    //    auto L_j = get_layer(j);
                        // std::cout << "15: Push clause to L_" << j << ": c = " << r.invert() << std::endl;
                    //    L_j->add_set(r.invert());
                        // std::cout << "15: L_" << j << " = " << *L_j << std::endl;
                    //}

                    // line 18 obligation rescheduling
                    if (enable_obligation_rescheduling && i < k)
                    {
                        // line 19
                        auto newObligation = std::shared_ptr<Obligation>(new Obligation(s, i + 1, si->get_parent()));
                        Q.push(newObligation);
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

        // std::cout << "clause propagation start" << std::endl;
        for (size_t j = 0; j < this->layers.size() - 1; ++j)
        {
            // std::cout << "layer " << (j) << " = " << this->layers[j] << std::endl;
            assert(this->layers[j + 1]->is_subset_eq_of(*(this->layers[j])));
        }
        // std::cout << "layer " << " = " << this->layers[this->layers.size()-1] << std::endl;

        // std::cout << "22: Clause propagation" << std::endl;
        // line 22
        for (int i = 1; i <= k + 1; i++)
        {
            // std::cout << "22: foreach i = " << i << " of " << k + 1 << std::endl;
            // line 23
            std::shared_ptr<Layer> Li1 = get_layer(i - 1);
            // std::cout << "22: L_" << (i-1) << " = " << Li1 << std::endl;
            // std::cout << "22: L_" << i << " = " << Li << std::endl;
            auto delta = Li1->get_delta();
            for (const auto c : *delta)
            {
                // std::cout << "23: foreach c in L_" << (i - 1) << " \\ L_" << i << std::endl;
                // std::cout << "25: c = " << c << std::endl;
                // std::cout << "25: X \\ c  = " << X.set_minus(c.pos()) << std::endl;

                // line 25
                LiteralSet s_c = LiteralSet(X.get_literals(), SetType::CUBE);
                for (const auto &p : c.get_literals())
                {
                    s_c.apply_literal(p.neg());
                }
                // std::cout << "25: s_c " << s_c << std::endl;

                // line 26
                bool for_all_not_models = true;
                for (size_t a_i = 0; a_i < A.size(); a_i++)
                {
                    LiteralSet pre_a = from_precondition(A[a_i].get_preconditions());
                    // build apply(s_c, a)
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
                    // std::cout << "26: for_all_not_models" << std::endl;
                    // line 27
                    get_layer(i)->add_set(c);
                    // std::cout << "27: Push clause to L_" << i << ": c = " << c << ", L = " << *get_layer(i) << std::endl;
                }
            }
            if (get_layer(i-1)->get_delta()->empty())
            {
                // line 30
                // std::cout << "30: No plan possible" << std::endl;
                this->clause_propagation_time.stop();
                return SearchStatus::FAILED;
            }
            // std::cout << "clause propagation " << i <<std::endl;
            for (size_t j = 0; j < this->layers.size() - 1; ++j)
            {
                assert(this->layers[j + 1]->is_subset_eq_of(*(this->layers[j])));
            }
        }
        this->clause_propagation_time.stop();

        // if (enable_layer_simplification)
        // {
        //     for (int i = 0; get_layer(i)->size() != 0; i++)
        //     {
        //         // std::cout << "Layer L_" << i << ": " << *get_layer(i) << std::endl;
        //         get_layer(i)->simplify();
        //         // std::cout << "Layer L_" << i << ": " << *get_layer(i) << std::endl;
        //     }
        // }
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
                //std::cout << "Step " << i << " - matching operator " << op.get_name() << std::endl;
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
