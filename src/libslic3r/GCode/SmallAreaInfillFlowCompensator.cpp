// 与挤出线长度成反比地修改挤出线的流量。
// 当填充线变短时，流量率会自动减少以减轻
// 小填充区域过度挤出的影响。

// 基于Alexander Thor的原创作品，根据GPLv3许可：
// https://github.com/Alexander-T-Moss/Small-Area-Flow-Comp

#include <math.h>
#include <cstring>
#include <cfloat>
#include <regex>

#include "../libslic3r.h"
#include "../PrintConfig.hpp"

#include "SmallAreaInfillFlowCompensator.hpp"
#include "spline/spline.h"
#include <boost/log/trivial.hpp>

namespace Slic3r {

bool nearly_equal(double a, double b)
{
    return std::nextafter(a, std::numeric_limits<double>::lowest()) <= b && std::nextafter(a, std::numeric_limits<double>::max()) >= b;
}

SmallAreaInfillFlowCompensator::SmallAreaInfillFlowCompensator(const Slic3r::GCodeConfig& config)
{
    try {
        for (auto& line : config.small_area_infill_flow_compensation_model.values) {
            std::istringstream iss(line);
            std::string        value_str;
            double             eLength = 0.0;

            if (std::getline(iss, value_str, ',')) {
                try {
                    // 修剪前导和尾随空白
                    value_str = std::regex_replace(value_str, std::regex("^\\s+|\\s+$"), "");
                    if (value_str.empty()) {
                        continue;
                    }
                    eLength = std::stod(value_str);
                    if (std::getline(iss, value_str, ',')) {
                        eLengths.push_back(eLength);
                        flowComps.push_back(std::stod(value_str));
                    }
                } catch (...) {
                    std::stringstream ss;
                    ss << "解析小面积填充补偿模型中的数据点时出错:" << line << std::endl;

                    throw Slic3r::InvalidArgument(ss.str());
                }
            }
        }

        for (int i = 0; i < eLengths.size(); i++) {
            if (i == 0) {
                if (!nearly_equal(eLengths[i], 0.0)) {
                    throw Slic3r::InvalidArgument("小面积填充补偿模型的第一个挤出长度必须为0");
                }
            } else {
                if (nearly_equal(eLengths[i], 0.0)) {
                    throw Slic3r::InvalidArgument("只有小面积填充补偿模型的第一个挤出长度可以为0");
                }
                if (eLengths[i] <= eLengths[i - 1]) {
                    throw Slic3r::InvalidArgument("后续点的挤出长度必须递增");
                }
            }
        }

        if (!flowComps.empty() && !nearly_equal(flowComps.back(), 1.0)) {
            throw Slic3r::InvalidArgument("小面积填充流量补偿模型的最终补偿因子必须为1.0");
        }

        flowModel = std::make_unique<tk::spline>();
        flowModel->set_points(eLengths, flowComps);

    } catch (std::exception& e) {
        BOOST_LOG_TRIVIAL(error) << "解析小面积填充补偿模型时出错: " << e.what();
    }
}

SmallAreaInfillFlowCompensator::~SmallAreaInfillFlowCompensator() = default;

double SmallAreaInfillFlowCompensator::flow_comp_model(const double line_length)
{
    if(flowModel == nullptr)
        return 1.0;

    if (line_length == 0 || line_length > max_modified_length()) {
        return 1.0;
    }

    return (*flowModel)(line_length);
}

double SmallAreaInfillFlowCompensator::modify_flow(const double line_length, const double dE, const ExtrusionRole role)
{
    if (flowModel &&
        (role == ExtrusionRole::erSolidInfill || role == ExtrusionRole::erTopSolidInfill || role == ExtrusionRole::erBottomSurface)) {
        return dE * flow_comp_model(line_length);
    }

    return dE;
}

} // namespace Slic3r
