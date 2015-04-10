#pragma once
#include "common_defines.hpp"

#ifdef new
#undef new
#endif

#include <tuple>
using std::tuple;
using std::make_tuple;

namespace impl
{
  template <int N> struct for_each_tuple_impl
  {
    template <class Tuple, class ToDo> void operator() (Tuple & t, ToDo & toDo)
    {
      toDo(std::get<N>(t), N);
      for_each_tuple_impl<N-1> c;
      c(t, toDo);
    }
  };

  template <> struct for_each_tuple_impl<-1>
  {
    template <class Tuple, class ToDo> void operator() (Tuple &, ToDo &) {}
  };
}

template <class Tuple, class ToDo>
void for_each_tuple(Tuple & t, ToDo & toDo)
{
  impl::for_each_tuple_impl<std::tuple_size<Tuple>::value-1> c;
  c(t, toDo);
}

#ifdef DEBUG_NEW
#define new DEBUG_NEW
#endif
