//
//  test.cpp
//  coastline
//
//  Created by Sergey Yershov on 04.06.15.
//  Copyright (c) 2015 maps.me. All rights reserved.
//

#include <iostream>


#define BOOST_GEOMETRY_DEBUG_SEGMENT_IDENTIFIER
#define BOOST_GEOMETRY_DEBUG_HAS_SELF_INTERSECTIONS

#include <boost/geometry.hpp>
#include <boost/geometry/geometries/point_xy.hpp>
#include <boost/geometry/algorithms/detail/overlay/turn_info.hpp>
#include <boost/geometry/extensions/algorithms/dissolve.hpp>

namespace bg = boost::geometry;

typedef bg::model::d2::point_xy<double> point;
typedef bg::model::polygon<point> polygon;
typedef bg::model::multi_polygon<polygon> multi_polygon;

void CheckAndFix(polygon & geometry)
{
  typedef bg::rescale_policy_type<point>::type rescale_policy_type;
  rescale_policy_type robust_policy = bg::get_rescale_policy<rescale_policy_type>(geometry);

  typedef bg::detail::overlay::turn_info<point, bg::segment_ratio_type<point, rescale_policy_type>::type> turn_info;
  std::deque<turn_info> turns;
  bg::detail::disjoint::disjoint_interrupt_policy policy;

  bg::self_turns<bg::detail::overlay::assign_null_policy>(geometry, robust_policy, turns, policy);

  for(auto const &info : turns)
  {
    bool const both_union_turn =
    info.operations[0].operation == bg::detail::overlay::operation_union
    && info.operations[1].operation == bg::detail::overlay::operation_union;
    bool const both_intersection_turn =
    info.operations[0].operation == bg::detail::overlay::operation_intersection
    && info.operations[1].operation == bg::detail::overlay::operation_intersection;

    bool const valid = (both_union_turn || both_intersection_turn)
    && (info.method == bg::detail::overlay::method_touch
        || info.method == bg::detail::overlay::method_touch_interior);

    if (! valid)
    {
      std::cout << info.method << " -- ";
      for (int i = 0; i < 2; i++)
      {
        std::cout << info.operations[i].operation << " " << info.operations[i].seg_id.segment_index << " | ";
      }
      std::cout << std::fixed << std::setprecision(7) << " " << bg::dsv(info.point) << std::endl;

      if (abs(info.operations[0].seg_id.segment_index - info.operations[1].seg_id.segment_index) == 2)
      {
        size_t idxA = info.operations[0].seg_id.segment_index;
        size_t idxB = info.operations[1].seg_id.segment_index;
        if (idxA > idxB)
          std::swap(idxA, idxB);

        point A = geometry.outer()[idxA];
        point B = geometry.outer()[idxB];
        point & C = geometry.outer()[idxA + 1];
        C = B;
        continue;
      }

      size_t seg_idx = 0;
      switch (info.operations[0].operation)
      {
        case 1: seg_idx = 0; break;
        case 2: seg_idx = 1; break;
        default: std::cout << "Unknown case" << std::endl;
      }
      size_t idx = info.operations[seg_idx].seg_id.segment_index+1;
      point A = geometry.outer()[idx-1];
      point B = geometry.outer()[idx+1];
      point & C = geometry.outer()[idx];

      std::cout << "A: " << A.x() << " " << A.y() << std::endl;
      std::cout << "B: " << B.x() << " " << B.y() << std::endl;

      point M{0,0};
      bg::add_point(M, A);
      bg::add_point(M, B);
      bg::divide_value(M, 2.0);

      std::cout << "Mediane: " << M.x() << " " << M.y() << std::endl;

      //      C = M;

      bg::subtract_point(M, C);
      bg::multiply_value(M, 0.1);
      bg::add_point(C, M);


      std::cout << geometry.outer()[idx].x() << " " << geometry.outer()[idx].y() << std::endl;
    }
  }
}

void test1()
{
  polygon polyLeft;
  polygon polyRight;

  std::vector<point> right{point(5,8), point(7,8), point(4.9,5), point(7,2), point(5,2), point(5,8)};
  std::vector<point> left{point(5,8), point(5,2), point(3,2), point(5.1,5), point(3,8), point(5,8)};
  polyRight.outer().swap(right);
  polyLeft.outer().swap(left);


  std::cout << "Left " << std::boolalpha <<  bg::is_valid(polyLeft) << std::endl;
  multi_polygon out;
  bg::dissolve(polyLeft, out);
  polyLeft.outer().swap(out.back().outer());

//  CheckAndFix(polyLeft);
  std::cout << "Left " << std::boolalpha <<  bg::is_valid(polyLeft) << std::endl << std::endl;

  std::cout << "Right " << std::boolalpha <<  bg::is_valid(polyRight) << std::endl;
  bg::clear(out);
  bg::dissolve(polyRight, out);
  polyRight.outer().swap(out.back().outer());
//  CheckAndFix(polyRight);
  std::cout << "Right " << std::boolalpha <<  bg::is_valid(polyRight) << std::endl<< std::endl;
}

int main()
{

  test1();

  polygon polyLeft;
  polygon polyRight;

  std::vector<point> right{point(6,6), point(6,2), point(3,2), point(4.1,5), point(5,4), point(3,6), point(6,6)};
  polyRight.outer().swap(right);
//  std::vector<point> left{point(5,8), point(5,2), point(3,2), point(5.5,5), point(3,8), point(5,8)};
//  polyLeft.outer().swap(left);

  std::cout << "Right " << std::boolalpha <<  bg::is_valid(polyRight) << std::endl;
  multi_polygon out;
  bg::dissolve(polyRight, out);
  polyRight.outer().swap(out.back().outer());
//  CheckAndFix(polyRight);
  std::cout << "Right " << std::boolalpha <<  bg::is_valid(polyRight) << std::endl;
//  std::cout << "Right" << std::endl;
//  CheckAndFix(polyRight);

  return 0;
}
