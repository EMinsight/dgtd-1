#include "Evolution.h"

namespace maxwell {

using namespace mfem;
using namespace mfemExtension;

void evalConductivity(const Vector& cond, const Vector& in, Vector& out)
{
	for (auto v{ 0 }; v < cond.Size(); v++) {
		out[v] -= cond[v] * in[v];
	}
}

void changeSignOfFieldGridFuncs(FieldGridFuncs& gfs)
{
	for (auto f : { E, H }) {
		for (auto d{ X }; d <= Z; d++) {
			gfs[f][d] *= -1.0;
		}
	}
}

const FieldGridFuncs evalTimeVarFunction(const Time time, SourcesManager& sm)
{
	auto res{ sm.evalTimeVarField(time, sm.getGlobalTFSFSpace()) };
	auto func_g_sf = res;
	sm.markDoFSforTForSF(res, true);
	{
		if (sm.getTFSFSubMesher().getSFSubMesh() != NULL) {
			sm.markDoFSforTForSF(func_g_sf, false);
			for (int f : {E, H}) {
				for (int x{ 0 }; x <= Z; x++) {
					res[f][x] -= func_g_sf[f][x];
					res[f][x] *= 0.5;
				}
			}
		}
	}
	return res;
}

Evolution::Evolution(
	FiniteElementSpace& fes, Model& model, SourcesManager& srcmngr, EvolutionOptions& options) :
	TimeDependentOperator(numberOfFieldComponents * numberOfMaxDimensions * fes.GetNDofs()),
	fes_{ fes },
	model_{ model },
	srcmngr_{ srcmngr },
	opts_{ options }
{
#ifdef SHOW_TIMER_INFORMATION
	auto startTime{ std::chrono::high_resolution_clock::now() };
#endif


#ifdef SHOW_TIMER_INFORMATION
	std::cout << "------------------------------------------------" << std::endl;
	std::cout << "---------OPERATOR ASSEMBLY INFORMATION----------" << std::endl;
	std::cout << "------------------------------------------------" << std::endl;
	std::cout << std::endl;
#endif

	if (model_.getTotalFieldScatteredFieldToMarker().find(BdrCond::TotalFieldIn) != model.getTotalFieldScatteredFieldToMarker().end()) {
		srcmngr_.initTFSFPreReqs(model_.getConstMesh(), model.getTotalFieldScatteredFieldToMarker().at(BdrCond::TotalFieldIn));
		auto globalTFSFfes{ srcmngr_.getGlobalTFSFSpace() };
		Model modelGlobal = Model(*globalTFSFfes->GetMesh(), GeomTagToMaterialInfo(), GeomTagToBoundaryInfo(GeomTagToBoundary{}, GeomTagToInteriorBoundary{}));

#ifdef SHOW_TIMER_INFORMATION
		std::cout << "Assembling TFSF Inverse Mass Operators" << std::endl;
#endif

		for (auto f : { E, H }) {
			MInvTFSF_[f] = buildInverseMassMatrix(f, modelGlobal, *globalTFSFfes);
		}

#ifdef SHOW_TIMER_INFORMATION
		std::cout << "Elapsed time (ms): " + std::to_string(std::chrono::duration_cast<std::chrono:: >
			(std::chrono::high_resolution_clock::now() - startTime).count()) << std::endl;
		std::cout << "Assembling TFSF Inverse Mass Zero-Normal Operators" << std::endl;
#endif

		for (auto f : { E, H }) {
			MP_GTFSF_[f] = buildByMult(*MInvTFSF_[f], *buildZeroNormalOperator(f, modelGlobal, *globalTFSFfes, opts_), *globalTFSFfes);
		}

#ifdef SHOW_TIMER_INFORMATION
		std::cout << "Elapsed time (ms): " + std::to_string(std::chrono::duration_cast<std::chrono::milliseconds>
			(std::chrono::high_resolution_clock::now() - startTime).count()) << std::endl;
		std::cout << "Assembling TFSF Inverse Mass One-Normal Operators" << std::endl;
#endif

		for (auto f : { E, H }) {
			for (auto d{ X }; d <= Z; d++) {
				for (auto f2 : { E, H }) {
					MFN_GTFSF_[f][f2][d] = buildByMult(*MInvTFSF_[f], *buildOneNormalOperator(f2, {d}, modelGlobal, *globalTFSFfes, opts_), *globalTFSFfes);
				}
			}
		}

#ifdef SHOW_TIMER_INFORMATION
		std::cout << "Elapsed time (ms): " + std::to_string(std::chrono::duration_cast<std::chrono::milliseconds>
			(std::chrono::high_resolution_clock::now() - startTime).count()) << std::endl;
		std::cout << "Assembling TFSF Inverse Mass Two-Normal Operators" << std::endl;
#endif

		for (auto f : { E, H }) {
			for (auto d{ X }; d <= Z; d++) {
				for (auto f2 : { E, H }) {
					for (auto d2{ X }; d2 <= Z; d2++) {
						MFNN_GTFSF_[f][f2][d][d2] = buildByMult(*MInvTFSF_[f], *buildTwoNormalOperator(f2, {d, d2}, modelGlobal, *globalTFSFfes, opts_), *globalTFSFfes);
					}
				}
			}
		}
	}

#ifdef SHOW_TIMER_INFORMATION
	std::cout << "Elapsed time (ms): " + std::to_string(std::chrono::duration_cast<std::chrono::milliseconds>
		(std::chrono::high_resolution_clock::now() - startTime).count()) << std::endl;
	std::cout << "Assembling Standard Inverse Mass Operators" << std::endl;
#endif

	for (auto f : { E, H }) {
		MInv_[f] = buildInverseMassMatrix(f, model_, fes_);
	}

	if (model_.getInteriorBoundaryToMarker().size() != 0) { //IntBdrConds

#ifdef SHOW_TIMER_INFORMATION
		std::cout << "Elapsed time (ms): " + std::to_string(std::chrono::duration_cast<std::chrono::milliseconds>
			(std::chrono::high_resolution_clock::now() - startTime).count()) << std::endl;
		std::cout << "Assembling IBFI Inverse Mass Zero-Normal Operators" << std::endl;
#endif

		for (auto f : { E, H }) {
			MPB_[f] = buildByMult(*MInv_[f], *buildZeroNormalIBFIOperator(f, model_, fes_, opts_), fes_);
		}

#ifdef SHOW_TIMER_INFORMATION
		std::cout << "Elapsed time (ms): " + std::to_string(std::chrono::duration_cast<std::chrono::milliseconds>
			(std::chrono::high_resolution_clock::now() - startTime).count()) << std::endl;
		std::cout << "Assembling IBFI Inverse Mass One-Normal Operators" << std::endl;
#endif

		for (auto f : { E, H }) {
			for (auto d{ X }; d <= Z; d++) {
				for (auto f2 : { E, H }) {
					MFNB_[f][f2][d] = buildByMult(*MInv_[f], *buildOneNormalIBFIOperator(f2, { d }, model_, fes_, opts_), fes_);
				} 
			}
		}

#ifdef SHOW_TIMER_INFORMATION
		std::cout << "Elapsed time (ms): " + std::to_string(std::chrono::duration_cast<std::chrono::milliseconds>
			(std::chrono::high_resolution_clock::now() - startTime).count()) << std::endl;
		std::cout << "Assembling IBFI Inverse Mass Two-Normal Operators" << std::endl;
#endif

		for (auto f : { E, H }) {
			for (auto d{ X }; d <= Z; d++) {
				for (auto f2 : { E, H }) {
					for (auto d2{ X }; d2 <= Z; d2++) {
						MFNNB_[f][f2][d][d2] = buildByMult(*MInv_[f], *buildTwoNormalIBFIOperator(f2, { d, d2 }, model_, fes_, opts_), fes_);
					}
				}
			}
		}
	}

#ifdef SHOW_TIMER_INFORMATION
	std::cout << "Elapsed time (ms): " + std::to_string(std::chrono::duration_cast<std::chrono::milliseconds>
		(std::chrono::high_resolution_clock::now() - startTime).count()) << std::endl;
	std::cout << "Assembling Standard Inverse Mass Stiffness Operators" << std::endl;
#endif

	for (auto f : { E, H }) {
		for (auto d{ X }; d <= Z; d++) {
			MS_[f][d] = buildByMult(*MInv_[f], *buildDerivativeOperator(d, fes_), fes_);
		}
	}

#ifdef SHOW_TIMER_INFORMATION
	std::cout << "Elapsed time (ms): " + std::to_string(std::chrono::duration_cast<std::chrono::milliseconds>
		(std::chrono::high_resolution_clock::now() - startTime).count()) << std::endl;
	std::cout << "Assembling Standard Inverse Mass Zero-Normal Operators" << std::endl;
#endif

	for (auto f : { E, H }) {
		MP_[f] = buildByMult(*MInv_[f], *buildZeroNormalOperator(f, model_, fes_, opts_), fes_);
	}

#ifdef SHOW_TIMER_INFORMATION
	std::cout << "Elapsed time (ms): " + std::to_string(std::chrono::duration_cast<std::chrono::milliseconds>
		(std::chrono::high_resolution_clock::now() - startTime).count()) << std::endl;
	std::cout << "Assembling Standard Inverse Mass One-Normal Operators" << std::endl;
#endif

	for (auto f : { E, H }) {
		for (auto d{ X }; d <= Z; d++) {
			for (auto f2 : { E, H }) {
				MFN_[f][f2][d] = buildByMult(*MInv_[f], *buildOneNormalOperator(f2, { d }, model_, fes_, opts_), fes_);
			}
		}
	}

#ifdef SHOW_TIMER_INFORMATION
	std::cout << "Elapsed time (ms): " + std::to_string(std::chrono::duration_cast<std::chrono::milliseconds>
		(std::chrono::high_resolution_clock::now() - startTime).count()) << std::endl;
	std::cout << "Assembling Standard Inverse Mass Two-Normal Operators" << std::endl;
#endif

	for (auto f : { E, H }) {
		for (auto d{ X }; d <= Z; d++) {
			for (auto f2 : { E, H }) {
				for (auto d2{ X }; d2 <= Z; d2++) {
					MFNN_[f][f2][d][d2] = buildByMult(*MInv_[f], *buildTwoNormalOperator(f2, { d, d2 }, model_, fes_, opts_), fes_);
				}
			}
		}
	}

#ifdef SHOW_TIMER_INFORMATION
	std::cout << "Elapsed time (ms): " + std::to_string(std::chrono::duration_cast<std::chrono::milliseconds>
		(std::chrono::high_resolution_clock::now() - startTime).count()) << std::endl;
	std::cout << "Operator assembly finished" << std::endl;
	std::cout << std::endl;
#endif

	CND_ = buildConductivityCoefficients(model_, fes_);
 }

void Evolution::Mult(const Vector& in, Vector& out) const
{
	const auto& dim{ fes_.GetMesh()->Dimension() };

	std::array<Vector, 3> eOld, hOld;
	std::array<GridFunction, 3> eNew, hNew;
	for (int d = X; d <= Z; d++) {
		eOld[d].SetDataAndSize(in.GetData() + d * fes_.GetNDofs(), fes_.GetNDofs());
		hOld[d].SetDataAndSize(in.GetData() + (d + 3) * fes_.GetNDofs(), fes_.GetNDofs());
		eNew[d].SetSpace(&fes_);
		hNew[d].SetSpace(&fes_);
		eNew[d].MakeRef(&fes_, &out[d * fes_.GetNDofs()]);
		hNew[d].MakeRef(&fes_, &out[(d + 3) * fes_.GetNDofs()]);
		eNew[d] = 0.0;
		hNew[d] = 0.0;
	}

	for (int x = X; x <= Z; x++) {
		int y = (x + 1) % 3;
		int z = (x + 2) % 3;
		
		evalConductivity(CND_, eOld[x], eNew[x]);

		//Centered
		MS_[H][y]		 ->AddMult(eOld[z], hNew[x],-1.0);
		MS_[H][z]		 ->AddMult(eOld[y], hNew[x]);
		MS_[E][y]		 ->AddMult(hOld[z], eNew[x]);
		MS_[E][z]		 ->AddMult(hOld[y], eNew[x],-1.0);
		
		MFN_[H][E][y]  	 ->AddMult(eOld[z], hNew[x], 1.0);
		MFN_[H][E][z]	 ->AddMult(eOld[y], hNew[x],-1.0);
		MFN_[E][H][y]  	 ->AddMult(hOld[z], eNew[x],-1.0);
		MFN_[E][H][z]	 ->AddMult(hOld[y], eNew[x], 1.0);

		if (opts_.fluxType == FluxType::Upwind) {

			MFNN_[H][H][X][x]->AddMult(hOld[X], hNew[x], 1.0);
			MFNN_[H][H][Y][x]->AddMult(hOld[Y], hNew[x], 1.0);
			MFNN_[H][H][Z][x]->AddMult(hOld[Z], hNew[x], 1.0);
			MP_[H]			 ->AddMult(hOld[x], hNew[x],-1.0);
			
			MFNN_[E][E][Y][x]->AddMult(eOld[Y], eNew[x], 1.0);
			MFNN_[E][E][X][x]->AddMult(eOld[X], eNew[x], 1.0);
			MFNN_[E][E][Z][x]->AddMult(eOld[Z], eNew[x], 1.0);
			MP_[E]			 ->AddMult(eOld[x], eNew[x],-1.0);
		}

		if (model_.getInteriorBoundaryToMarker().size() != 0) {
			
			MFNB_[H][E][y]->AddMult(eOld[z], hNew[x]);
			MFNB_[H][E][z]->AddMult(eOld[y], hNew[x], -1.0);
			MFNB_[E][H][y]->AddMult(hOld[z], eNew[x], -1.0);
			MFNB_[E][H][z]->AddMult(hOld[y], eNew[x]);

			if (opts_.fluxType == FluxType::Upwind) {

				MFNNB_[H][H][X][x]->AddMult(hOld[X], hNew[x]);
				MFNNB_[H][H][Y][x]->AddMult(hOld[Y], hNew[x]);
				MFNNB_[H][H][Z][x]->AddMult(hOld[Z], hNew[x]);
				MPB_[H]->AddMult(hOld[x], hNew[x], -1.0);

				MFNNB_[E][E][X][x]->AddMult(eOld[X], eNew[x]);
				MFNNB_[E][E][Y][x]->AddMult(eOld[Y], eNew[x]);
				MFNNB_[E][E][Z][x]->AddMult(eOld[Z], eNew[x]);
				MPB_[E]->AddMult(eOld[x], eNew[x], -1.0);
			}
		}
	}

	for (const auto& source : srcmngr_.sources) {
		if (dynamic_cast<Planewave*>(source.get())) {
			
			auto func { evalTimeVarFunction(GetTime(),srcmngr_) };

			std::array<GridFunction, 3> eTemp, hTemp;

			for (int d = X; d <= Z; d++) {
				eTemp[d].SetSpace(srcmngr_.getGlobalTFSFSpace());
				hTemp[d].SetSpace(srcmngr_.getGlobalTFSFSpace());
				eTemp[d] = 0.0;
				hTemp[d] = 0.0;
			}

			for (int x = X; x <= Z; x++) {
				int y = (x + 1) % 3;
				int z = (x + 2) % 3;

				MaxwellTransferMap eMap(eTemp[x], eNew[x]);
				MaxwellTransferMap hMap(hTemp[x], hNew[x]);

				//Centered

				MFN_GTFSF_[H][E][y]->Mult(func[E][z], hTemp[x]);
				eMap.TransferSub(hTemp[x], hNew[x]);
				MFN_GTFSF_[H][E][z]->Mult(func[E][y], hTemp[x]);
				eMap.TransferAdd(hTemp[x], hNew[x]);
				MFN_GTFSF_[E][H][y]->Mult(func[H][z], eTemp[x]);
				eMap.TransferAdd(eTemp[x], eNew[x]);
				MFN_GTFSF_[E][H][z]->Mult(func[H][y], eTemp[x]);
				eMap.TransferSub(eTemp[x], eNew[x]);

				if (opts_.fluxType == FluxType::Upwind) {
					MFNN_GTFSF_[H][H][X][x]->Mult(func[H][X], hTemp[x]);
					hMap.TransferSub(hTemp[x], hNew[x]);
					MFNN_GTFSF_[H][H][Y][x]->Mult(func[H][Y], hTemp[x]);
					hMap.TransferSub(hTemp[x], hNew[x]);
					MFNN_GTFSF_[H][H][Z][x]->Mult(func[H][Z], hTemp[x]);
					hMap.TransferSub(hTemp[x], hNew[x]);
					MP_GTFSF_[H]           ->Mult(func[H][x], hTemp[x]);
					hMap.TransferAdd(hTemp[x], hNew[x]);

					MFNN_GTFSF_[E][E][X][x]->Mult(func[E][X], eTemp[x]);
					eMap.TransferSub(eTemp[x], eNew[x]);
					MFNN_GTFSF_[E][E][Y][x]->Mult(func[E][Y], eTemp[x]);
					eMap.TransferSub(eTemp[x], eNew[x]);
					MFNN_GTFSF_[E][E][Z][x]->Mult(func[E][Z], eTemp[x]);
					eMap.TransferSub(eTemp[x], eNew[x]);
					MP_GTFSF_[E]           ->Mult(func[E][x], eTemp[x]);
					eMap.TransferAdd(eTemp[x], eNew[x]);
				}

			}
		}
	}
}

DynamicMatrix assembleInverseMassMatrix(FiniteElementSpace& fes)
{
	BilinearForm bf(&fes);
	ConstantCoefficient one(1.0);
	bf.AddDomainIntegrator(new InverseIntegrator(new MassIntegrator(one)));
	bf.Assemble();
	bf.Finalize();

	return toEigen(*bf.SpMat().ToDenseMatrix());
}

DynamicMatrix getReferenceInverseMassMatrix(const Element::Type elType, const int order, const int dimension)
{
	std::unique_ptr<Mesh> m;
	switch (dimension) {
	case 2:
		m = std::make_unique<Mesh>(Mesh::MakeCartesian2D(1, 1, elType));
		break;
	case 3:
		m = std::make_unique<Mesh>(Mesh::MakeCartesian3D(1, 1, 1, elType));
		break;
	default:
		throw std::runtime_error("Hesthaven Evolution Operator only supports dimensions 2 or 3.");
	}
	auto fec{ L2_FECollection(order, dimension, BasisType::GaussLobatto) };
	auto fes{ FiniteElementSpace(m.get(), &fec)};
	auto mass_mat{ assembleInverseMassMatrix(fes) };

	DynamicMatrix res = getElementMassMatrixFromGlobal(0, mass_mat);
	return res;
}

GlobalBoundaryMap assembleGlobalBoundaryMap(BoundaryToMarker& markers, FiniteElementSpace& fes)
{
	GlobalBoundaryMap res;

	for (auto& [bdr_cond, marker] : markers) {
		auto bf{ BilinearForm(&fes) };
		bf.AddBdrFaceIntegrator(new mfemExtension::HesthavenFluxIntegrator(1.0), markers[bdr_cond]);
		bf.Assemble();
		bf.Finalize();
		auto bdr_matrix{ toEigen(*bf.SpMat().ToDenseMatrix()) };
		std::vector<int> nodes;
		for (auto r{ 0 }; r < bdr_matrix.rows(); r++) {
			if (bdr_matrix(r, r) != 0.0) { //These conditions would be those of a node on itself, thus we only need to check if the 'self-value' is not zero.
					nodes.push_back(r);
			}
		}
		res.push_back(std::make_pair(bdr_cond, nodes));
	}

	return res;
}

const int returnFaces(const Geometry::Type& geom)
{
	int res;
	Geometry::Dimension[geom] == 2 ? res = Geometry::NumEdges[geom] : res = Geometry::NumFaces[geom];
	return res;
}

const Eigen::VectorXd applyScalingFactors(const HesthavenElement& hestElem, const Eigen::VectorXd& flux)
{
	Eigen::VectorXd res(flux.size());
	const auto numFaces{ returnFaces(hestElem.geom) };
	for (auto f{ 0 }; f < numFaces; f++) {
		const int cols{ int(hestElem.emat[f]->cols()) };
		res(Eigen::seq(f * cols, (f * cols + cols) - 1)) = *hestElem.invmass * *hestElem.emat[f] * flux(Eigen::seq(f * cols, (f * cols + cols) - 1));
	}
	res /= 2.0;
	return res;
}

HesthavenEvolution::HesthavenEvolution(FiniteElementSpace& fes, Model& model, SourcesManager& srcmngr, EvolutionOptions& opts) :
	fes_(fes),
	model_(model),
	srcmngr_(srcmngr),
	opts_(opts)
{
	Array<int> elementMarker;
	elementMarker.Append(hesthavenMeshingTag);

	auto mesh{ Mesh(model.getMesh()) };
	auto fec{ dynamic_cast<const L2_FECollection*>(fes.FEColl()) };

	connectivity_ =  assembleGlobalConnectivityMap(mesh, fec);
	bdr_connectivity_ = assembleGlobalBoundaryMap(model.getBoundaryToMarker(), fes_);

	const auto* cmesh = &model.getConstMesh();
	mesh = Mesh(model.getMesh()); 
	auto attMap{ mapOriginalAttributes(mesh) };

	for (auto e{ 0 }; e < cmesh->GetNE(); e++)
	{
		HesthavenElement hestElem;
		hestElem.id = e;
		hestElem.geom = cmesh->GetElementBaseGeometry(e);

		mesh.SetAttribute(e, hesthavenMeshingTag);
		auto sm = SubMesh::CreateFromDomain(mesh, elementMarker);
		restoreOriginalAttributesAfterSubMeshing(e, mesh, attMap);
		FiniteElementSpace subFES(&sm, dynamic_cast<const L2_FECollection*>(fes.FEColl()));

		for (auto f{ 0 }; f < subFES.GetNF(); f++) {
			sm.SetBdrAttribute(f, sm.bdr_attributes[f]);
		}

		auto inverseMassMatrix{ getReferenceInverseMassMatrix(sm.GetElementType(0), subFES.FEColl()->GetOrder(), sm.Dimension())};
		StorageIterator it = matrixStorage_.find(inverseMassMatrix);
		if (it == matrixStorage_.end()) {
			matrixStorage_.insert(inverseMassMatrix);
			StorageIterator it = matrixStorage_.find(inverseMassMatrix);
			hestElem.invmass = &(*it);
		}
		else {
			hestElem.invmass = &(*it);
		}

		auto boundaryMarkers = assembleBoundaryMarkers(subFES);
		for (auto f{ 0 }; f < sm.GetNEdges(); f++){
			auto surfaceMatrix{ assembleConnectivityFaceMassMatrix(subFES, boundaryMarkers[f]) };
			StorageIterator it = matrixStorage_.find(surfaceMatrix);
			if (it == matrixStorage_.end()) {
				matrixStorage_.insert(surfaceMatrix);
				StorageIterator it = matrixStorage_.find(surfaceMatrix);
				hestElem.emat.push_back(&(*it));
			}
			else {
				hestElem.emat.push_back(&(*it));
			}
		}

		for (auto d{ X }; d <= Z; d++) {
			auto derivativeMatrix{ toEigen(*buildDerivativeOperator(d, subFES)->SpMat().ToDenseMatrix())};
			StorageIterator it = matrixStorage_.find(derivativeMatrix);
			if (it == matrixStorage_.end()) {
				matrixStorage_.insert(derivativeMatrix);
				StorageIterator it = matrixStorage_.find(derivativeMatrix);
				hestElem.dir[d] = &(*it);
			}
			else {
				hestElem.dir[d] = &(*it);
			}
		}

		for (auto f{ 0 }; f < sm.GetNEdges(); f++) {
			Vector normal(sm.SpaceDimension());
			ElementTransformation* faceTrans = sm.GetEdgeTransformation(f);
			faceTrans->SetIntPoint(&Geometries.GetCenter(faceTrans->GetGeometryType()));
			CalcOrtho(faceTrans->Jacobian(), normal);
			hestElem.normals[X].resize(cmesh->GetElement(e)->GetNEdges() * (subFES.FEColl()->GetOrder() + 1));
			hestElem.normals[Y].resize(cmesh->GetElement(e)->GetNEdges() * (subFES.FEColl()->GetOrder() + 1));
			hestElem.normals[Z].resize(cmesh->GetElement(e)->GetNEdges() * (subFES.FEColl()->GetOrder() + 1));
			hestElem.fscale    .resize(cmesh->GetElement(e)->GetNEdges() * (subFES.FEColl()->GetOrder() + 1));
			for (auto b{ 0 }; b < subFES.FEColl()->GetOrder() + 1; b++) { //hesthaven requires normals to be stored per node at face
				hestElem.normals[X][b] = normal[0];
				hestElem.fscale[b] = abs(normal[0]); //likewise for fscale, surface per volume ratio per node at face
				if (sm.SpaceDimension() >= 2) {
					hestElem.normals[Y][b] = normal[1];
					hestElem.fscale[b] += abs(normal[1]);
				}
				if (sm.SpaceDimension() == 3) {
					hestElem.normals[Z][b] = normal[2];
					hestElem.fscale[b] += abs(normal[2]);
				}
			}
		}

		hestElemStorage_.push_back(hestElem);

	}


}

void HesthavenEvolution::Mult(const Vector& in, Vector& out)
{
	// --MAP BETWEEN MFEM VECTOR AND EIGEN VECTOR-- //

	const Eigen::Map<Eigen::VectorXd> Ex_in(in.GetData() + 0 * fes_.GetNDofs(), fes_.GetNDofs(), 1);
	const Eigen::Map<Eigen::VectorXd> Ey_in(in.GetData() + 1 * fes_.GetNDofs(), fes_.GetNDofs(), 1);
	const Eigen::Map<Eigen::VectorXd> Ez_in(in.GetData() + 2 * fes_.GetNDofs(), fes_.GetNDofs(), 1);
	const Eigen::Map<Eigen::VectorXd> Hx_in(in.GetData() + 3 * fes_.GetNDofs(), fes_.GetNDofs(), 1);
	const Eigen::Map<Eigen::VectorXd> Hy_in(in.GetData() + 4 * fes_.GetNDofs(), fes_.GetNDofs(), 1);
	const Eigen::Map<Eigen::VectorXd> Hz_in(in.GetData() + 5 * fes_.GetNDofs(), fes_.GetNDofs(), 1);

	Eigen::Map<Eigen::VectorXd> Ex_out(out.GetData() + 0 * fes_.GetNDofs(), fes_.GetNDofs(), 1);
	Eigen::Map<Eigen::VectorXd> Ey_out(out.GetData() + 1 * fes_.GetNDofs(), fes_.GetNDofs(), 1);
	Eigen::Map<Eigen::VectorXd> Ez_out(out.GetData() + 2 * fes_.GetNDofs(), fes_.GetNDofs(), 1);
	Eigen::Map<Eigen::VectorXd> Hx_out(out.GetData() + 3 * fes_.GetNDofs(), fes_.GetNDofs(), 1);
	Eigen::Map<Eigen::VectorXd> Hy_out(out.GetData() + 4 * fes_.GetNDofs(), fes_.GetNDofs(), 1);
	Eigen::Map<Eigen::VectorXd> Hz_out(out.GetData() + 5 * fes_.GetNDofs(), fes_.GetNDofs(), 1);

	// ---JUMPS--- //

	Eigen::VectorXd dEx(connectivity_.size()), dEy(connectivity_.size()), dEz(connectivity_.size()),
					dHx(connectivity_.size()), dHy(connectivity_.size()), dHz(connectivity_.size());

	for (auto v{ 0 }; v < connectivity_.size(); v++) {
		dEx[v] = Ex_in[connectivity_[v].first] - Ex_in[connectivity_[v].second];
		dEy[v] = Ey_in[connectivity_[v].first] - Ey_in[connectivity_[v].second];
		dEz[v] = Ez_in[connectivity_[v].first] - Ez_in[connectivity_[v].second];
		dHx[v] = Hx_in[connectivity_[v].first] - Hx_in[connectivity_[v].second];
		dHy[v] = Hy_in[connectivity_[v].first] - Hy_in[connectivity_[v].second];
		dHz[v] = Hz_in[connectivity_[v].first] - Hz_in[connectivity_[v].second];
	}

	// --BOUNDARIES-- //

	for (auto m{ 0 }; m < bdr_connectivity_.size(); m++) {
		switch (bdr_connectivity_[m].first) {
		case BdrCond::PEC:
			for (auto v{ 0 }; v < bdr_connectivity_[m].second.size(); v++) {
				dEx[bdr_connectivity_[m].second[v]] -= 2.0 * Ex_in[bdr_connectivity_[m].second[v]];
				dEy[bdr_connectivity_[m].second[v]] -= 2.0 * Ey_in[bdr_connectivity_[m].second[v]];
				dEz[bdr_connectivity_[m].second[v]] -= 2.0 * Ez_in[bdr_connectivity_[m].second[v]];
			}
			break;
		case BdrCond::PMC:
			for (auto v{ 0 }; v < bdr_connectivity_[m].second.size(); v++) {
				dHx[bdr_connectivity_[m].second[v]] -= 2.0 * Hx_in[bdr_connectivity_[m].second[v]];
				dHy[bdr_connectivity_[m].second[v]] -= 2.0 * Hy_in[bdr_connectivity_[m].second[v]];
				dHz[bdr_connectivity_[m].second[v]] -= 2.0 * Hz_in[bdr_connectivity_[m].second[v]];
			}
			break;
		case BdrCond::SMA:
			for (auto v{ 0 }; v < bdr_connectivity_[m].second.size(); v++) {
				dEx[bdr_connectivity_[m].second[v]] -= 1.0 * Ex_in[bdr_connectivity_[m].second[v]];
				dEy[bdr_connectivity_[m].second[v]] -= 1.0 * Ey_in[bdr_connectivity_[m].second[v]];
				dEz[bdr_connectivity_[m].second[v]] -= 1.0 * Ez_in[bdr_connectivity_[m].second[v]];
				dHx[bdr_connectivity_[m].second[v]] -= 1.0 * Hx_in[bdr_connectivity_[m].second[v]];
				dHy[bdr_connectivity_[m].second[v]] -= 1.0 * Hy_in[bdr_connectivity_[m].second[v]];
				dHz[bdr_connectivity_[m].second[v]] -= 1.0 * Hz_in[bdr_connectivity_[m].second[v]];
			}
			break;
		default:
			throw std::runtime_error("Other BdrConds are yet to be implemented for Hesthaven Evolution Operator.");
		}
	}

	// Extend to all elements

	for (auto e{ 0 }; e < fes_.GetNE(); e++) {

		Array<int> dofs;
		auto el2dofs = fes_.GetElementDofs(e, dofs);

		// Dof ordering will always be incremental due to L2 space (i.e: element 0 will have 0, 1, 2... element 1 will have 3, 4, 5...)

		const Eigen::Map<Eigen::VectorXd> dEx_Elem(dEx.data() + e * dofs.Size(), dofs.Size(), 1);
		const Eigen::Map<Eigen::VectorXd> dEy_Elem(dEy.data() + e * dofs.Size(), dofs.Size(), 1);
		const Eigen::Map<Eigen::VectorXd> dEz_Elem(dEz.data() + e * dofs.Size(), dofs.Size(), 1);
		const Eigen::Map<Eigen::VectorXd> dHx_Elem(dHx.data() + e * dofs.Size(), dofs.Size(), 1);
		const Eigen::Map<Eigen::VectorXd> dHy_Elem(dHy.data() + e * dofs.Size(), dofs.Size(), 1);
		const Eigen::Map<Eigen::VectorXd> dHz_Elem(dHz.data() + e * dofs.Size(), dofs.Size(), 1);

		const Eigen::Map<Eigen::VectorXd> Ex_Elem(in.GetData() + e * dofs.Size() + 0 * fes_.GetNDofs(), dofs.Size(), 1);
		const Eigen::Map<Eigen::VectorXd> Ey_Elem(in.GetData() + e * dofs.Size() + 1 * fes_.GetNDofs(), dofs.Size(), 1);
		const Eigen::Map<Eigen::VectorXd> Ez_Elem(in.GetData() + e * dofs.Size() + 2 * fes_.GetNDofs(), dofs.Size(), 1);
		const Eigen::Map<Eigen::VectorXd> Hx_Elem(in.GetData() + e * dofs.Size() + 3 * fes_.GetNDofs(), dofs.Size(), 1);
		const Eigen::Map<Eigen::VectorXd> Hy_Elem(in.GetData() + e * dofs.Size() + 4 * fes_.GetNDofs(), dofs.Size(), 1);
		const Eigen::Map<Eigen::VectorXd> Hz_Elem(in.GetData() + e * dofs.Size() + 5 * fes_.GetNDofs(), dofs.Size(), 1);

		auto ndotdH =	hestElemStorage_[e].normals[X].asDiagonal() * dHx_Elem + 
						hestElemStorage_[e].normals[Y].asDiagonal() * dHy_Elem + 
						hestElemStorage_[e].normals[Z].asDiagonal() * dHz_Elem ;

		auto ndotdE =	hestElemStorage_[e].normals[X].asDiagonal() * dEx_Elem + 
						hestElemStorage_[e].normals[Y].asDiagonal() * dEy_Elem + 
						hestElemStorage_[e].normals[Z].asDiagonal() * dEz_Elem ;

		double alpha{ 1.0 }; // upwind term, to relate to options later.

		Eigen::VectorXd fluxHx = -1.0 * hestElemStorage_[e].normals[Y].asDiagonal() * dEz_Elem +	   hestElemStorage_[e].normals[Z].asDiagonal() * dEy_Elem + alpha * (dHx_Elem - ndotdH.asDiagonal() * hestElemStorage_[e].normals[X]);
		Eigen::VectorXd fluxHy = -1.0 * hestElemStorage_[e].normals[Z].asDiagonal() * dEx_Elem +	   hestElemStorage_[e].normals[X].asDiagonal() * dEz_Elem + alpha * (dHy_Elem - ndotdH.asDiagonal() * hestElemStorage_[e].normals[Y]);
		Eigen::VectorXd fluxHz = -1.0 * hestElemStorage_[e].normals[X].asDiagonal() * dEy_Elem +	   hestElemStorage_[e].normals[Y].asDiagonal() * dEx_Elem + alpha * (dHz_Elem - ndotdH.asDiagonal() * hestElemStorage_[e].normals[Z]);
		Eigen::VectorXd fluxEx =        hestElemStorage_[e].normals[Y].asDiagonal() * dHz_Elem - 1.0 * hestElemStorage_[e].normals[Z].asDiagonal() * dHy_Elem + alpha * (dEx_Elem - ndotdE.asDiagonal() * hestElemStorage_[e].normals[X]);
		Eigen::VectorXd fluxEy =        hestElemStorage_[e].normals[Z].asDiagonal() * dHx_Elem - 1.0 * hestElemStorage_[e].normals[X].asDiagonal() * dHz_Elem + alpha * (dEy_Elem - ndotdE.asDiagonal() * hestElemStorage_[e].normals[Y]);
		Eigen::VectorXd fluxEz =        hestElemStorage_[e].normals[X].asDiagonal() * dHy_Elem - 1.0 * hestElemStorage_[e].normals[Y].asDiagonal() * dHx_Elem + alpha * (dEz_Elem - ndotdE.asDiagonal() * hestElemStorage_[e].normals[Z]);

		Hx_out(Eigen::seq(e * dofs.Size(), (e * dofs.Size() + dofs.Size()) - 1)) = -1.0 * *hestElemStorage_[e].invmass * *hestElemStorage_[e].dir[Y] * Ez_Elem +       *hestElemStorage_[e].invmass * *hestElemStorage_[e].dir[Z] * Ey_Elem + applyScalingFactors(hestElemStorage_[e], fluxHx);
		Hy_out(Eigen::seq(e * dofs.Size(), (e * dofs.Size() + dofs.Size()) - 1)) = -1.0 * *hestElemStorage_[e].invmass * *hestElemStorage_[e].dir[Z] * Ex_Elem +       *hestElemStorage_[e].invmass * *hestElemStorage_[e].dir[X] * Ez_Elem + applyScalingFactors(hestElemStorage_[e], fluxHy);
		Hz_out(Eigen::seq(e * dofs.Size(), (e * dofs.Size() + dofs.Size()) - 1)) = -1.0 * *hestElemStorage_[e].invmass * *hestElemStorage_[e].dir[X] * Ey_Elem +       *hestElemStorage_[e].invmass * *hestElemStorage_[e].dir[Y] * Ex_Elem + applyScalingFactors(hestElemStorage_[e], fluxHz);
		Ex_out(Eigen::seq(e * dofs.Size(), (e * dofs.Size() + dofs.Size()) - 1)) =		  *hestElemStorage_[e].invmass * *hestElemStorage_[e].dir[Y] * Hz_Elem - 1.0 * *hestElemStorage_[e].invmass * *hestElemStorage_[e].dir[Z] * Hy_Elem + applyScalingFactors(hestElemStorage_[e], fluxEx);
		Ey_out(Eigen::seq(e * dofs.Size(), (e * dofs.Size() + dofs.Size()) - 1)) =	      *hestElemStorage_[e].invmass * *hestElemStorage_[e].dir[Z] * Hx_Elem - 1.0 * *hestElemStorage_[e].invmass * *hestElemStorage_[e].dir[X] * Hz_Elem + applyScalingFactors(hestElemStorage_[e], fluxEy);
		Ez_out(Eigen::seq(e * dofs.Size(), (e * dofs.Size() + dofs.Size()) - 1)) =		  *hestElemStorage_[e].invmass * *hestElemStorage_[e].dir[X] * Hy_Elem - 1.0 * *hestElemStorage_[e].invmass * *hestElemStorage_[e].dir[Y] * Hx_Elem + applyScalingFactors(hestElemStorage_[e], fluxEz);


	}

}

}

