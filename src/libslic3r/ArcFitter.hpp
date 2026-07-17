#ifndef slic3r_ArcFitter_hpp_
#define slic3r_ArcFitter_hpp_

#include "Circle.hpp"

namespace Slic3r {

//BBS: 直线移动(G0 和 G1)或圆弧移动(G2 和 G3)。
enum class EMovePathType : unsigned char
{
    Noop_move,
    Linear_move,
    Arc_move_cw,
    Arc_move_ccw,
    Count
};

//BBS
struct PathFittingData{
    size_t start_point_index;
    size_t end_point_index;
    EMovePathType path_type;
    // BBS: only valid when path_type is arc move
    // Used to store detail information of arc segment
    ArcSegment arc_data;

    bool is_linear_move() {
        return (path_type == EMovePathType::Linear_move);
    }
    bool is_arc_move() {
        return (path_type == EMovePathType::Arc_move_ccw || path_type == EMovePathType::Arc_move_cw);
    }
    bool reverse_arc_path() {
        if (!is_arc_move() || !arc_data.reverse())
            return false;
        path_type = (arc_data.direction == ArcDirection::Arc_Dir_CCW) ? EMovePathType::Arc_move_ccw : EMovePathType::Arc_move_cw;
        return true;
    }
};

class ArcFitter {
public:
    //BBS: 此函数用于检查点列表并返回哪些部分可以拟合为圆弧，哪些部分应为直线
    static void do_arc_fitting(const Points& points, std::vector<PathFittingData> &result, double tolerance);
    //BBS: 此函数用于检查点列表并返回哪些部分可以拟合为圆弧，哪些部分应为直线.
    //By the way, it also use DP simplify to reduce point of straight part and only keep the start and end point of arc.
    static void do_arc_fitting_and_simplify(Points& points, std::vector<PathFittingData>& result, double tolerance);
};

}


#endif
