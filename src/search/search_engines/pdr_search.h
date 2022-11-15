#ifndef SEARCH_ENGINES_PDR_SEARCH_H
#define SEARCH_ENGINES_PDR_SEARCH_H

#include "../search_engine.h"
#include "search_common.h"

#include <vector>
#include <set>
#include <memory>
#include <optional>

namespace options
{
    class OptionParser;
    class Options;
}

namespace pdr_search
{

    // only represent unit clause for now
    class Clause
    {
    private:
        const int variable;
        const int value = 1;

    public:
        Clause(int variable, int value);
        bool operator<(const Clause &b) const;

        static Clause FromFact(FactProxy fp);
    };

    class Layer
    {
    private:
        std::set<Clause> clauses_set;


    public:
        Layer(const Layer &l);
        Layer();
        ~Layer();

        // L_i <- L_i \cup \{c\}
        void add_clause(Clause c);
        // L_i \ L_l
        Layer without(Layer *l);

        bool models(State s);

        bool extend(State s, State *successor);
        
    };

    class PDRSearch : public SearchEngine
    {
        std::vector<Layer> layers;
        int iteration = 0;

    protected:
        virtual void initialize() override;
        virtual SearchStatus step() override;

    public:
        PDRSearch(const options::Options &opts);
        ~PDRSearch() = default;

        virtual void print_statistics() const override;

        void dump_search_space() const;
    };

    extern void add_options_to_parser(options::OptionParser &parser);
}

#endif
