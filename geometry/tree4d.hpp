#pragma once

#include "geometry/rect2d.hpp"
#include "geometry/point2d.hpp"

#include "base/stl_add.hpp"
#include "base/logging.hpp"

#include "std/sstream.hpp"

#include "3party/kdtree++/kdtree.hpp"


namespace m4
{
  template <typename T>
  struct TraitsDef
  {
    m2::RectD const LimitRect(T const & t) const
    {
      return t.GetLimitRect();
    }
  };

  template <class T, typename Traits = TraitsDef<T> >
  class Tree
  {
    class ValueT
    {
      void SetRect(m2::RectD const & r)
      {
        m_pts[0] = r.minX();
        m_pts[1] = r.minY();
        m_pts[2] = r.maxX();
        m_pts[3] = r.maxY();
      }

    public:
      T m_val;
      double m_pts[4];

      typedef double value_type;

      ValueT(T const & t, m2::RectD const & r) : m_val(t)
      {
        SetRect(r);
      }
      ValueT(T && t, m2::RectD const & r) : m_val(move(t))
      {
        SetRect(r);
      }

      bool IsIntersect(m2::RectD const & r) const
      {
        return !((m_pts[2] <= r.minX()) || (m_pts[0] >= r.maxX()) ||
                 (m_pts[3] <= r.minY()) || (m_pts[1] >= r.maxY()));
      }

      bool operator ==(ValueT const & r) const
      {
        return (m_val == r.m_val);
      }

      string DebugPrint() const
      {
        ostringstream out;

        out << DebugPrint(m_val) << ", ("
            << m_pts[0] << ", "
            << m_pts[1] << ", "
            << m_pts[2] << ", "
            << m_pts[3] << ")";

        return out.str();
      }

      double operator[](size_t i) const { return m_pts[i]; }

      m2::RectD GetRect() const { return m2::RectD(m_pts[0], m_pts[1], m_pts[2], m_pts[3]); }
    };

    typedef KDTree::KDTree<4, ValueT> TreeT;
    TreeT m_tree;

    // Do-class for rect-iteration in tree.
    template <class ToDo> class for_each_helper
    {
      m2::RectD const & m_rect;
      ToDo m_toDo;

    public:
      for_each_helper(m2::RectD const & r, ToDo toDo)
        : m_rect(r), m_toDo(toDo)
      {
      }

      bool ScanLeft(size_t plane, ValueT const & v) const
      {
        switch (plane & 3)    // % 4
        {
        case 2: return m_rect.minX() < v[2];
        case 3: return m_rect.minY() < v[3];
        default: return true;
        }
      }

      bool ScanRight(size_t plane, ValueT const & v) const
      {
        switch (plane & 3)  // % 4
        {
        case 0: return m_rect.maxX() > v[0];
        case 1: return m_rect.maxY() > v[1];
        default: return true;
        }
      }

      void operator() (ValueT const & v) const
      {
        if (v.IsIntersect(m_rect))
          m_toDo(v);
      }
    };

    template <class ToDo> for_each_helper<ToDo> GetFunctor(m2::RectD const & rect, ToDo toDo) const
    {
      return for_each_helper<ToDo>(rect, toDo);
    }

  protected:
    Traits m_traits;
    m2::RectD GetLimitRect(T const & t) const { return m_traits.LimitRect(t); }

  public:
    Tree(Traits const & traits = Traits()) : m_traits(traits) {}

    typedef T elem_t;

    void Add(T const & obj)
    {
      Add(obj, GetLimitRect(obj));
    }
    void Add(T && obj)
    {
      Add(move(obj), GetLimitRect(obj));
    }

    void Add(T const & obj, m2::RectD const & rect)
    {
      m_tree.insert(ValueT(obj, rect));
    }
    void Add(T && obj, m2::RectD const & rect)
    {
      m_tree.insert(ValueT(move(obj), rect));
    }

  private:
    template <class CompareT>
    void ReplaceImpl(T const & obj, m2::RectD const & rect, CompareT comp)
    {
      bool skip = false;
      vector<ValueT const *> isect;

      m_tree.for_each(GetFunctor(rect, [&] (ValueT const & v)
      {
        if (skip)
          return;

        switch (comp(obj, v.m_val))
        {
        case 1:
          isect.push_back(&v);
          break;
        case -1:
          skip = true;
          break;
        }
      }));

      if (skip)
        return;

      for (ValueT const * v : isect)
        m_tree.erase(*v);

      Add(obj, rect);
    }

  public:
    template <class CompareT>
    void ReplaceAllInRect(T const & obj, CompareT comp)
    {
      ReplaceImpl(obj, GetLimitRect(obj), [&comp] (T const & t1, T const & t2)
      {
        return comp(t1, t2) ? 1 : -1;
      });
    }

    template <class EqualT, class CompareT>
    void ReplaceEqualInRect(T const & obj, EqualT eq, CompareT comp)
    {
      ReplaceImpl(obj, GetLimitRect(obj), [&] (T const & t1, T const & t2)
      {
        if (eq(t1, t2))
          return comp(t1, t2) ? 1 : -1;
        else
          return 0;
      });
    }

    void Erase(T const & obj, m2::RectD const & r)
    {
      ValueT val(obj, r);
      m_tree.erase_exact(val);
    }

    void Erase(T const & obj)
    {
      ValueT val(obj, m_traits.LimitRect(obj));
      m_tree.erase_exact(val);
    }

    template <class ToDo>
    void ForEach(ToDo toDo) const
    {
      for (ValueT const & v : m_tree)
        toDo(v.m_val);
    }

    template <class ToDo>
    bool FindNode(ToDo const & toDo) const
    {
      for (ValueT const & v : m_tree)
        if (toDo(v.m_val))
          return true;

      return false;
    }

    template <class ToDo>
    void ForEachWithRect(ToDo toDo) const
    {
      for (ValueT const & v : m_tree)
        toDo(v.GetRect(), v.m_val);
    }

    template <class ToDo>
    void ForEachInRect(m2::RectD const & rect, ToDo toDo) const
    {
      m_tree.for_each(GetFunctor(rect, [&toDo] (ValueT const & v) { toDo(v.m_val); }));
    }

    bool IsEmpty() const { return m_tree.empty(); }

    size_t GetSize() const { return m_tree.size(); }

    void Clear() { m_tree.clear(); }

    string DebugPrint() const
    {
      ostringstream out;
      for (ValueT const & v : m_tree.begin())
        out << v.DebugPrint() << ", ";
      return out.str();
    }
  };

  template <typename T, typename Traits>
  string DebugPrint(Tree<T, Traits> const & t)
  {
    return t.DebugPrint();
  }
}
