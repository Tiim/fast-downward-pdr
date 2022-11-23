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
        os << "(" << (l.positive ? "" : "¬") << l.variable << "," << l.value << ")";
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

    LiteralSet::LiteralSet()
    {
    }
    LiteralSet::LiteralSet(Literal v)
    {
        literals.insert(v);
    }
    LiteralSet::LiteralSet(const LiteralSet &s) : clause(s.clause), literals(s.literals)
    {
    }

    LiteralSet::LiteralSet(std::set<Literal> init_literals, bool is_clause) : clause(is_clause), literals(init_literals)
    {
    }

    LiteralSet LiteralSet::operator=(const LiteralSet &s) const
    {
        return LiteralSet(s.literals, s.clause);
    }

    bool LiteralSet::operator==(const LiteralSet &s) const
    {
        if (clause == s.clause)
        {
            return literals == s.literals;
        }
        return false;
    }
    bool LiteralSet::operator<(const LiteralSet &b) const
    {
        if (!(literals < b.literals) && !(b.literals < literals))
        {
            return clause < b.clause;
        }
        return literals < b.literals;
    }
    std::ostream &operator<<(std::ostream &os, const LiteralSet &ls)
    {
        os << "LiteralSet: {";
        bool first = true;
        for(auto c : ls.get_literals())
        {
            if (!first && !ls.clause)
            {
                os << " ∧ ";
            } else if (!first && ls.clause)
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

    LiteralSet LiteralSet::invert() const
    {
        std::set<Literal> new_set;
        for (auto l : literals)
        {
            new_set.insert(l.invert());
        }
        return LiteralSet(new_set, !clause);
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

    bool LiteralSet::is_subset_eq_of(const LiteralSet &ls) const
    {
        assert(clause == ls.clause);
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
        assert(clause == s.clause);
        LiteralSet tmp = LiteralSet(literals, clause);
        for (auto l : s.literals)
        {
            tmp.add_literal(l);
        }
        return tmp;
    }

    LiteralSet LiteralSet::set_intersect(const LiteralSet &s) const
    {
        assert(clause == s.clause);
        std::set<Literal> output_set;
        std::set_intersection(
            literals.begin(), literals.end(),
            s.literals.begin(), s.literals.end(),
            output_set.begin());
        return LiteralSet(output_set, clause);
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
        for (auto c : l.get_clauses())
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
        os << "Ob("<< o.state << "," << o.priority << ")";
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

    Layer::Layer()
    {
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
    bool Layer::operator==(const Layer &l) const
    {
        return clauses == l.clauses;
    }
    bool Layer::operator<(const Layer &l) const
    {
        return clauses < l.clauses;
    }
    std::ostream &operator<<(std::ostream &os, const Layer &l)
    {
        os << "Layer{";
        for (auto c : l.clauses)
        {
            os << c;
        }
        os << "}";
        //os << "Layer: {" << c.clauses << "}";
        return os;
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

    Layer Layer::set_minus(const Layer &l) const
    {
        std::set<LiteralSet> result;
        set_difference(
            this->clauses.begin(), this->clauses.end(),
            l.clauses.begin(), l.clauses.end(),
            inserter(result, result.end()));

        Layer lnew = Layer();
        for (auto clause : result)
        {
            lnew.add_clause(clause);
        }
        return lnew;
    }

    std::pair<LiteralSet, bool> PDRSearch::extend(LiteralSet s, Layer L)
    {
        auto A = this->task_proxy.get_operators();
        //  Pseudocode 3

        // line 2
        Layer Ls = Layer();
        Layer Rnoop = Layer();

        std::set<Layer> Reasons;
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
                if (!s.models(LiteralSet(l)))
                {
                    pre_sa.add_literal(l);
                }
            }
            LiteralSet eff_a;
            for (auto eff_proxy : a.get_effects())
            {
                Literal l = Literal::from_fact(eff_proxy.get_fact());
                eff_a.add_literal(l);
            }
            // line 9
            LiteralSet t = LiteralSet(s);
            for (auto effect : a.get_effects())
            {
                FactProxy fact = effect.get_fact();
                t.apply_literal(Literal::from_fact(fact));
            }

            // line 10
            Layer Lt = Layer();
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
                return std::pair<LiteralSet, bool>(t, true);
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
                // Comment: In the pseudocode, the arrow should be pointing left.
                Layer Lt0 = Layer();
                for (auto c : Lt.get_clauses())
                {
                    if (c.set_intersect(pre_sa).size() == 0)
                    {
                        Lt0.add_clause(c);
                    }
                }
                // line 17
                Layer R_a = Layer();
                for (auto l : pre_sa.get_literals())
                {
                    R_a.add_clause(LiteralSet(l.invert()));
                }
                for (auto c : Lt0.get_clauses())
                {
                    for (auto l : c.get_literals())
                    {
                        if (!eff_a.contains_literal(l.invert()))
                        {
                            R_a.add_clause(LiteralSet(l));
                        }
                    }
                }
                // line 18
                Reasons.insert(R_a);
            }
        }

        // line 20
        LiteralSet r = LiteralSet();
        // line 21
        std::vector<Layer> R = std::vector<Layer>(Reasons.begin(), Reasons.end());
        std::sort(R.begin(), R.end(), [](const Layer &f, const Layer &l)
                  { return f.size() > l.size(); });
        for (auto Ra : R)
        {
            // line 22
            LiteralSet ra;
            for (auto ra_cur : Ra.get_clauses())
            {
                if (ra.size() == 0 || ra.size() > r.set_union(ra_cur).size())
                {
                    ra = ra_cur;
                }
            }
            r = r.set_union(ra);
        }

        // line 25 - 27 optional

        // line 29
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
                l0.add_clause(LiteralSet(Literal::from_fact(g[i])));
            }
            this->layers.insert(this->layers.begin(), l0);
            return &layers[i];
        }
        else
        {
            Layer l_i = Layer();
            this->layers.insert(this->layers.begin(), l_i);

            // TODO: initialize layer with heuristic here

            return &layers[i];
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
        std::cout << "------------------" << std::endl;
        std::cout << "Step " << iteration << " of PDR search" << std::endl;
        // line 3
        const int k = iteration;
        iteration += 1;

        // line 5
        auto initial_state = this->task_proxy.get_initial_state();
        auto s_i = from_state(initial_state);
        if (s_i.models(*get_layer(k)))
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
                int i = si.get_priority();
                auto s = si.get_state();
                // line 9
                if (i == 0)
                {
                    // line 10
                    return SearchStatus::SOLVED;
                }

                // line 11 TODO
                // if extend(s, L_i-1) returns a successor state t
                auto extended = extend(s, *get_layer(i - 1));
                if (extended.second)
                {
                    // extend returns a successor state t
                    LiteralSet t = extended.first;
                    // line 12
                    Q.push(si);
                    Q.push(Obligation(t, si.get_priority() - 1));
                }
                else
                {
                    // line 14 extend returns a reason r
                    LiteralSet r = extended.first;
                    // line 15
                    for (int j = 0; j <= i; j++)
                    {
                        auto L_j = get_layer(j);
                        L_j->add_clause(r.invert());
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

        // line 22
        for (int i = 1; i <= k + 1; i++)
        {
            // line 23
            auto Li1 = *get_layer(i-1);
            auto Li = *get_layer(i);
            for (auto c : Li1.set_minus(Li).get_clauses())
            {
                // line 25
                auto s_c_temp = c.invert();
                auto s_c = LiteralSet(s_c_temp.get_literals(), false);
                // line 26
                bool models = true;
                for (auto a : A)
                {
                    LiteralSet pre_a = LiteralSet(std::set<Literal>(), false);
                    for (auto prec : a.get_preconditions())
                    {
                        pre_a.add_literal(Literal::from_fact(prec));
                    }
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
                if (models)
                {
                    // line 27
                    get_layer(i)->add_clause(c);
                }
            }
            if (*get_layer(i - 1) == *get_layer(i))
            {
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
            i += 1;
            Literal v = Literal(i, value);
            result.insert(result.begin(), v);
        }
        LiteralSet c = LiteralSet(result, false);

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
}
