#pragma once

#include <mfem.hpp>

#include "Types.h"

namespace maxwell {

struct ExporterProbe {
    std::string name{"MaxwellView"};
    int visSteps{ 10 };
};

struct NearFieldProbe {
    std::string name{ "NearFieldProbe" };
    std::string exportPath{ "./defaultExportPath/" + name + "/"};
    int expSteps{ 10 };
    std::vector<int> tags;
};

class FieldProbe {
public:
    FieldProbe(const FieldType& ft, const Direction& d, const Point& p) :
        fieldToExtract_{ ft },
        directionToExtract_{ d },
        point_{ p }
    {}

    const FieldType& getFieldType() const { return fieldToExtract_; }
    const Direction& getDirection() const { return directionToExtract_; }
    const FieldMovie& getFieldMovie() const { return fieldMovie_; }
    const Point& getPoint() const { return point_; }
    void addFieldToMovies(double time, const double& field) { fieldMovie_.emplace(time, field); };

    std::pair<Time, double> findFrameWithMax() const
    {
        std::pair<Time, double> res{ 0.0, -std::numeric_limits<double>::infinity() };
        for (const auto& [t, f] : fieldMovie_) {
            if (res.second < f) {
                res = { t,f };
            }
        }
        return res;
    }

    std::pair<Time, double> findFrameWithMin() const
    {
        std::pair<Time, double> res{ 0.0, std::numeric_limits<double>::infinity() };
        for (const auto& [t, f] : fieldMovie_) {
            if (res.second > f) {
                res = { t, f };
            }
        }
        return res;
    }
    
private:
    FieldType fieldToExtract_;
    Direction directionToExtract_;
    Point point_;

    FieldMovie fieldMovie_;
};

class PointProbe {
public:
    PointProbe(const Point& p) :
        point_{ p }
    {}

    const FieldMovies& getFieldMovies() const { return fieldMovies_; }
    const Point& getPoint() const { return point_; }
    void addFieldsToMovies(Time t, const FieldsForMovie& fields) { fieldMovies_.emplace(t, fields); };


private:

    Point point_;

    FieldMovies fieldMovies_;
};

struct Probes {
    std::vector<FieldProbe> fieldProbes;
    std::vector<ExporterProbe> exporterProbes;
    std::vector<PointProbe> pointProbes;
    std::vector<NearFieldProbe> nearFieldProbes;
};

}