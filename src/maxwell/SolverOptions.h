#pragma once

#include "Types.h"

namespace maxwell {

struct SolverOptions {
    int order = 2;
    double dt = 0.0;
    double t_final = 2.0;
    double CFL = 0.8;
    //decltype(BasisType::GaussLobatto) basis = BasisType::GaussLobatto;
    MaxwellEvolOptions evolutionOperatorOptions;
    
    SolverOptions& setTimeStep(double t) {
        dt = t;
        return *this;
    };
    SolverOptions& setFinalTime(double t) { 
        t_final = t; 
        return *this; 
    };
    SolverOptions& setCentered() {
        evolutionOperatorOptions.fluxType = FluxType::Centered;
        return *this;
    };
    SolverOptions& setCFL(double cfl) {
        CFL = cfl;
        return *this;
    }
    SolverOptions& setOrder(int or) {
        order = or;
        return *this;
    }
    SolverOptions& setSpectralEO(bool eigenvals = false) {
        evolutionOperatorOptions.spectral = true;
        evolutionOperatorOptions.eigenvals = eigenvals;
        return *this;
    }
    //SolverOptions& setBasis(BasisType bst) {
    //    basis = bst;
    //    return *this;
    //}
};

}