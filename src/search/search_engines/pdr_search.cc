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

// TODO: perf improvements unordered sets

namespace pdr_search
{
    Literal::Literal(int var, int val) : variable(var), value(val)
    {
    }
    Literal::Literal(int var, int val, bool pos) : variable(var), value(val), positive(pos)
    {
    }
    Literal::Literal(const Literal &l) : variable(l.variable), value(l.value), positive(l.positive)
    {
    }
    Literal Literal::operator=(const Literal &l) const
    {
        return Literal(l);
    }
    bool Literal::operator==(const Literal &l) const
    {
        return (variable == l.variable) && (value == l.value) && (positive == l.positive);
    }
    bool Literal::operator<(const Literal &b) const
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
    std::ostream &operator<<(std::ostream &os, const Literal &l)
    {
        os << (l.positive ? "" : "¬") << "(" << l.variable << "," << l.value << ")";
        return os;
    }

    Literal Literal::invert() const
    {
        return Literal(variable, value, !positive);
    }

    Literal Literal::from_fact(FactProxy fp)
    {
        auto factPair = fp.get_pair();
        return Literal(factPair.var, factPair.value);
    }

    LiteralSet::LiteralSet(SetType type) : set_type(type)
    {
    }
    LiteralSet::LiteralSet(Literal v, SetType type) : set_type(type)
    {
        literals.insert(v);
    }
    LiteralSet::LiteralSet(const LiteralSet &s) : set_type(s.set_type), literals(s.literals)
    {
    }

    LiteralSet::LiteralSet(std::set<Literal> init_literals, SetType type) : set_type(type), literals(init_literals)
    {
    }

    LiteralSet& LiteralSet::operator=(const LiteralSet &s)
    {
        literals = s.literals;
        set_type = s.set_type;
        return *this;
    }

    bool LiteralSet::operator==(const LiteralSet &s) const
    {
        if (set_type == s.set_type)
        {
            return literals == s.literals;
        }
        return false;
    }
    bool LiteralSet::operator<(const LiteralSet &b) const
    {
        if (!(literals < b.literals) && !(b.literals < literals))
        {
            return set_type < b.set_type;
        }
        return literals < b.literals;
    }
    std::ostream &operator<<(std::ostream &os, const LiteralSet &ls)
    {
        os << "LiteralSet{";
        bool first = true;
        for (auto c : ls.get_literals())
        {
            if (!first && ls.set_type == SetType::CUBE)
            {
                os << " ∧ ";
            }
            else if (!first && ls.set_type == SetType::CLAUSE)
            {
                os << " ∨ ";
            }
            first = false;
            os << c;
        }
        os << "} ";
        return os;
    }

    std::set<Literal> LiteralSet::get_literals() const
    {
        return literals;
    }
    SetType LiteralSet::get_set_type() const
    {
        return set_type;
    }

    LiteralSet LiteralSet::invert() const
    {
        std::set<Literal> new_set;
        for (auto l : literals)
        {
            new_set.insert(l.invert());
        }
        SetType type;
        if (set_type == SetType::CLAUSE)
        {
            type = SetType::CUBE;
        }
        else
        {
            type = SetType::CLAUSE;
        }
        return LiteralSet(new_set, type);
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
        return set_type == SetType::CLAUSE;
    }

    bool LiteralSet::is_cube() const
    {
        return set_type == SetType::CUBE;
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

    bool LiteralSet::is_subset_eq_of(const LiteralSet &ls) const
    {
        assert(set_type == ls.set_type);
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

    LiteralSet LiteralSet::set_union(const LiteralSet &s) const
    {
        assert(set_type == s.set_type);
        LiteralSet tmp = LiteralSet(literals, set_type);
        for (auto l : s.literals)
        {
            tmp.add_literal(l);
        }
        return tmp;
    }

    LiteralSet LiteralSet::set_intersect(const LiteralSet &s) const
    {
        assert(set_type == s.set_type);
        std::set<Literal> output_set;

        // sometimes doesn't terminate ?
        // std::set_intersection(
        //    literals.begin(), literals.end(),
        //    s.literals.begin(), s.literals.end(),
        //    output_set.begin());

        const LiteralSet *small = this, *big = &s;
        if (this->size() >= s.size())
        {
            small = &s;
            big = this;
        }
        for (auto l : big->get_literals())
        {
            if (small->contains_literal(l))
            {
                output_set.insert(l);
            }
        }
        return LiteralSet(output_set, set_type);
    }

    bool LiteralSet::models(const LiteralSet &c) const
    {
        assert(is_cube());
        if (c.is_clause())
        {
            for (auto cl : c.get_literals())
            {
                if (contains_literal(cl))
                {
                    return true;
                }
            }
            return false;
        }
        else
        {
            return is_subset_eq_of(c);
        }
    }

    bool LiteralSet::models(const Layer &l) const
    {
        for (auto c : l.get_sets())
        {
            if (!models(c))
            {
                return false;
            }
        }
        return true;
    }

    Obligation::Obligation(LiteralSet s, int p) : state(s), priority(p)
    {
        assert(s.is_cube());
    }
    Obligation::Obligation(const Obligation &o) : state(o.state), priority(o.priority)
    {
    }
    Obligation Obligation::operator=(const Obligation &o) const
    {
        return Obligation(o);
    }
    std::ostream &operator<<(std::ostream &os, const Obligation &o)
    {
        os << "Ob(" << o.state << "," << o.priority << ")";
        return os;
    }

    int Obligation::get_priority() const
    {
        return priority;
    }

    LiteralSet Obligation::get_state() const
    {
        return state;
    }

    bool Obligation::operator<(const Obligation &o) const
    {
        // We want an inverted priority queue. Smallest first
        return priority > o.priority;
    }

    SetOfLiteralSets::SetOfLiteralSets() : set_type(SetType::CUBE)
    {
    }
    SetOfLiteralSets::SetOfLiteralSets(SetType type) : set_type(type)
    {
    }
    SetOfLiteralSets::SetOfLiteralSets(const SetOfLiteralSets &s) : set_type(s.set_type)
    {
        this->sets = std::set<LiteralSet>(s.sets);
    }
    SetOfLiteralSets::SetOfLiteralSets(const std::set<LiteralSet> s, SetType type) : set_type(type)
    {
        this->sets = std::set<LiteralSet>(s);
        for (auto set : s)
        {
            assert(set.get_set_type() == type);
        }
    }
    SetOfLiteralSets::~SetOfLiteralSets()
    {
    }
    SetOfLiteralSets SetOfLiteralSets::operator=(const SetOfLiteralSets &s) const
    {
        return SetOfLiteralSets(s);
    }

    bool SetOfLiteralSets::operator==(const SetOfLiteralSets &s) const
    {
        return set_type == s.set_type && sets == s.sets;
    }
    bool SetOfLiteralSets::operator<(const SetOfLiteralSets &s) const
    {
        assert(set_type == s.set_type);
        if (sets.size() == s.sets.size())
        {
            return sets < s.sets;
        }
        return sets.size() < s.sets.size();
    }
    std::ostream &operator<<(std::ostream &os, const SetOfLiteralSets &s)
    {
        os << "SET{";
        for (auto c : s.sets)
        {
            os << c;
        }
        os << "}";
        return os;
    }
    size_t SetOfLiteralSets::size() const
    {
        return sets.size();
    }
    const std::set<LiteralSet> SetOfLiteralSets::get_sets() const
    {
        return sets;
    }
    void SetOfLiteralSets::add_set(LiteralSet c)
    {
        assert(c.get_set_type() == set_type);
        this->sets.insert(c);
    }
    bool SetOfLiteralSets::contains_set(LiteralSet c) const
    {
        auto res = this->sets.find(c);
        return res != this->sets.end();
    }
    bool SetOfLiteralSets::is_subset_eq_of(const SetOfLiteralSets &s) const
    {
        if (this->size() > s.size())
        {
            return false;
        }
        for (LiteralSet c : this->sets)
        {
            if (!s.contains_set(c))
            {
                return false;
            }
        }
        return true;
    }
    SetOfLiteralSets SetOfLiteralSets::set_minus(const SetOfLiteralSets &l) const
    {
        std::set<LiteralSet> result;
        set_difference(
            this->sets.begin(), this->sets.end(),
            l.sets.begin(), l.sets.end(),
            inserter(result, result.end()));

        Layer lnew = Layer();
        for (auto clause : result)
        {
            lnew.add_set(clause);
        }
        return lnew;
    }

    Layer::Layer() : SetOfLiteralSets(std::set<LiteralSet>(), SetType::CLAUSE)
    {
    }

    Layer::Layer(const Layer &l) : SetOfLiteralSets(l.sets, SetType::CLAUSE)
    {
    }
    Layer::Layer(const std::set<LiteralSet> c) : SetOfLiteralSets(c, SetType::CLAUSE)
    {
        for (LiteralSet ls : c)
        {
            assert(ls.is_clause());
            //assert(ls.is_unit());
        }
    }
    Layer Layer::operator=(const Layer &l) const
    {
        return Layer(l);
    }
    std::ostream &operator<<(std::ostream &os, const Layer &l)
    {
        os << "Layer{";
        for (auto c : l.sets)
        {
            os << c;
        }
        os << "}";
        return os;
    }

    void Layer::add_set(LiteralSet c)
    {
        assert(c.is_clause());
        //assert(c.is_unit());
        this->sets.insert(c);
    }

    Layer Layer::set_minus(const Layer &l) const
    {
        std::set<LiteralSet> result;
        set_difference(
            this->sets.begin(), this->sets.end(),
            l.sets.begin(), l.sets.end(),
            inserter(result, result.end()));

        Layer lnew = Layer();
        for (auto clause : result)
        {
            lnew.add_set(clause);
        }
        return lnew;
    }

    std::pair<LiteralSet, bool> PDRSearch::extend(LiteralSet s, Layer L)
    {
        std::cout << "e1: Call to extend s=" << s << ",  L=" << L << std::endl;
        // input condition
        assert(!s.models(L));
        
        auto A = this->task_proxy.get_operators();
        //  Pseudocode 3

        // line 2
        Layer Ls = Layer();
        SetOfLiteralSets Rnoop = SetOfLiteralSets(SetType::CUBE);

        std::set<SetOfLiteralSets> Reasons;
        for (LiteralSet c : L.get_sets())
        {
            if (!s.models(c))
            {
                Ls.add_set(c);
                // line 3
                Rnoop.add_set(c.invert());
            }
        }
        std::cout << "e2: Lˢ =" << Ls << std::endl;
        std::cout << "e3: Rₙₒₒₚ=" << Rnoop << std::endl;

        // line 4
        assert(Rnoop.size() > 0);
        // line 5
        Reasons.insert(Rnoop);
        std::cout << "e5: ℛ = " << Reasons << std::endl;
        // line 7
        for (auto a : A)
        {
            LiteralSet pre_sa = SetType::CLAUSE;
            for (auto fact : a.get_preconditions())
            {
                Literal l = Literal::from_fact(fact);
                // line 8
                if (!s.models(LiteralSet(l, SetType::CUBE)))
                {
                    pre_sa.add_literal(l);
                }
            }
            std::cout << "e8: preₛᵃ=" << pre_sa << std::endl;
            // line 9
            LiteralSet eff_a = LiteralSet(SetType::CUBE);
            LiteralSet t = LiteralSet(s);
            for (auto eff_proxy : a.get_effects())
            {
                // build eff_a for later
                Literal l = Literal::from_fact(eff_proxy.get_fact());
                eff_a.add_literal(l);

                // apply eff_a to t
                t.apply_literal(l);
            }
            std::cout << "e9: effₐ=" << eff_a << std::endl;
            std::cout << "e9: t=" << t << std::endl;
            // line 10
            SetOfLiteralSets Lt = SetOfLiteralSets(SetType::CLAUSE);
            for (LiteralSet c : L.get_sets())
            {
                if (!t.models(c))
                {
                    Lt.add_set(c);
                }
            }
            std::cout << "e10: Lᵗ = " << Lt << std::endl;

            // line 11 & 12
            if (pre_sa.size() == 0 && Lt.size() == 0)
            {
                std::cout << "e12: Successor t found: t = " << t << std::endl;
                // output condition of successor
                assert(t.models(L));
                return std::pair<LiteralSet, bool>(t, true);
            }

            // line 13 & 14
            std::cout << "e13: Lˢ ⊆ Lᵗ = " << Ls.is_subset_eq_of(Lt)
                      << ", Lˢ=" << Ls << ", Lᵗ=" << Lt << std::endl;
            if (Ls.is_subset_eq_of(Lt))
            {
                continue;
            }
            // line 15
            else
            {
                // line 16
                // Comment: In the pseudocode, the arrow should be pointing left.
                SetOfLiteralSets Lt0 = SetOfLiteralSets(SetType::CLAUSE);
                for (auto c : Lt.get_sets())
                {
                    if (c.set_intersect(pre_sa).size() == 0)
                    {
                        Lt0.add_set(c);
                    }
                }
                std::cout << "e16: Lᵗ₀= " << Lt0 << std::endl;
                // line 17
                SetOfLiteralSets R_a = SetOfLiteralSets(SetType::CUBE);
                for (auto l : pre_sa.get_literals())
                {
                    R_a.add_set(LiteralSet(l.invert(), SetType::CUBE));
                }
                for (auto c : Lt0.get_sets())
                {
                    for (auto l : c.get_literals())
                    {
                        if (!eff_a.contains_literal(l.invert()))
                        {
                            R_a.add_set(LiteralSet(l, SetType::CUBE));
                        }
                    }
                }
                std::cout << "e17: Rₐ=" << R_a << std::endl;
                // line 18
                Reasons.insert(R_a);
            }
            std::cout << "e19: ℛ = " << Reasons << std::endl;
        }

        // line 20
        LiteralSet r = LiteralSet(SetType::CUBE);
        // line 21
        std::vector<SetOfLiteralSets> R = std::vector<SetOfLiteralSets>(Reasons.begin(), Reasons.end());
        std::sort(R.begin(), R.end(), [](const SetOfLiteralSets &f, const SetOfLiteralSets &l)
                  { return f.size() < l.size(); });

        std::cout << "e21: ℛ = " << R << std::endl;
        for (auto Ra : R)
        {
            // line 22
            std::cout << "e22: Rₐ="<< Ra <<std::endl; 
            
            LiteralSet ra = LiteralSet(SetType::CUBE);
            for (auto ra_cur : Ra.get_sets())
            {
                if (ra.size() == 0 || r.set_union(ra).size() > r.set_union(ra_cur).size())
                {
                    ra = ra_cur;
                }
            }
            r = r.set_union(ra);
        }
        std::cout << "e23: r=" << r << std::endl;

        // line 25 - 27 optional

        // line 29
        assert(r.size() > 0);
        std::cout << "e29: No successor t found, reason: r = " << r << std::endl;
        // output condition of reason.
        assert(r.is_subset_eq_of(s));
        return std::pair<LiteralSet, bool>(r, false);
    }

    PDRSearch::PDRSearch(const Options &opts) : SearchEngine(opts)
    {
    }

    Layer *PDRSearch::get_layer(long unsigned int i)
    {
        assert(i <= layers.size() + 1);

        if (layers.size() > i)
        {
            return &layers[i];
        }
        else if (i == 0)
        {
            Layer l0 = Layer();
            auto g = this->task_proxy.get_goals();
            for (size_t i = 0; i < g.size(); i++)
            {
                l0.add_set(LiteralSet(Literal::from_fact(g[i]), SetType::CLAUSE));
            }
            this->layers.insert(this->layers.end(), l0);
            return &layers[i];
        }
        else
        {
            Layer l_i = Layer();
            this->layers.insert(this->layers.end(), l_i);

            // TODO: initialize layer with heuristic here
            return &layers[i];
        }
    }

    void PDRSearch::initialize()
    {
        auto L0 = *get_layer(0);
    }

    void PDRSearch::print_statistics() const
    {
        statistics.print_detailed_statistics();
        search_space.print_statistics();
    }

    SearchStatus PDRSearch::step()
    {
        std::cout << "------------------" << std::endl;
        std::cout << "Step " << iteration << " of PDR search" << std::endl;
        // line 3
        const int k = iteration;
        iteration += 1;

        std::cout << "4: Path construction" << std::endl;
        // line 5
        auto initial_state = this->task_proxy.get_initial_state();
        auto s_i = from_state(initial_state);
        if (s_i.models(*get_layer(k)))
        {
            std::cout << "5: " << s_i << " ⊧ " << *get_layer(k) << std::endl;

            // line 6
            std::cout
                << "6: Initialize Q " << Obligation(s_i, k) << std::endl;
            std::priority_queue<Obligation> Q;
            Q.push(Obligation(s_i, k));

            // line 7
            while (!Q.empty())
            {

                // line 8
                Obligation si = Q.top();
                Q.pop();
                std::cout << "8: Pop obligation from queue: (s, i) = " << si << std::endl;
                int i = si.get_priority();
                auto s = si.get_state();
                // line 9
                if (i == 0)
                {
                    // line 10
                    std::cout << "10: Path found" << std::endl;
                    return SearchStatus::SOLVED;
                }

                // line 11
                // if extend(s, L_i-1) returns a successor state t
                auto extended = extend(s, *get_layer(i - 1));
                if (extended.second)
                {
                    // extend returns a successor state t
                    LiteralSet t = extended.first;
                    // line 12
                    Q.push(si);
                    std::cout << "12: Add obligation to queue: " << si << std::endl;
                    Q.push(Obligation(t, si.get_priority() - 1));
                    std::cout << "12: Add obligation to queue: " << Obligation(t, si.get_priority() - 1) << std::endl;
                }
                else
                {
                    // line 14 extend returns a reason r
                    LiteralSet r = extended.first;
                    // line 15
                    for (int j = 0; j <= i; j++)
                    {
                        auto L_j = get_layer(j);
                        std::cout << "15: Push clause to L_" << j << ": c = " << r.invert() << std::endl;
                        L_j->add_set(r.invert());
                        std::cout << "15: L_" << j << " = " << *L_j << std::endl;
                    }

                    // line 18
                    if (i < k)
                    {
                        // line 19
                        Q.push(Obligation(s, i + 1));
                    }
                }
            }
        }

        // Clause propagation
        auto A = this->task_proxy.get_operators();

        std::cout << "22: Clause propagation" << std::endl;
        // line 22
        for (int i = 1; i <= k + 1; i++)
        {
            std::cout << "22: foreach i = " << i << " of " << k + 1 << std::endl;
            // line 23
            auto Li1 = *get_layer(i - 1);
            auto Li = *get_layer(i);
            for (auto c : Li1.set_minus(Li).get_sets())
            {
                std::cout << "23: foreach c in Li1 \\ Li: c = " << c << std::endl;
                // line 25
                auto s_c_temp = c.invert();
                auto s_c = LiteralSet(s_c_temp.get_literals(), SetType::CUBE);
                // line 26
                bool models = true;
                for (auto a : A)
                {
                    // build pre_a from preconditions
                    LiteralSet pre_a = LiteralSet(std::set<Literal>(), SetType::CUBE);
                    for (auto prec : a.get_preconditions())
                    {
                        pre_a.add_literal(Literal::from_fact(prec));
                    }
                    // build apply(s_c, a)
                    LiteralSet applied = LiteralSet(s_c);
                    for (auto a_i : a.get_effects())
                    {
                        applied.apply_literal(Literal::from_fact(a_i.get_fact()));
                    }
                    if (s_c.models(pre_a) && applied.models(*get_layer(i - 1)))
                    {
                        models = false;
                        break;
                    }
                }
                if (!models)
                {
                    std::cout << "26: !models" << std::endl;
                    // line 27
                    get_layer(i)->add_set(c);
                    std::cout << "27: Push clause to L_" << i << ": c = " << c << ", L = " << *get_layer(i) << std::endl;
                }
            }
            std::cout << "29: L_" << i - 1
                      << *get_layer(i - 1)
                      << "== L_" << i
                      << *get_layer(i) << ": -> "
                      << (*get_layer(i - 1) == *get_layer(i))
                      << std::endl;
            if (*get_layer(i - 1) == *get_layer(i))
            {
                // line 30
                std::cout << "30: No plan possible" << std::endl;
                return SearchStatus::FAILED;
            }
        }

        std::cout << "------------------" << std::endl;
        return SearchStatus::IN_PROGRESS;
    }

    LiteralSet PDRSearch::from_state(const State &s) const
    {
        s.unpack();

        int i = 0;
        std::set<Literal> result;
        for (auto value : s.get_unpacked_values())
        {
            Literal v = Literal(i, value);
            result.insert(result.begin(), v);
            i += 1;
        }
        LiteralSet c = LiteralSet(result, SetType::CUBE);

        // TODO: insert negated litrals
        auto vars = this->task_proxy.get_variables();
        int variable_index = 0;
        for (auto var : vars)
        {
            int dom_size = var.get_domain_size();
            for (int i = 0; i < dom_size; i++)
            {
                Literal l = Literal(variable_index, i);
                if (!c.contains_literal(l))
                {
                    c.add_literal(l.invert());
                }
            }
            variable_index += 1;
        }
        return c;
    }

    void add_options_to_parser(OptionParser &parser)
    {
        SearchEngine::add_options_to_parser(parser);
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
