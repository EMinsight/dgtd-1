#pragma once

#include "mfemExtension/BilinearIntegrators.h"

#include "Types.h"
#include "Model.h"
#include "Sources.h"
#include "SourcesManager.h"
#include "MaxwellEvolutionMethods.h"

namespace maxwell {

class MaxwellEvolution2D_Spectral : public mfem::TimeDependentOperator {
public:
	static const int numberOfFieldComponents = 2;
	static const int numberOfMaxDimensions = 3;

	MaxwellEvolution2D_Spectral(mfem::FiniteElementSpace&, Model&, SourcesManager&, MaxwellEvolOptions&);
	virtual void Mult(const mfem::Vector& x, mfem::Vector& y) const;

private:

	Eigen::SparseMatrix<double> global_;
	Eigen::SparseMatrix<double> forcing_;
	Eigen::VectorXcd eigenvals_;

	mfem::FiniteElementSpace& fes_;
	Model& model_;
	SourcesManager& srcmngr_;
	MaxwellEvolOptions& opts_;

};

}