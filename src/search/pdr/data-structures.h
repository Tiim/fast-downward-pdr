#ifndef PDR_DATA_STRUCTURES_H
#define PDR_DATA_STRUCTURES_H

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
    Literal(int variable, int value, FactProxy fp);
    Literal(int variable, int value, bool positive, FactProxy fp);
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
    LiteralSet &operator=(const LiteralSet &s);
    bool operator==(const LiteralSet &s) const;
    bool operator!=(const LiteralSet &s) const;
    bool operator<(const LiteralSet &s) const;
    friend std::ostream &operator<<(std::ostream &os, const LiteralSet &ls);
    std::set<Literal> &get_literals();
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
    void apply_cube(LiteralSet l);
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

    void simplify();
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
    std::set<LiteralSet> sets;

  public:
    SetOfLiteralSets();
    SetOfLiteralSets(SetType type);
    SetOfLiteralSets(const SetOfLiteralSets &s);
    SetOfLiteralSets(const std::set<LiteralSet> sets, SetType type);
    virtual ~SetOfLiteralSets();
    SetOfLiteralSets &operator=(const SetOfLiteralSets &l);
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
     std::shared_ptr<Layer> parent;
  public:
    Layer(std::shared_ptr<Layer> parent);
    Layer(const Layer &l);
    Layer(const std::set<LiteralSet> clauses, std::shared_ptr<Layer> parent);
    Layer &operator=(const Layer &l);
    friend std::ostream &operator<<(std::ostream &os, const Layer &l);
    void add_set(LiteralSet c);
    // Lₜₕᵢₛ ∖ Lₗ
    Layer set_minus(const Layer &l) const;

    void simplify();
  };
}
#endif
