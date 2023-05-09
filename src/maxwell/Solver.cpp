#include <fstream>
#include <iostream>
#include <algorithm>

#include "Solver.h"

using namespace mfem;

namespace maxwell {

std::unique_ptr<FiniteElementSpace> buildFiniteElementSpace(Mesh* m, FiniteElementCollection* fec)
{
#ifdef MAXWELL_USE_MPI	
	if (dynamic_cast<ParMesh*>(m) != nullptr) {
		auto pm{ dynamic_cast<ParMesh*>(m) };
		return std::make_unique<ParFiniteElementSpace>(pm, fec);
	}
#endif
	if (dynamic_cast<Mesh*>(m) != nullptr) {
		return std::make_unique<FiniteElementSpace>(m, fec);
	}
	throw std::runtime_error("Invalid mesh to build FiniteElementSpace");
}

Solver::Solver(const ProblemDescription& problem, const SolverOptions& options) :
	Solver(problem.model, problem.probes, problem.sources, options)
{}

Solver::Solver(
	const Model& model,
	const Probes& probes,
	const Sources& sources,
	const SolverOptions& options) :
	opts_{ options },
	model_{ model },
	fec_{ opts_.order, model_.getMesh().Dimension(), BasisType::GaussLobatto},
	fes_{ buildFiniteElementSpace(& model_.getMesh(), &fec_) },
	fields_{ *fes_ },
	sourcesManager_{ sources, *fes_ },
	probesManager_{ probes, *fes_, fields_, opts_ },
	time_{0.0}
{
	
	checkOptionsAreValid(opts_);

	if (opts_.dt == 0.0) {
		dt_ = getTimeStep();
	}
	else {
		dt_ = opts_.dt;
	}

	if (opts_.evolutionOperatorOptions.spectral == true) {
		performSpectralAnalysis(*fes_.get(), model_, opts_.evolutionOperatorOptions);
	}

	sourcesManager_.setInitialFields(fields_);
	maxwellEvol_ = std::make_unique<MaxwellEvolution3D>(
			*fes_, model_, sourcesManager_, opts_.evolutionOperatorOptions);
	
	maxwellEvol_->SetTime(time_);
	odeSolver_->Init(*maxwellEvol_);

	probesManager_.updateProbes(time_);


}

void Solver::checkOptionsAreValid(const SolverOptions& opts) const
{
	if ((opts.order < 0) ||
		(opts.t_final < 0)) {
		throw std::runtime_error("Incorrect parameters in Options");
	}

	if (opts.dt == 0.0) {
		if (fes_->GetMesh()->Dimension() > 1) {
			throw std::runtime_error("Automatic TS calculation not implemented yet for Dimensions higher than 1.");
		}
	}

	for (const auto& bdrMarker : model_.getBoundaryToMarker())
	{
		if (bdrMarker.first == BdrCond::SMA && opts_.evolutionOperatorOptions.fluxType == FluxType::Centered) {
			throw std::runtime_error("SMA and Centered FluxType are not compatible.");
		}
	}
}

const PointProbe& Solver::getPointProbe(const std::size_t probe) const 
{ 
	return probesManager_.getPointProbe(probe); 
}

double getMinimumInterNodeDistance(FiniteElementSpace& fes)
{
	GridFunction nodes(&fes);
	fes.GetMesh()->GetNodes(nodes);
	double res{ std::numeric_limits<double>::max() };
	for (int e = 0; e < fes.GetMesh()->ElementToElementTable().Size(); ++e) {
		Array<int> dofs;
		fes.GetElementDofs(e, dofs);
		if (dofs.Size() == 1) {
			res = std::min(res, fes.GetMesh()->GetElementSize(e));
		}
		else {
			for (int i = 0; i < dofs.Size(); ++i) {
				for (int j = i + 1; j < dofs.Size(); ++j) {
					res = std::min(res, std::abs(nodes[dofs[i]] - nodes[dofs[j]]));
				}
			}
		}
	}
	return res;
}

double Solver::getTimeStep()
{
	double signalSpeed{ 1.0 };
	double maxTimeStep{ 0.0 };
	if (opts_.order == 0) {
		maxTimeStep = getMinimumInterNodeDistance(*fes_) / signalSpeed;
	}
	else {
		maxTimeStep = getMinimumInterNodeDistance(*fes_) / pow(opts_.order, 1.5) / signalSpeed;
	}
	return opts_.CFL * maxTimeStep;
}

void Solver::run()
{
	while (time_ <= opts_.t_final - 1e-8*dt_) {
		step();
	}
}

void Solver::step()
{
	double truedt{ std::min(dt_, opts_.t_final - time_) };
	odeSolver_->Step(fields_.allDOFs(), time_, truedt);
	probesManager_.updateProbes(time_);
}


AttributeToBoundary Solver::assignAttToBdrByDimForSpectral(Mesh& submesh)
{
	switch (submesh.Dimension()) {
	case 1:
		return AttributeToBoundary{ {1, BdrCond::SMA }, {2, BdrCond::SMA} };
	case 2:
		switch (submesh.GetElementType(0)) {
		case Element::TRIANGLE:
			return AttributeToBoundary{ {1, BdrCond::SMA }, {2, BdrCond::SMA}, {3, BdrCond::SMA } };
		case Element::QUADRILATERAL:
			return AttributeToBoundary{ {1, BdrCond::SMA }, {2, BdrCond::SMA}, {3, BdrCond::SMA }, {4, BdrCond::SMA} };
		default:
			throw std::runtime_error("Incorrect element type for 2D spectral AttToBdr assignation.");
		}
	case 3:
		switch (submesh.GetElementType(0)) {
		case Element::TETRAHEDRON:
			return AttributeToBoundary{ {1, BdrCond::SMA }, {2, BdrCond::SMA}, {3, BdrCond::SMA }, {4, BdrCond::SMA} };
		case Element::HEXAHEDRON:
			return AttributeToBoundary{ {1, BdrCond::SMA }, {2, BdrCond::SMA}, {3, BdrCond::SMA }, {4, BdrCond::SMA}, {5, BdrCond::SMA }, {6, BdrCond::SMA} };
		default:
			throw std::runtime_error("Incorrect element type for 3D spectral AttToBdr assignation.");
		}
	default:
		throw std::runtime_error("Dimension is incorrect for spectral AttToBdr assignation.");
	}

}

Eigen::SparseMatrix<double> Solver::assembleSubmeshedSpectralOperatorMatrix(Mesh& submesh, const FiniteElementCollection& fec, const MaxwellEvolOptions& opts)
{
	Model submodel(submesh, AttributeToMaterial{}, assignAttToBdrByDimForSpectral(submesh), AttributeToInteriorConditions{});
	FiniteElementSpace subfes(&submesh, &fec);
	Eigen::SparseMatrix<double> local;
	auto numberOfFieldComponents = 2;
	auto numberofMaxDimensions = 3;
	local.resize(numberOfFieldComponents * numberofMaxDimensions * subfes.GetNDofs(), 
		numberOfFieldComponents * numberofMaxDimensions * subfes.GetNDofs());
	for (int x = X; x <= Z; x++) {
		int y = (x + 1) % 3;
		int z = (x + 2) % 3;

		allocateDenseInEigen(buildByMult(*buildInverseMassMatrix(H, submodel, subfes), *buildDerivativeOperator(y, subfes), subfes)->SpMat().ToDenseMatrix(), local, { H,E }, { x,z }, -1.0); // MS
		allocateDenseInEigen(buildByMult(*buildInverseMassMatrix(H, submodel, subfes), *buildDerivativeOperator(z, subfes), subfes)->SpMat().ToDenseMatrix(), local, { H,E }, { x,y });
		allocateDenseInEigen(buildByMult(*buildInverseMassMatrix(E, submodel, subfes), *buildDerivativeOperator(y, subfes), subfes)->SpMat().ToDenseMatrix(), local, { E,H }, { x,z });
		allocateDenseInEigen(buildByMult(*buildInverseMassMatrix(E, submodel, subfes), *buildDerivativeOperator(z, subfes), subfes)->SpMat().ToDenseMatrix(), local, { E,H }, { x,y }, -1.0);

		allocateDenseInEigen(buildByMult(*buildInverseMassMatrix(H, submodel, subfes), *buildOneNormalOperator(E, { y }, submodel, subfes, opts), subfes)->SpMat().ToDenseMatrix(), local, { H,E }, { x,z }); // MFN
		allocateDenseInEigen(buildByMult(*buildInverseMassMatrix(H, submodel, subfes), *buildOneNormalOperator(E, { z }, submodel, subfes, opts), subfes)->SpMat().ToDenseMatrix(), local, { H,E }, { x,y }, -1.0);
		allocateDenseInEigen(buildByMult(*buildInverseMassMatrix(E, submodel, subfes), *buildOneNormalOperator(H, { y }, submodel, subfes, opts), subfes)->SpMat().ToDenseMatrix(), local, { E,H }, { x,z }, -1.0);
		allocateDenseInEigen(buildByMult(*buildInverseMassMatrix(E, submodel, subfes), *buildOneNormalOperator(H, { z }, submodel, subfes, opts), subfes)->SpMat().ToDenseMatrix(), local, { E,H }, { x,y });

		if (opts.fluxType == FluxType::Upwind) {

			allocateDenseInEigen(buildByMult(*buildInverseMassMatrix(H, submodel, subfes), *buildZeroNormalOperator(H, submodel, subfes, opts), subfes)->SpMat().ToDenseMatrix(), local, { H,H }, { x }, -1.0); // MP
			allocateDenseInEigen(buildByMult(*buildInverseMassMatrix(E, submodel, subfes), *buildZeroNormalOperator(E, submodel, subfes, opts), subfes)->SpMat().ToDenseMatrix(), local, { E,E }, { x }, -1.0);

			allocateDenseInEigen(buildByMult(*buildInverseMassMatrix(H, submodel, subfes), *buildTwoNormalOperator(H, { X, x }, submodel, subfes, opts), subfes)->SpMat().ToDenseMatrix(), local, { H,H }, { X,x }); //MPNN
			allocateDenseInEigen(buildByMult(*buildInverseMassMatrix(H, submodel, subfes), *buildTwoNormalOperator(H, { Y, x }, submodel, subfes, opts), subfes)->SpMat().ToDenseMatrix(), local, { H,H }, { Y,x });
			allocateDenseInEigen(buildByMult(*buildInverseMassMatrix(H, submodel, subfes), *buildTwoNormalOperator(H, { Z, x }, submodel, subfes, opts), subfes)->SpMat().ToDenseMatrix(), local, { H,H }, { Z,x });
			allocateDenseInEigen(buildByMult(*buildInverseMassMatrix(E, submodel, subfes), *buildTwoNormalOperator(E, { X, x }, submodel, subfes, opts), subfes)->SpMat().ToDenseMatrix(), local, { E,E }, { X,x });
			allocateDenseInEigen(buildByMult(*buildInverseMassMatrix(E, submodel, subfes), *buildTwoNormalOperator(E, { Y, x }, submodel, subfes, opts), subfes)->SpMat().ToDenseMatrix(), local, { E,E }, { Y,x });
			allocateDenseInEigen(buildByMult(*buildInverseMassMatrix(E, submodel, subfes), *buildTwoNormalOperator(E, { Z, x }, submodel, subfes, opts), subfes)->SpMat().ToDenseMatrix(), local, { E,E }, { Z,x });

		}

	}
	return local;
}

double Solver::findMaxEigenvalueModulus(const Eigen::VectorXcd& eigvals)
{
	auto res{ 0.0 };
	for (int i = 0; i < eigvals.size(); ++i) {
		auto modulus{ sqrt(pow(eigvals[i].real(),2.0) + pow(eigvals[i].imag(),2.0)) };
		if (modulus <= 1.0 && modulus >= res) {
			res = modulus;
		}
	}
	return res;
}

void reassembleSpectralBdrForSubmesh(SubMesh* submesh) 
{
	switch (submesh->GetElementType(0)) {
	case Element::SEGMENT:
		for (int i = 0; i < submesh->GetParentVertexIDMap().Size(); ++i) {
			submesh->AddBdrPoint(i, i + 1);
		}
		submesh->FinalizeMesh();
		break;
	case Element::TRIANGLE:
		for (int i = 0; i < submesh->GetNBE(); ++i) {
			submesh->SetBdrAttribute(i, i + 1);
		}
		submesh->FinalizeMesh();
		break;
	case Element::QUADRILATERAL:
		for (int i = 0; i < submesh->GetNBE(); ++i) {
			submesh->SetBdrAttribute(i, i + 1);
		}
		submesh->FinalizeMesh();
		break;
	case Element::TETRAHEDRON:
		for (int i = 0; i < submesh->GetNBE(); ++i) {
			submesh->SetBdrAttribute(i, i + 1);
		}
		submesh->FinalizeMesh();
		break;
	case Element::HEXAHEDRON:
		for (int i = 0; i < submesh->GetNBE(); ++i) {
			submesh->SetBdrAttribute(i, i + 1);
		}
		submesh->FinalizeMesh();
		break;
	default:
		throw std::runtime_error("Incorrect element type for Bdr Spectral assignation.");
	}
}

void Solver::evaluateStabilityByEigenvalueEvolutionFunction(Eigen::VectorXcd& eigenvals, std::unique_ptr<MaxwellEvolution3D>& maxwellEvol)
{
	auto real { toMFEMVector(eigenvals.real()) };
	auto realPre = real;
	auto imag { toMFEMVector(eigenvals.imag()) };
	auto imagPre = imag;
	auto time { 0.0 };
	maxwellEvol->SetTime(time);
	odeSolver_->Init(*maxwellEvol);
	odeSolver_->Step(real, time, opts_.dt);
	time = 0.0;
	maxwellEvol->SetTime(time);
	odeSolver_->Init(*maxwellEvol);
	odeSolver_->Step(imag, time, opts_.dt);
	
	for (int i = 0; i < real.Size(); ++i) {
		
		auto modPre{ sqrt(pow(realPre[i],2.0) + pow(imagPre[i],2.0)) };
		auto mod   { sqrt(pow(real[i]   ,2.0) + pow(imag[i]   ,2.0)) };

		if (modPre != 0.0) {
			if (mod / modPre > 1.0) {
				throw std::runtime_error("The coefficient between the modulus of a time evolved eigenvalue and its original value is higher than 1.0 - RK4 instability.");
			}
		}
	}
}

void Solver::performSpectralAnalysis(const FiniteElementSpace& fes, Model& model, const MaxwellEvolOptions& opts)
{
	Array<int> domainAtts(1);
	domainAtts[0] = 501;
	auto meshCopy{ Mesh::Mesh(model.getConstMesh()) };

	for (int elem = 0; elem < meshCopy.GetNE(); ++elem) {

		auto preAtt(meshCopy.GetAttribute(elem));
		meshCopy.SetAttribute(elem, domainAtts[0]);
		auto submesh{ SubMesh::CreateFromDomain(meshCopy,domainAtts) };
		meshCopy.SetAttribute(elem, preAtt);
		submesh.SetAttribute(0, preAtt);

		reassembleSpectralBdrForSubmesh(&submesh);

		auto eigenvals{ assembleSubmeshedSpectralOperatorMatrix(submesh, *fes.FEColl(), opts).toDense().eigenvalues() };

		const FiniteElementCollection& fec = *fes.FEColl();
		evaluateStabilityByEigenvalueEvolutionFunction(
			eigenvals, 
			std::make_unique<MaxwellEvolution3D>(
				FiniteElementSpace(&submesh, &fec), 
				Model(submesh, 
					AttributeToMaterial{}, 
					assignAttToBdrByDimForSpectral(submesh), 
					AttributeToInteriorConditions{}), 
				SourcesManager(Sources(), FiniteElementSpace(&submesh, &fec)), 
				opts_.evolutionOperatorOptions));
	}
}


}
