#include "data-structures.h"
#include <cstddef>
#include <iterator>
#include <ostream>

namespace pdr_search
{
  Literal::Literal(int var, int val, const FactProxy &f) : variable(var), value(val), fact(f)
  {
  }
  Literal::Literal(int var, int val, bool pos, const FactProxy &f) : variable(var), value(val), positive(pos), fact(f)
  {
  }
  Literal::Literal(const Literal &l) : variable(l.variable), value(l.value), positive(l.positive), fact(l.fact)
  {
  }
  Literal &Literal::operator=(const Literal &l)
  {
    variable = l.variable;
    value = l.value;
    fact = l.fact;
    return *this;
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
  bool Literal::is_positive() const
  {
    return positive;
  }
  std::ostream &operator<<(std::ostream &os, const Literal &l)
  {
    auto name = l.fact.get_name();
    auto color = (l.positive) ? COLOR_GREEN : COLOR_YELLOW;
    os << color << (l.positive ? "" : "¬") << "(";
    if (name.size() > 0)
    {
      // os << COLOR_GRAY;
      os << name;
      // os << "," << color;
    }
    // os << l.variable << "," << l.value;
    os << ")" COLOR_NONE;
    return os;
  }

  Literal Literal::invert() const
  {
    return Literal(variable, value, !positive, fact);
  }

  Literal Literal::neg() const
  {
    return Literal(variable, value, false, fact);
  }
  Literal Literal::pos() const
  {
    return Literal(variable, value, true, fact);
  }

  Literal Literal::from_fact(FactProxy fp)
  {
    auto factPair = fp.get_pair();
    return Literal(factPair.var, factPair.value, fp);
  }

  std::size_t Literal::hash() const 
  {
    utils::HashState hs;
    utils::feed(hs, variable);
    utils::feed(hs, value);
    utils::feed(hs, positive);
    return hs.get_hash64();
  }

  std::size_t LiteralHash::operator()(const Literal &v) const
  {
     return v.hash(); 
  }

  LiteralSet::LiteralSet(SetType type) : set_type(type)
  {
  }
  LiteralSet::LiteralSet(const Literal &v, SetType type) : set_type(type)
  {
    literals.insert(v);
  }
  LiteralSet::LiteralSet(const LiteralSet &s) : set_type(s.set_type), literals(s.literals)
  {
  }

  LiteralSet::LiteralSet(const std::unordered_set<Literal, LiteralHash> &init_literals, SetType type) : set_type(type), literals(init_literals)
  {
  }

  LiteralSet &LiteralSet::operator=(const LiteralSet &s)
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
  bool LiteralSet::operator!=(const LiteralSet &s) const
  {
    return !(*this == s);
  }


  std::ostream &operator<<(std::ostream &os, const LiteralSet &ls)
  {
    os << COLOR_RED "{";
    bool first = true;
    for (const auto &c : ls.literals)
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
      os << c << COLOR_RED;
    }
    os << "}" COLOR_NONE;
    return os;
  }

  const std::unordered_set<Literal, LiteralHash> &LiteralSet::get_literals() const
  {
    return literals;
  }
  SetType LiteralSet::get_set_type() const
  {
    return set_type;
  }

  LiteralSet LiteralSet::invert() const
  {
    std::unordered_set<Literal, LiteralHash> new_set;
    for (const auto &l : literals)
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

  LiteralSet LiteralSet::pos() const
  {
    std::unordered_set<Literal,LiteralHash> new_set;
    for (const auto &l : literals)
    {
      new_set.insert(l.pos());
    }
    return LiteralSet(new_set, set_type);
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
    if (contains_literal(l.invert()))
    {
      std::cout << "already contains literal: " << *this << " l=" << l << std::endl;
      assert(false);
    }
    literals.insert(literals.begin(), l);
  }

  void LiteralSet::remove_literal(Literal l)
  {
    literals.erase(l);
  }

  void LiteralSet::apply_literal(const Literal &l)
  {
    remove_literal(l.invert());
    add_literal(l);
  }

  void LiteralSet::apply_cube(const LiteralSet &l)
  {
    assert(l.is_cube());
    for (const auto &lit : l.get_literals())
    {
      apply_literal(lit);
    }
  }

  bool LiteralSet::contains_literal(const Literal &l) const
  {
    return literals.count(l) > 0;
  }

  bool LiteralSet::is_subset_eq_of(const LiteralSet &ls) const
  {
    if (size() > ls.size())
    {
      return false;
    }
    for (const Literal &l : literals)
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
    for (const auto &l : s.literals)
    {
      tmp.add_literal(l);
    }

    assert(is_subset_eq_of(tmp));
    assert(s.is_subset_eq_of(tmp));
    return tmp;
  }

  size_t LiteralSet::set_intersect_size(const LiteralSet &s) const
  {
    assert(set_type == s.set_type);
    size_t count = 0;

    const LiteralSet *small = this, *big = &s;
    if (this->size() >= s.size())
    {
      small = &s;
      big = this;
    }
    for (const auto &l : big->literals)
    {
      if (small->contains_literal(l))
      {
        count += 1; 
      }
    }

    return count;
  }

  LiteralSet LiteralSet::set_minus(const LiteralSet &s) const
  {
    std::unordered_set<Literal,LiteralHash> nliterals;
    for (const auto &l : literals)
    {
      if (!s.contains_literal(l))
      {
        nliterals.insert(l);
      }
    }
    auto output = LiteralSet(nliterals, set_type);
    assert(output.is_subset_eq_of(*this));
    return output;
  }

  bool LiteralSet::models(const LiteralSet &c) const
  {
    assert(is_cube());
    if (c.is_clause())
    {
      // rhs must contain at least one literal from the lhs
      for (const auto &cl : c.literals)
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
      // lhs must be more or equally strict than rhs
      return c.is_subset_eq_of(*this);
    }
  }

  bool LiteralSet::models(const Layer &l) const
  {
    const Layer *layer = &l;
    while (layer != nullptr)
    {
        auto delta = *layer->get_delta();
        for (const auto &c : delta)
        {
          if (!models(c))
          {
            return false;
          }
        }
        layer = layer->get_child().get();
    }
    return true;
  }

  std::size_t LiteralSet::hash() const
  {
    utils::HashState hs;
    utils::feed(hs, this->set_type);
    utils::feed(hs, this->literals.size());

    // for ordering independent hash function
    size_t literal_hash = 0;
    for (const auto &s: this->literals) {
        literal_hash ^= s.hash();
    }

    utils::feed(hs, literal_hash);
    return hs.get_hash64();
  }

  std::size_t LiteralSetHash::operator () (LiteralSet const &v) const
  {
    return v.hash();
  }

  Obligation::Obligation(const LiteralSet &s, int p, std::shared_ptr<Obligation> par) : parent(par), state(s), priority(p)
  {
    assert(s.is_cube());
  }
  Obligation::Obligation(const Obligation &o) : state(o.state), priority(o.priority)
  {
  }
  Obligation &Obligation::operator=(const Obligation &o)
  {
    priority = o.priority;
    state = o.state;
    return *this;
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

  const std::shared_ptr<Obligation> Obligation::get_parent() const
  {
    return parent;
  }

  SetOfLiteralSets::SetOfLiteralSets() : set_type(SetType::CUBE)
  {
  }
  SetOfLiteralSets::SetOfLiteralSets(SetType type) : set_type(type)
  {
  }
  SetOfLiteralSets::SetOfLiteralSets(const SetOfLiteralSets &s) : set_type(s.set_type), sets(s.get_sets())
  {
  }
  SetOfLiteralSets::SetOfLiteralSets(const std::unordered_set<LiteralSet, LiteralSetHash> &s, SetType type) : set_type(type), sets(s)
  {
    for (const auto &set : s)
    {
      assert(set.get_set_type() == type);
    }
  }
  SetOfLiteralSets::~SetOfLiteralSets()
  {
  }
  SetOfLiteralSets &SetOfLiteralSets::operator=(const SetOfLiteralSets &s)
  {
    set_type = s.set_type;
    sets = s.get_sets();
    return *this;
  }

  bool SetOfLiteralSets::operator==(const SetOfLiteralSets &s) const
  {
    return set_type == s.set_type && get_sets() == s.get_sets();
  }

  std::ostream &operator<<(std::ostream &os, const SetOfLiteralSets &s)
  {
    os << COLOR_CYAN "Set{";
    bool first = true;
    for (const auto &c : s.get_sets())
    {
      if (first)
      {
        first = false;
      }
      else
      {
        os << COLOR_CYAN ", ";
      }
      os << c;
    }
    os << COLOR_CYAN "}" COLOR_NONE;
    return os;
  }
  size_t SetOfLiteralSets::size() const
  {
    return get_sets().size();
  }

  const std::unordered_set<LiteralSet, LiteralSetHash> SetOfLiteralSets::get_sets() const
  {
    return sets;
  }

  void SetOfLiteralSets::add_set(const LiteralSet &c)
  {
    assert(c.get_set_type() == set_type);
    this->sets.insert(c);
  }

  bool SetOfLiteralSets::contains_set(const LiteralSet &c) const
  {
    auto s = this->get_sets();
    auto res = s.count(c);
    return res > 0;
  }

  bool SetOfLiteralSets::is_subset_eq_of(const SetOfLiteralSets &s) const
  {
    if (this->size() > s.size())
    {
      return false;
    }
    for (const LiteralSet &c : this->get_sets())
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
    auto se = get_sets();
    auto lse = l.get_sets();
    std::unordered_set<LiteralSet, LiteralSetHash> result;
    std::copy_if(
            se.begin(), se.end(), 
            std::inserter(result, result.end()), 
            [&lse] (LiteralSet ls) {return lse.count(ls) == 0;}
        );

    /* set_difference(  */
    /*     se.begin(), se.end(), */
    /*     lse.begin(), lse.end(), */
    /*     inserter(result, result.end())); */

    SetOfLiteralSets lnew = SetOfLiteralSets();
    for (const auto &clause : result)
    {
      lnew.add_set(clause);
    }
    assert(lnew.is_subset_eq_of(*this));
    return lnew;
  }

  std::size_t SetOfLiteralSets::hash() const 
  {
    utils::HashState hs;
    utils::feed(hs, this->set_type);
    utils::feed(hs, this->sets.size());

    // for ordering independent hash function
    size_t literal_hash = 0;
    for (const auto &s: this->sets) {
        literal_hash ^= s.hash();
    }

    utils::feed(hs, literal_hash);
    return hs.get_hash64();
  }

  std::size_t SetOfLiteralSetsHash::operator()(const SetOfLiteralSets &v) const
  {
     return v.hash(); 
  }

  Layer::Layer(std::shared_ptr<Layer> c, std::shared_ptr<Layer> p) : 
      parent(p), 
      child(c), 
      __sets(std::shared_ptr<std::unordered_set<LiteralSet, LiteralSetHash>>(new std::unordered_set<LiteralSet, LiteralSetHash>())) 
  {
  }

  Layer::Layer(const Layer &l) :  
      child(l.child), 
      parent(l.parent), 
      __sets(std::shared_ptr<std::unordered_set<LiteralSet, LiteralSetHash>>(new std::unordered_set<LiteralSet, LiteralSetHash>(*l.get_delta())))
  {
      
  }

  Layer::Layer(const std::unordered_set<LiteralSet> &c,std::shared_ptr<Layer> ci, std::shared_ptr<Layer> p ):
      parent(p),
      child(ci), 
      __sets(std::shared_ptr<std::unordered_set<LiteralSet, LiteralSetHash>>(new std::unordered_set<LiteralSet, LiteralSetHash>())) 
  {
    for (const LiteralSet &ls : c)
    {
      assert(ls.is_clause());
      add_set(ls);
    }
  }

  Layer &Layer::operator=(const Layer &l)
  {
    __sets = l.__sets;
    parent = l.parent;
    child = l.child;
    return *this;
  }

  std::ostream &operator<<(std::ostream &os, const Layer &l)
  {
    os << COLOR_CYAN "Layer{";
    bool first = true;
    auto sets = *l.get_sets();
    for (const auto &c : sets)
    {
      if (first)
      {
        first = false;
      }
      else
      {
        os << COLOR_CYAN ", ";
      }
      os << c;
    }
    os << COLOR_CYAN "}" COLOR_NONE;
    return os;
  }

  void Layer::set_child(std::shared_ptr<Layer> c)
  {
      this->child = c;
  }

  const std::shared_ptr<std::unordered_set<LiteralSet, LiteralSetHash>> Layer::get_sets() const 
  {
    std::shared_ptr<std::unordered_set<LiteralSet, LiteralSetHash>> sets(new std::unordered_set<LiteralSet, LiteralSetHash>());
    size_t total_size = 0;
    // calculate size of set
    const Layer *child = this;
    while (child != nullptr) {
        total_size += child->size();
        child = child->child.get();
    }
    sets->reserve(total_size);

    child = this;
    while (child != nullptr) {
        auto delta = child->get_delta();
        for (const LiteralSet &s : *delta) {
            sets->insert(s);
        }
        child = child->child.get();
    }
    return sets;
  }


  void Layer::print_stack() const 
  {
      if (this->parent) {
          this->parent->print_stack();
      }
      std::cout << " Layer "<< (this->parent?"(p)":"" )<<(this->child?"(c)":"");
      //std::cout << *this << std::endl;
      std::cout << "Layer delta: ";
      auto delta = *child->get_delta();
      for (const auto &s : delta) {
          std::cout << s << ", ";
      }
      std::cout << std::endl;


  }

  void Layer::add_set(const LiteralSet &c)
  {
    assert(c.is_clause());
    /* std::cout << "---- add_set ----" <<std::endl; */
    /* std::cout << "adding set "<<c<<std::endl; */
    /* std::cout << "Layer stack:" << std::endl; */
    /* this->print_stack(); */
    /* std::cout << "End Layer stack" << std::endl; */


    // don't insert if current or child layer already has the literalset
    bool child_already_has_set = false;
    Layer* child = this;
    while (child != nullptr) {
        auto sets = child->get_delta(); 
        if (sets->count(c) > 0) {
            child_already_has_set = true;
            break;
        }
        child = child->child.get();
    }

    if (!child_already_has_set) {
        this->__sets->insert(c);
    }        

    // erase from all parent layers
    // TODO: should be inside of if child_already_has_set
    std::shared_ptr<Layer> parent = this->parent;
    while (parent != nullptr) {
        parent->__sets->erase(c);
        parent = parent->parent;
    }
    
    /* std::cout << "-- After adding --" <<std::endl; */
    /* this->print_stack(); */

    /* std::cout << "---- end add_set ----" << std::endl; */

    //TODO: remove this for performance
    /* auto all_sets = get_sets(); */
    /* assert(all_sets.find(c) != all_sets.end()); */
    /* if (this->parent) { */
    /*     assert(this->is_subset_eq_of(*(this->parent))); */
    /* } */
  }


  const std::shared_ptr<std::unordered_set<LiteralSet, LiteralSetHash>> Layer::get_delta() const 
  {
      return this->__sets;
  }
  
  bool Layer::is_subset_eq_of(const Layer &s) const
  {
    if (this->size() > s.size())
    {
      /* std::cout<< "is_subset_eq_of (size)" << std::endl */
      /*     << *this << std::endl */
      /*     << s << std::endl; */
      return false;
    }
    auto sets = *this->get_sets();
    for (const LiteralSet &c : sets)
    {
      if (!s.contains_set(c))
      {
        /* std::cout<< "is_subset_eq_of " << std::endl */
        /*   << "literal that violates: " << c <<std::endl */
        /*   << "subset: " << *this << std::endl */
        /*   << "superset: " << s << std::endl; */
        return false;
      }
    }
    return true;
  }
  
  size_t Layer::size() const 
  {
      size_t sum = this->__sets->size();
      Layer *l = this->child.get();
      while (l != nullptr) {
          sum += l->__sets->size();
          l = l->child.get();
      }
      return sum;
  }


  bool Layer::contains_set(const LiteralSet &c) const
  {
    const Layer* child = this;
    while (child != nullptr) {
        auto sets = child->get_delta(); 
        if (sets->count(c) > 0) {
            return true;
        }
        child = child->child.get();
    }
    return false;
  }

  std::shared_ptr<Layer> Layer::get_child() const 
  {
    return this->child;
  }

  void Layer::simplify()
  {
    /*LiteralSet intersection = LiteralSet(SetType::CLAUSE);
    int i = 0;
    for (LiteralSet literal_set : sets)
    {
      literal_set.simplify();
      if (i == 0)
      {
        intersection = literal_set;
      }
      else
      {
        intersection = intersection.set_intersect(literal_set);
      }
      if (literal_set.size() <= 1) 
      {
        intersection = LiteralSet(SetType::CLAUSE);
        break;
      }
      i += 1;
    }*/
  }
}
