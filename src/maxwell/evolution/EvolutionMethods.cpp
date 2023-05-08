#include "EvolutionMethods.h"

namespace maxwell {

using namespace mfem;

using namespace mfemExtension;

InteriorCoefficients intCoeff{
	{FluxType::Centered, { 1.0, 0.0 }},
	{FluxType::Upwind  , { 1.0, 1.0 }}
};

FluxBdrCoefficientsCentered bdrCentCoeff{
	{BdrCond::PEC               ,	{2.0, 0.0}},
	{BdrCond::PMC               ,	{0.0, 2.0}},
	{BdrCond::SMA               ,	{0.0, 0.0}},
	{BdrCond::NONE              ,   {0.0, 0.0}},
};

FluxBdrCoefficientsUpwind bdrUpwindCoeff{
	{BdrCond::PEC               ,	{2.0, 0.0}},
	{BdrCond::PMC               ,	{0.0, 2.0}},
	{BdrCond::SMA               ,	{1.0, 1.0}},
	{BdrCond::NONE              ,   {0.0, 0.0}},
};

FluxSrcCoefficientsCentered srcCentCoeff{
	{BdrCond::TotalFieldInBacked,	{ 1.0, 1.0}},
	{BdrCond::TotalFieldIn      ,	{ 1.0, 1.0}},
	{BdrCond::TotalFieldOut     ,   {-1.0,-1.0}},
};

FluxSrcCoefficientsUpwind srcUpwindCoeff{
	{BdrCond::TotalFieldInBacked,	{ 1.0, 1.0}},
	{BdrCond::TotalFieldIn      ,	{ 1.0, 1.0}},
	{BdrCond::TotalFieldOut     ,   {-1.0,-1.0}},
};

std::map<BdrCond, std::vector<double>> bdrCoeffCheck(const FluxType& ft)
{
	std::map<BdrCond, std::vector<double>> res;
	switch (ft) {
	case(FluxType::Centered):
		res = bdrCentCoeff;
		break;
	case(FluxType::Upwind):
		res = bdrUpwindCoeff;
		break;
	}
	return res;
}

std::map<BdrCond, std::vector<double>> srcCoeffCheck(const FluxType& ft)
{
	std::map<BdrCond, std::vector<double>> res;
	switch (ft) {
	case(FluxType::Centered):
		res = srcCentCoeff;
		break;
	case(FluxType::Upwind):
		res = srcUpwindCoeff;
		break;
	}
	return res;
}

Eigen::MatrixXd toEigen(const DenseMatrix& mat)
{
	Eigen::MatrixXd res(mat.Width(), mat.Height());
	for (int i = 0; i < mat.Width(); i++) {
		for (int j = 0; j < mat.Height(); j++) {
			res(i, j) = mat.Elem(i, j);
		}
	}
	return res;
}

FiniteElementOperator buildByMult(
	const BilinearForm& op1,
	const BilinearForm& op2,
	FiniteElementSpace& fes)
{
	auto aux = mfem::Mult(op1.SpMat(), op2.SpMat());
	auto res = std::make_unique<BilinearForm>(&fes);
	res->Assemble();
	res->Finalize();
	res->SpMat().Swap(*aux);

	return res;
}

FiniteElementIBFIOperator buildIBFIByMult(
	const BilinearForm& op1,
	const BilinearFormIBFI& op2,
	FiniteElementSpace& fes)
{
	auto aux = mfem::Mult(op1.SpMat(), op2.SpMat());
	auto res = std::make_unique<BilinearFormIBFI>(&fes);
	res->Assemble();
	res->Finalize();
	res->SpMat().Swap(*aux);

	return res;
}

Vector buildNVector(const Direction& d, const FiniteElementSpace& fes)
{
	const auto dim{ fes.GetMesh()->Dimension() };
	Vector r(dim);
	r = 0.0;
	if (d < dim) {
		r[d] = 1.0;
		return r;
	}
	else {
		return r;
	}
}

FiniteElementOperator buildInverseMassMatrix(const FieldType& f, const Model& model, FiniteElementSpace& fes)
{
	Vector aux{ model.buildPiecewiseArgVector(f) };
	PWConstCoefficient PWCoeff(aux);

	auto MInv = std::make_unique<BilinearForm>(&fes);
	MInv->AddDomainIntegrator(new InverseIntegrator(new MassIntegrator(PWCoeff)));

	MInv->Assemble();
	MInv->Finalize();
	return MInv;
}



FiniteElementOperator buildDerivativeOperator(const Direction& d, FiniteElementSpace& fes)
{
	auto res = std::make_unique<BilinearForm>(&fes);

	if (d >= fes.GetMesh()->Dimension()) {
		res->Assemble();
		res->Finalize();
		return res;
	}

	ConstantCoefficient coeff = (d <= fes.GetMesh()->Dimension()) ? ConstantCoefficient(1.0) : ConstantCoefficient(0.0);
	res->AddDomainIntegrator(
		new DerivativeIntegrator(coeff, d)
	);

	res->Assemble();
	res->Finalize();
	return res;
}



FiniteElementOperator buildFluxOperator(const FieldType& f, const std::vector<Direction>& dirTerms, Model& model, FiniteElementSpace& fes, const MaxwellEvolOptions& opts)
{
	auto res = std::make_unique<BilinearForm>(&fes);

	res->AddInteriorFaceIntegrator(
		new MaxwellDGTraceJumpIntegrator(dirTerms, intCoeff[opts.fluxType].at(int(FluxType::Centered))));

	for (auto& kv : model.getBoundaryToMarker()) {

		auto c = bdrCoeffCheck(opts.fluxType);
		if (kv.first != BdrCond::SMA) {
			res->AddBdrFaceIntegrator(
				new MaxwellDGTraceJumpIntegrator(dirTerms, c[kv.first].at(f)), kv.second);
		}
		else {
			res->AddBdrFaceIntegrator(
				new MaxwellSMAJumpIntegrator(dirTerms, c[kv.first].at(f)), kv.second);
		}
	}

	res->Assemble();
	res->Finalize();
	return res;
}

FiniteElementOperator buildPenaltyOperator(const FieldType& f, const std::vector<Direction>& dirTerms, Model& model, FiniteElementSpace& fes, const MaxwellEvolOptions& opts)
{
	auto res = std::make_unique<BilinearForm>(&fes);

	res->AddInteriorFaceIntegrator(
		new MaxwellDGTraceJumpIntegrator(dirTerms, intCoeff[opts.fluxType].at(int(FluxType::Upwind))));

	for (auto& kv : model.getBoundaryToMarker()) {
		auto c = bdrCoeffCheck(opts.fluxType);
		if (kv.first != BdrCond::SMA) {
			res->AddBdrFaceIntegrator(
				new MaxwellDGTraceJumpIntegrator(dirTerms, c[kv.first].at(f)), kv.second);
		}
		else {
			res->AddBdrFaceIntegrator(
				new MaxwellSMAJumpIntegrator(dirTerms, c[kv.first].at(f)), kv.second);
		}
	}

	res->Assemble();
	res->Finalize();
	return res;
}

FiniteElementOperator buildZeroNormalOperator(const FieldType& f, Model& model, FiniteElementSpace& fes, const MaxwellEvolOptions& opts)
{
	auto res = std::make_unique<BilinearForm>(&fes);
	res->AddInteriorFaceIntegrator(
		new MaxwellDGZeroNormalJumpIntegrator(intCoeff[opts.fluxType].at(int(opts.fluxType))));

	for (auto& kv : model.getBoundaryToMarker()) {
		auto c = bdrCoeffCheck(opts.fluxType);
		res->AddBdrFaceIntegrator(
			new MaxwellDGZeroNormalJumpIntegrator(c[kv.first].at(f)), kv.second);
	}

	res->Assemble();
	res->Finalize();
	return res;
}

FiniteElementOperator buildOneNormalOperator(const FieldType& f, const std::vector<Direction>& dirTerms, Model& model, FiniteElementSpace& fes, const MaxwellEvolOptions& opts)
{
	auto res = std::make_unique<BilinearForm>(&fes);
	res->AddInteriorFaceIntegrator(
		new MaxwellDGOneNormalJumpIntegrator(dirTerms, intCoeff[opts.fluxType].at(int(opts.fluxType))));

	for (auto& kv : model.getBoundaryToMarker()) {
		auto c = bdrCoeffCheck(opts.fluxType);
		res->AddBdrFaceIntegrator(
			new MaxwellDGOneNormalJumpIntegrator(dirTerms, c[kv.first].at(f)), kv.second);
	}

	res->Assemble();
	res->Finalize();
	return res;
}

FiniteElementOperator buildTwoNormalOperator(const FieldType& f, const std::vector<Direction>& dirTerms, Model& model, FiniteElementSpace& fes, const MaxwellEvolOptions& opts)
{
	auto res = std::make_unique<BilinearForm>(&fes);
	res->AddInteriorFaceIntegrator(
		new MaxwellDGTwoNormalJumpIntegrator(dirTerms, intCoeff[opts.fluxType].at(int(opts.fluxType))));

	for (auto& kv : model.getBoundaryToMarker()) {
		auto c = bdrCoeffCheck(opts.fluxType);
		res->AddBdrFaceIntegrator(
			new MaxwellDGTwoNormalJumpIntegrator(dirTerms, c[kv.first].at(f)), kv.second);
	}

	res->Assemble();
	res->Finalize();
	return res;
}

FiniteElementOperator buildPenaltyFixOperator(const FieldType& f, const std::vector<Direction>& dirTerms, Model& model, FiniteElementSpace& fes, const MaxwellEvolOptions& opts)
{
	auto res = std::make_unique<BilinearForm>(&fes);

	if (model.getInteriorSourceToMarker().size() != 0 && opts.fluxType == FluxType::Upwind) {

		res->AddInteriorFaceIntegrator(
			new MaxwellDGTraceJumpIntegrator(dirTerms, -intCoeff[opts.fluxType].at(int(FluxType::Upwind))));

		for (auto& kv : model.getBoundaryToMarker()) {
			auto c = bdrCoeffCheck(opts.fluxType);
			if (kv.first != BdrCond::SMA) {
				res->AddBdrFaceIntegrator(
					new MaxwellDGTraceJumpIntegrator(dirTerms, -c[kv.first].at(f)), kv.second);
			}
			else {
				res->AddBdrFaceIntegrator(
					new MaxwellSMAJumpIntegrator(dirTerms, -c[kv.first].at(f)), kv.second);
			}
		}
	}

	res->Assemble();
	res->Finalize();
	return res;
}

FiniteElementIBFIOperator buildFluxIBFIOperator(const FieldType& f, const std::vector<Direction>& dirTerms, Model& model, FiniteElementSpace& fes, const MaxwellEvolOptions& opts)
{
	auto res = std::make_unique<BilinearFormIBFI>(&fes);

	for (auto& kv : model.getInteriorBoundaryToMarker()) {
		auto c = bdrCoeffCheck(opts.fluxType);
		switch (kv.first) {
		case (BdrCond::SMA):
			res->AddInteriorBoundaryFaceIntegrator(
				new MaxwellSMAJumpIntegrator(dirTerms, c[kv.first].at(f)), kv.second);
			break;
		default:
			res->AddInteriorBoundaryFaceIntegrator(
				new MaxwellDGInteriorJumpIntegrator(dirTerms, c[kv.first].at(f)), kv.second);
			break;
		}
	}

	res->Assemble();
	res->Finalize();
	return res;
}

FiniteElementIBFIOperator buildPenaltyIBFIOperator(const FieldType& f, const std::vector<Direction>& dirTerms, Model& model, FiniteElementSpace& fes, const MaxwellEvolOptions& opts)
{
	auto res = std::make_unique<BilinearFormIBFI>(&fes);

	for (auto& kv : model.getInteriorBoundaryToMarker()) {
		auto c = bdrCoeffCheck(opts.fluxType);
		switch (kv.first) {
		case (BdrCond::SMA):
			res->AddInteriorBoundaryFaceIntegrator(
				new MaxwellSMAJumpIntegrator(dirTerms, c[kv.first].at(f)), kv.second);
			break;
		default:
			res->AddInteriorBoundaryFaceIntegrator(
				new MaxwellDGInteriorJumpIntegrator(dirTerms, c[kv.first].at(f)), kv.second);
			break;

		}
	}
	res->Assemble();
	res->Finalize();
	return res;
}

FiniteElementIBFIOperator buildFluxFunctionOperator(const FieldType& f, const std::vector<Direction>& dirTerms, Model& model, FiniteElementSpace& fes, const MaxwellEvolOptions& opts)
{
	auto res = std::make_unique<BilinearFormIBFI>(&fes);

	for (auto& kv : model.getInteriorSourceToMarker())
	{
		auto c = srcCoeffCheck(opts.fluxType);
		res->AddInteriorBoundaryFaceIntegrator(
			new MaxwellDGFluxTotalFieldIntegrator({ X }, c[kv.first].at(f), 0.5), kv.second
		);
	}

	res->Assemble();
	res->Finalize();
	return res;
}

FieldType altField(const FieldType& f)
{
	switch (f) {
	case E:
		return H;
	case H:
		return E;
	}
	throw std::runtime_error("Invalid field type for altField.");

}

void allocateDenseInEigen(const std::array<FiniteElementOperator, 2>& arr, Eigen::MatrixXd& res, const double sign, bool altField)
{
	int offset = arr[E]->SpMat().ToDenseMatrix()->Height();
	for (int i = 0; i < arr[E]->Height(); ++i) {
		for (int j = 0; j < arr[E]->Width(); ++j) {
			switch (altField) {
			case false:
				res(i, j) += sign * arr[E]->SpMat().ToDenseMatrix()->Elem(i, j);
				res(i + offset, j + offset) += sign * arr[H]->SpMat().ToDenseMatrix()->Elem(i, j);
				break;
			case true:
				res(i, j + offset) += sign * arr[E]->SpMat().ToDenseMatrix()->Elem(i, j);
				res(i + offset, j) += sign * arr[H]->SpMat().ToDenseMatrix()->Elem(i, j);
				break;
			}
		}
	}
}

void calculateEigenvalues(const Eigen::MatrixXd& mat, Eigen::VectorXcd& res)
{
	res = mat.eigenvalues();
}

void calculateEigenvalues(SparseMatrix& mat, Vector& res)
{
	auto denseMat{ mat.ToDenseMatrix() };
	denseMat->Finalize(1);
	denseMat->Eigenvalues(res);
}

void exportSparseToMarketFile(const Eigen::MatrixXd& mat)
{
	Eigen::SparseMatrix<double> sparse = mat.sparseView();
	Eigen::saveMarket(sparse, "SparseMatrix.mtx");
}

Eigen::VectorXd toEigenVector(const Vector& in)
{
	Eigen::VectorXd res;
	res.resize(in.Size());
	for (int i = 0; i < in.Size(); ++i) {
		res(i) = in.Elem(i);
	}
	return res;
}

Eigen::VectorXcd toComplexEigenVector(const Vector& in)
{
	Eigen::VectorXcd res;
	res.resize(in.Size());
	for (int i = 0; i < in.Size(); ++i) {
		res(i) = in.Elem(i);
	}
	return res;
}

Vector toMFEMVector(const Eigen::VectorXd& in)
{
	Vector res(int(in.size()));
	for (int i = 0; i < res.Size(); ++i) {
		res(i) = in[i];
	}
	return res;
}

std::vector<int> calcOffsetCoeff1D(const std::vector<FieldType>& f)
{
	std::vector<int> res(2);
	if (f[0] == f[1]) {
		if (f[0] == E) {
			res[0] = 0;
			res[1] = 0;
		}
		else {
			res[0] = 1;
			res[1] = 1;
		}
	}
	else if (f[0] != f[1]) {
		if (f[0] == E) {
			res[0] = 0;
			res[1] = 1;
		}
		else {
			res[0] = 1;
			res[1] = 0;
		}
	}
	else {
		throw std::runtime_error("Wrong input in method, check direction or field type vectors.");
	}
	return res;
}

std::vector<int> calcOffsetCoeff(const std::vector<FieldType>& f, const std::vector<Direction>& d)
{
	std::vector<int> res(2);
	if (d.size() == 1) {
		if (f[0] == E) {
			res[0] = d[0];
			res[1] = d[0];
		}
		else {
			res[0] = 3 + d[0];
			res[1] = 3 + d[0];
		}
	}
	else if (f[0] == f[1]) {
		if (f[0] == E) {
			res[0] = d[0];
			res[1] = d[1];
		}
		else {
			res[0] = 3 + d[0];
			res[1] = 3 + d[1];
		}
	}
	else if (f[0] != f[1]) {
		if (f[0] == E) {
			res[0] = d[0];
			res[1] = 3 + d[1];
		}
		else {
			res[0] = 3 + d[0];
			res[1] = d[1];
		}
	}
	else {
		throw std::runtime_error("Wrong input in method, check direction or field type vectors.");
	}
	return res;
}

void allocateDenseInEigen1D(DenseMatrix* bilMat, Eigen::SparseMatrix<double>& res, const std::vector<FieldType> f, const double sign)
{
	auto offset = bilMat->Height();
	auto offsetCoeff{ calcOffsetCoeff1D(f) };

	for (int i = 0; i < bilMat->Height(); ++i) {
		for (int j = 0; j < bilMat->Width(); ++j) {
			if (bilMat->Elem(i, j) != 0.0) {
				res.coeffRef(i + offset * offsetCoeff[0], j + offset * offsetCoeff[1]) += sign * bilMat->Elem(i, j);
			}
		}
	}
}

void allocateDenseInEigen(DenseMatrix* bilMat, Eigen::SparseMatrix<double>& res, const std::vector<FieldType> f, const std::vector<Direction> d, const double sign)
{
	auto offset = bilMat->Height();
	auto offsetCoeff{ calcOffsetCoeff(f,d) };

	for (int i = 0; i < bilMat->Height(); ++i) {
		for (int j = 0; j < bilMat->Width(); ++j) {
			if (bilMat->Elem(i, j) != 0.0) {
				res.coeffRef(i + offset * offsetCoeff[0], j + offset * offsetCoeff[1]) += sign * bilMat->Elem(i, j);
			}
		}
	}
}

SparseMatrix toMFEMSparse(const Eigen::SparseMatrix<double>& sp)
{
	SparseMatrix res(int(sp.rows()), int(sp.cols()));
	for (int k = 0; k < sp.outerSize(); ++k)
		for (Eigen::SparseMatrix<double>::InnerIterator it(sp, k); it; ++it)
		{
			res.Set(int(it.row()), int(it.col()), it.value());
		}
	res.Finalize();
	return res;
}

double usePowerMethod(const Eigen::SparseMatrix<double>& global, int iterations)
{
	auto spMat{ toMFEMSparse(global) };
	Vector itVec(int(global.cols()));
	auto mfemSparse{ toMFEMSparse(global) };
	PowerMethod pwrMtd;
	return pwrMtd.EstimateLargestEigenvalue(mfemSparse, itVec, iterations);
}

}