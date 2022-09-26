#pragma once

#include "mfemExtension/BilinearIntegrators.h"

#include "Types.h"
#include "Model.h"
#include "Sources.h"
#include "MaxwellDefs.h"

namespace maxwell {

class MaxwellEvolution3D: public mfem::TimeDependentOperator {
public:
	static const int numberOfFieldComponents = 2;
	static const int numberOfMaxDimensions = 3;

	MaxwellEvolution3D(FiniteElementSpace&, Model&, MaxwellEvolOptions&);
	virtual void Mult(const Vector& x, Vector& y) const;

private:

	std::array<std::array<FiniteElementOperator, 3>, 2> MS_;
	std::array<std::array<FiniteElementOperator, 3>, 2> MF_;
	std::array<std::array<FiniteElementOperator, 3>, 2> MP_;

	FiniteElementSpace& fes_;
	Model& model_;
	MaxwellEvolOptions opts_;
	

};

}