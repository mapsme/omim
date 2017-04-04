#pragma once

#include "std/vector.hpp"

// These headers are necessary for cross-python compilation.
// Python3 does not have PyString_* methods. One should use PyBytes_* instead.
// bytesobject.h contains a mappign from PyBytes_* to PyString_*.
// See https://docs.python.org/2/howto/cporting.html for more.
#include "stdlib.h"
#include "Python.h"
#include "bytesobject.h"

#include <boost/python.hpp>
#include <boost/python/suite/indexing/map_indexing_suite.hpp>

namespace
{
using namespace boost::python;

// Converts a vector<uint8_t> to/from Python str.
struct vector_uint8t_to_str
{
  static PyObject * convert(vector<uint8_t> const & v)
  {
    str s(reinterpret_cast<char const *>(v.data()), v.size());
    return incref(s.ptr());
  }
};

struct vector_uint8t_from_python_str
{
  vector_uint8t_from_python_str()
  {
    converter::registry::push_back(&convertible, &construct, type_id<vector<uint8_t>>());
  }

  static void * convertible(PyObject * obj_ptr)
  {
    if (!PyBytes_Check(obj_ptr))
      return nullptr;
    return obj_ptr;
  }

  static void construct(PyObject * obj_ptr, converter::rvalue_from_python_stage1_data * data)
  {
    const char * value = PyBytes_AsString(obj_ptr);
    if (value == nullptr)
      throw_error_already_set();
    void * storage =
        ((converter::rvalue_from_python_storage<vector<uint8_t>> *)data)->storage.bytes;
    new (storage) vector<uint8_t>(value, value + PyBytes_Size(obj_ptr));
    data->convertible = storage;
  }
};
}  // namespace
