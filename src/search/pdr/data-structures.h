#ifndef PDR_DATA_STRUCTURES_H
#define PDR_DATA_STRUCTURES_H

#include <cstddef>
#include <vector>
#include <set>
#include <memory>
#include <optional>
#include <queue>

#include "../task_proxy.h"
#include "constants.h"

namespace pdr_search
{
  enum SetType
  {
    CUBE,
    CLAUSE,
  };
  class Literal;
  class LiteralHash;
  class LiteralSet;
  class Obligation;
  class SetOfLiteralSets;
  class Layer;

  class Literal
  {
  private:
    int variable;
    int value;
    bool positive = true;
    FactProxy fact;

  public:
    Literal(int variable, int value, const FactProxy &fp);
    Literal(int variable, int value, bool positive, const FactProxy &fp);
    Literal(const Literal &l);
    Literal &operator=(const Literal &l);
    bool operator==(const Literal &l) const;
    bool operator<(const Literal &b) const;
    bool is_positive() const;
    friend std::ostream &operator<<(std::ostream &os, const Literal &l);
    Literal invert() const;
    Literal neg() const;
    Literal pos() const;
    static Literal from_fact(FactProxy f);
    std::size_t hash() const;
  };

  struct LiteralHash 
  {
    std::size_t operator () (Literal const &v) const;
  };

  // A set of literals,
  // can represent a clause (disjunction ∨)
  // or a cube (conjunction ∧)
  class LiteralSet
  {
  private:
    SetType set_type;
    std::unordered_set<Literal, LiteralHash> literals;

  public:
    LiteralSet(SetType type);
    LiteralSet(const Literal &v, SetType type);
    LiteralSet(const LiteralSet &s);
    LiteralSet(const std::unordered_set<Literal, LiteralHash> &init_literals, SetType type);
    LiteralSet &operator=(const LiteralSet &s);
    bool operator==(const LiteralSet &s) const;
    bool operator!=(const LiteralSet &s) const;
    friend std::ostream &operator<<(std::ostream &os, const LiteralSet &ls);
    const std::unordered_set<Literal, LiteralHash> &get_literals() const;
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
    void apply_literal(const Literal &l);
    void apply_cube(const LiteralSet &l);
    bool contains_literal(const Literal &l) const;
    bool is_subset_eq_of(const LiteralSet &ls) const;
    LiteralSet set_union(const LiteralSet &s) const;
    size_t set_intersect_size(const LiteralSet &s) const;
    LiteralSet set_minus(const LiteralSet &s) const;

    // Returns true if this LiteralSet is a model
    // of s.
    //
    // should only be called for state s and clause c
    // with "s ⊧ c".
    bool models(const LiteralSet &c) const;
    // returns true if this literal set models every clause in the layer
    bool models(const Layer &l) const;


    std::size_t hash() const;
  };

  struct LiteralSetHash 
  {
    std::size_t operator () (LiteralSet const &v) const;
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
    Obligation(const LiteralSet &s, int priority, std::shared_ptr<Obligation> parent);
    Obligation(const Obligation &o);
    Obligation &operator=(const Obligation &o);
    friend std::ostream &operator<<(std::ostream &os, const Obligation &o);
    int get_priority() const;
    LiteralSet get_state() const;
    bool operator<(const Obligation &o) const;
    const std::shared_ptr<Obligation> get_parent() const;
  };

  class SetOfLiteralSets
  {
  protected:
    SetType set_type;
    std::unordered_set<LiteralSet, LiteralSetHash> sets;

  public:
    SetOfLiteralSets();
    SetOfLiteralSets(SetType type);
    SetOfLiteralSets(const SetOfLiteralSets &s);
    SetOfLiteralSets(const std::unordered_set<LiteralSet, LiteralSetHash> &sets, SetType type);
    virtual ~SetOfLiteralSets();
    SetOfLiteralSets &operator=(const SetOfLiteralSets &l);
    bool operator==(const SetOfLiteralSets &s) const;
    friend std::ostream &operator<<(std::ostream &os, const SetOfLiteralSets &l);
    size_t size() const;
    const std::unordered_set<LiteralSet, LiteralSetHash> get_sets() const;
    void add_set(const LiteralSet &s);
    bool contains_set(const LiteralSet &s) const;
    bool is_subset_eq_of(const SetOfLiteralSets &s) const;
    SetOfLiteralSets set_minus(const SetOfLiteralSets &s) const;
    std::size_t hash() const;
  };

  struct SetOfLiteralSetsHash 
  {
    std::size_t operator () (SetOfLiteralSets const &v) const;
  };

  class Layer 
  {
  private:
     std::shared_ptr<Layer> parent;
     std::shared_ptr<Layer> child;
     std::unordered_set<LiteralSet, LiteralSetHash> __sets;
  public:
    Layer(std::shared_ptr<Layer> child, std::shared_ptr<Layer> parent);
    Layer(const Layer &l);
    Layer(const std::unordered_set<LiteralSet> &clauses,std::shared_ptr<Layer> child, std::shared_ptr<Layer> parent);
    Layer &operator=(const Layer &l);
    friend std::ostream &operator<<(std::ostream &os, const Layer &l);
    void set_child(std::shared_ptr<Layer> c);

    const std::shared_ptr<std::unordered_set<LiteralSet, LiteralSetHash>> get_sets() const;
    bool contains_set(const LiteralSet &ls) const;
    std::shared_ptr<Layer> get_child() const;
    // Automatically adds the set also to the parents of the set (L_{j}) for j = 0,...,i-1
    // See Suda, 3.6.1 Representation of the Layers
    void add_set(const LiteralSet &c);
    // Returns a list of literal sets that are in the current layer but not in its child layer.
    const std::shared_ptr<std::unordered_set<LiteralSet, LiteralSetHash>> get_delta() const;

    bool is_subset_eq_of(const Layer &s) const;
    size_t size() const;

    void simplify();
    void print_stack() const;

  };

}

#endif
