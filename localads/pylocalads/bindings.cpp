#include "localads/campaign.hpp"
#include "localads/campaign_serialization.hpp"

// #include "pyhelpers/pair.hpp"
#include "pyhelpers/vector_uint8.hpp"
#include "pyhelpers/vector_list_conversion.hpp"

#include <boost/python.hpp>
#include <boost/python/suite/indexing/vector_indexing_suite.hpp>


using namespace localads;

namespace
{
std::vector<uint8_t> PySerialize(boost::python::list const & cs)
{
  auto const campaigns = python_list_to_std_vector<Campaign>(cs);
  return Serialize(campaigns);
}

boost::python::list PyDeserialize(std::vector<uint8_t> const & blob)
{
  auto const campaigns = Deserialize(blob);
  return std_vector_to_python_list(campaigns);
}
}  // namespace


BOOST_PYTHON_MODULE(pylocalads)
{
  using namespace boost::python;

  // Register the to-python converters.
  to_python_converter<vector<uint8_t>, vector_uint8t_to_str>();
  vector_uint8t_from_python_str();

  class_<Campaign>("Campaign", init<uint32_t, uint16_t, uint8_t, bool>())
      .def_readonly("m_featureId", &Campaign::m_featureId)
      .def_readonly("m_iconId", &Campaign::m_iconId)
      .def_readonly("m_daysBeforeExpired", &Campaign::m_daysBeforeExpired)
      .def_readonly("m_priorityBit", &Campaign::m_priorityBit);

  class_<std::vector<Campaign>>("CampaignList")
      .def(vector_indexing_suite<std::vector<Campaign>>());

  def("serialize", PySerialize);
  def("deserialize", PyDeserialize);
}
