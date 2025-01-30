#include "evolution/HesthavenEvolutionTools.h"

namespace maxwell {

	using namespace mfem;

	HesthavenFields::HesthavenFields(int size)
	{
		for (int d = X; d <= Z; d++) {
			e_[d].resize(size);
			h_[d].resize(size);
		}
	}

	FieldsInputMaps::FieldsInputMaps(const Vector& in, FiniteElementSpace& fes)
	{
		e_.push_back(Eigen::Map<Eigen::VectorXd>(in.GetData() + 0 * fes.GetNDofs(), fes.GetNDofs(), 1));
		e_.push_back(Eigen::Map<Eigen::VectorXd>(in.GetData() + 1 * fes.GetNDofs(), fes.GetNDofs(), 1));
		e_.push_back(Eigen::Map<Eigen::VectorXd>(in.GetData() + 2 * fes.GetNDofs(), fes.GetNDofs(), 1));
		h_.push_back(Eigen::Map<Eigen::VectorXd>(in.GetData() + 3 * fes.GetNDofs(), fes.GetNDofs(), 1));
		h_.push_back(Eigen::Map<Eigen::VectorXd>(in.GetData() + 4 * fes.GetNDofs(), fes.GetNDofs(), 1));
		h_.push_back(Eigen::Map<Eigen::VectorXd>(in.GetData() + 5 * fes.GetNDofs(), fes.GetNDofs(), 1));
	}

	FieldsOutputMaps::FieldsOutputMaps(Vector& out, FiniteElementSpace& fes)
	{
		e_.push_back(Eigen::Map<Eigen::VectorXd>(out.GetData() + 0 * fes.GetNDofs(), fes.GetNDofs(), 1));
		e_.push_back(Eigen::Map<Eigen::VectorXd>(out.GetData() + 1 * fes.GetNDofs(), fes.GetNDofs(), 1));
		e_.push_back(Eigen::Map<Eigen::VectorXd>(out.GetData() + 2 * fes.GetNDofs(), fes.GetNDofs(), 1));
		h_.push_back(Eigen::Map<Eigen::VectorXd>(out.GetData() + 3 * fes.GetNDofs(), fes.GetNDofs(), 1));
		h_.push_back(Eigen::Map<Eigen::VectorXd>(out.GetData() + 4 * fes.GetNDofs(), fes.GetNDofs(), 1));
		h_.push_back(Eigen::Map<Eigen::VectorXd>(out.GetData() + 5 * fes.GetNDofs(), fes.GetNDofs(), 1));
	}

	FieldsElementMaps::FieldsElementMaps(const Vector& in, FiniteElementSpace& fes, const ElementId& id)
	{

		Array<int> dofs;
		auto el2dofs = fes.GetElementDofs(id, dofs);

		e_.push_back(Eigen::Map<Eigen::VectorXd>(in.GetData() + id * dofs.Size() + 0 * fes.GetNDofs(), dofs.Size(), 1));
		e_.push_back(Eigen::Map<Eigen::VectorXd>(in.GetData() + id * dofs.Size() + 1 * fes.GetNDofs(), dofs.Size(), 1));
		e_.push_back(Eigen::Map<Eigen::VectorXd>(in.GetData() + id * dofs.Size() + 2 * fes.GetNDofs(), dofs.Size(), 1));
		h_.push_back(Eigen::Map<Eigen::VectorXd>(in.GetData() + id * dofs.Size() + 3 * fes.GetNDofs(), dofs.Size(), 1));
		h_.push_back(Eigen::Map<Eigen::VectorXd>(in.GetData() + id * dofs.Size() + 4 * fes.GetNDofs(), dofs.Size(), 1));
		h_.push_back(Eigen::Map<Eigen::VectorXd>(in.GetData() + id * dofs.Size() + 5 * fes.GetNDofs(), dofs.Size(), 1));
	}

	HesthavenElementJumps::HesthavenElementJumps(HesthavenFields& in, ElementId id, Eigen::Index& elFluxSize)
	{
		e_.push_back(Eigen::Map<Eigen::VectorXd>(in.e_[X].data() + id * elFluxSize, elFluxSize, 1));
		e_.push_back(Eigen::Map<Eigen::VectorXd>(in.e_[Y].data() + id * elFluxSize, elFluxSize, 1));
		e_.push_back(Eigen::Map<Eigen::VectorXd>(in.e_[Z].data() + id * elFluxSize, elFluxSize, 1));
		h_.push_back(Eigen::Map<Eigen::VectorXd>(in.h_[X].data() + id * elFluxSize, elFluxSize, 1));
		h_.push_back(Eigen::Map<Eigen::VectorXd>(in.h_[Y].data() + id * elFluxSize, elFluxSize, 1));
		h_.push_back(Eigen::Map<Eigen::VectorXd>(in.h_[Z].data() + id * elFluxSize, elFluxSize, 1));
	}

	InteriorFaceConnectivityMaps mapConnectivity(const DynamicMatrix& fluxMatrix)
	{
		// Make map
		std::vector<int> vmapM, vmapP;
		double tol = 1e-5;
		for (auto r{ 0 }; r < fluxMatrix.rows() / 2; r++) {
			for (auto c{ 0 }; c < fluxMatrix.cols(); c++) {
				if (r != c && abs(fluxMatrix(r, c) - fluxMatrix(r, r)) < tol && fluxMatrix(r, r) > tol) {
					vmapM.emplace_back(r);
					vmapP.emplace_back(c);
				}
			}
		}
		return std::make_pair(vmapM, vmapP);
	}

	void applyBoundaryConditionsToNodes(const BoundaryMaps& bdrMaps, const FieldsInputMaps& in, HesthavenFields& out) 
	{
		for (auto m{ 0 }; m < bdrMaps.vmapB.size(); m++) {
			switch (bdrMaps.vmapB[m].first) {
			case BdrCond::PEC:
				for (int d = X; d <= Z; d++) {
					for (auto v{ 0 }; v < bdrMaps.vmapB[m].second.size(); v++) {
						out.e_[d][bdrMaps.mapB[m][v]] = -2.0 * in.e_[d][bdrMaps.vmapB[m].second[v]];
						out.h_[d][bdrMaps.mapB[m][v]] = 0.0;
					}
				}
				break;
			case BdrCond::PMC:
				for (int d = X; d <= Z; d++) {
					for (auto v{ 0 }; v < bdrMaps.vmapB[m].second.size(); v++) {
						out.e_[d][bdrMaps.mapB[m][v]] = 0.0;
						out.h_[d][bdrMaps.mapB[m][v]] = -2.0 * in.h_[d][bdrMaps.vmapB[m].second[v]];
					}
				}
				break;
			case BdrCond::SMA:
				for (int d = X; d <= Z; d++) {
					for (auto v{ 0 }; v < bdrMaps.vmapB[m].second.size(); v++) {
						out.e_[d][bdrMaps.mapB[m][v]] = -1.0 * in.e_[d][bdrMaps.vmapB[m].second[v]];
						out.h_[d][bdrMaps.mapB[m][v]] = -1.0 * in.h_[d][bdrMaps.vmapB[m].second[v]];
					}
				}
				break;
			default:
				throw std::runtime_error("Other BdrConds are yet to be implemented for Hesthaven Evolution Operator.");
			}
		}
	}

	void applyInteriorBoundaryConditionsToNodes(const InteriorBoundaryMaps& intBdrMaps, const FieldsInputMaps& in, HesthavenFields& out)
	{
		applyBoundaryConditionsToNodes(intBdrMaps, in, out);
	}

	std::map<int, Attribute> mapOriginalAttributes(const Mesh& m)
	{
		// Create backup of attributes
		auto res{ std::map<int,Attribute>() };
		for (auto e{ 0 }; e < m.GetNE(); e++) {
			res[e] = m.GetAttribute(e);
		}
		return res;
	}

	void restoreOriginalAttributesAfterSubMeshing(FaceElementTransformations* faceTrans, Mesh& m, const std::map<int, Attribute>& attMap)
	{
		m.SetAttribute(faceTrans->Elem1No, attMap.at(faceTrans->Elem1No));
		if (faceTrans->Elem2No) {
			m.SetAttribute(faceTrans->Elem2No, attMap.at(faceTrans->Elem2No));
		}
	}

	void restoreOriginalAttributesAfterSubMeshing(ElementId e, Mesh& m, const std::map<int, Attribute>& attMap) 
	{
		m.SetAttribute(e, attMap.at(e));
	}

	Array<int> getFacesForElement(const Mesh& m, const ElementId e)
	{
		auto e2f = m.ElementToEdgeTable();
		e2f.Finalize();
		Array<int> res;
		e2f.GetRow(e, res);
		return res;
	}

	const int getNumFaces(const Geometry::Type& geom)
	{
		int res;
		Geometry::Dimension[geom] == 2 ? res = Geometry::NumEdges[geom] : res = Geometry::NumFaces[geom];
		return res;
	}

	const int getFaceNodeNumByGeomType(const FiniteElementSpace& fes)
	{
		int res;
		int order = fes.FEColl()->GetOrder();
		switch (fes.GetMesh()->Dimension()) {
		case 2:
			res = order + 1;
			break;
		case 3:
			fes.GetFE(0)->GetGeomType() == Geometry::Type::TRIANGLE ? res = ((order + 1) * (order + 2) / 2) : res = (order + 1) * (order + 1);
			break;
		default:
			throw std::runtime_error("Method only supports 2D and 3D problems.");
		}
		return res;
	}

	FaceElementTransformations* getInteriorFaceTransformation(Mesh& m_copy, const Array<int>& faces)
	{
		for (auto f{ 0 }; f < faces.Size(); f++) {
			FaceElementTransformations* res = m_copy.GetFaceElementTransformations(faces[f]);
			if (res->GetConfigurationMask() == 31) {
				return res;
			}
		}
		throw std::runtime_error("There is no interior face for the selected element in getInteriorFaceTransformation.");
	}

	void markElementsForSubMeshing(FaceElementTransformations* faceTrans, Mesh& m)
	{
		m.SetAttribute(faceTrans->Elem1No, hesthavenMeshingTag);
		m.SetAttribute(faceTrans->Elem2No, hesthavenMeshingTag);
	}

	DynamicMatrix loadMatrixWithValues(const DynamicMatrix& global, const int startRow, const int startCol)
	{
		DynamicMatrix res;
		res.resize(int(global.rows() / 2), int(global.cols() / 2));
		for (auto r{ startRow }; r < startRow + int(global.rows() / 2); r++) {
			for (auto c{ startCol }; c < startCol + int(global.cols() / 2); c++) {
				res(r - startRow, c - startCol) = global(r, c);
			}
		}
		return res;
	}

	DynamicMatrix getElementMassMatrixFromGlobal(const ElementId e, const DynamicMatrix& global)
	{
		switch (e) {
		case 0:
			return loadMatrixWithValues(global, 0, 0);
		case 1:
			return loadMatrixWithValues(global, int(global.rows() / 2), int(global.cols() / 2));
		default:
			throw std::runtime_error("Incorrect element index for getElementMassMatrixFromGlobal");
		}
	}

	DynamicMatrix getFaceMassMatrixFromGlobal(const DynamicMatrix& global)
	{
		DynamicMatrix res;
		res.resize(int(global.rows() / 2) - 1, int(global.cols() / 2) - 1);
		for (auto r{ 0 }; r < int(global.rows() / 2) - 1; r++) {
			for (auto c{ 0 }; c < int(global.cols() / 2) - 1; c++) {
				res(r, c) = global(r, c);
			}
		}
		return res;
	}

	void removeColumn(DynamicMatrix& matrix, const int colToRemove)
	{
		auto numRows = matrix.rows();
		auto numCols = matrix.cols() - 1;

		if (colToRemove < numCols)
			matrix.block(0, colToRemove, numRows, numCols - colToRemove) = matrix.block(0, colToRemove + 1, numRows, numCols - colToRemove);

		matrix.conservativeResize(numRows, numCols);
	}

	void removeZeroColumns(DynamicMatrix& matrix)
	{
		std::vector<int> cols_to_del;
		for (auto c{ 0 }; c < matrix.cols(); c++) {
			bool isZero = true;
			for (auto r{ 0 }; r < matrix.rows(); r++) {
				if (abs(matrix(r, c) - 0.0) >= 1e-6)
				{
					isZero = false;
					break;
				}
			}
			if (isZero == true) {
				cols_to_del.emplace_back(c);
			}
		}
		for (auto it = cols_to_del.rbegin(); it != cols_to_del.rend(); ++it) {
			removeColumn(matrix, *it);
		}
	}

	std::vector<Array<int>> assembleBoundaryMarkers(FiniteElementSpace& fes)
	{
		std::vector<Array<int>> res;
		for (auto f{ 0 }; f < fes.GetNF(); f++) {
			Array<int> bdr_marker;
			bdr_marker.SetSize(fes.GetNF());
			bdr_marker = 0;
			bdr_marker[f] = 1;
			res.emplace_back(bdr_marker);
		}
		return res;
	}

	std::unique_ptr<BilinearForm> assembleFaceMassBilinearForm(FiniteElementSpace& fes, Array<int>& boundaryMarker)
	{
		auto res = std::make_unique<BilinearForm>(&fes);
		res->AddBdrFaceIntegrator(new mfemExtension::HesthavenFluxIntegrator(1.0), boundaryMarker);
		res->Assemble();
		res->Finalize();

		return res;
	}

	DynamicMatrix assembleConnectivityFaceMassMatrix(FiniteElementSpace& fes, Array<int> boundaryMarker)
	{
		auto bf = assembleFaceMassBilinearForm(fes, boundaryMarker);
		auto res = toEigen(*bf->SpMat().ToDenseMatrix());
		removeZeroColumns(res);
		return res;
	}

	DynamicMatrix assembleEmat(FiniteElementSpace& fes, std::vector<Array<int>>& boundaryMarkers)
	{
		DynamicMatrix res;
		auto numNodesAtFace = getFaceNodeNumByGeomType(fes);
		res.resize(fes.GetNDofs(), fes.GetNF() * numNodesAtFace);
		for (auto f{ 0 }; f < fes.GetNF(); f++) {
			auto surface_matrix{ assembleConnectivityFaceMassMatrix(fes, boundaryMarkers[f]) };
			res.block(0, f * numNodesAtFace, fes.GetNDofs(), numNodesAtFace) = surface_matrix;
		}
		return res;
	}

	SubMesh assembleInteriorFaceSubMesh(Mesh& m, const FaceElementTransformations& trans, const FaceToGeomTag& attMap)
	{
		Array<int> volume_marker;
		volume_marker.Append(hesthavenMeshingTag);
		m.SetAttribute(trans.Elem1No, hesthavenMeshingTag);
		m.SetAttribute(trans.Elem2No, hesthavenMeshingTag);
		auto res{ SubMesh::CreateFromDomain(m, volume_marker) };
		restoreOriginalAttributesAfterSubMeshing(trans.Elem1No, m, attMap);
		restoreOriginalAttributesAfterSubMeshing(trans.Elem2No, m, attMap);
		return res;
	}

	DynamicMatrix assembleInteriorFluxMatrix(FiniteElementSpace& fes)
	{
		BilinearForm bf(&fes);
		bf.AddInteriorFaceIntegrator(new mfemExtension::HesthavenFluxIntegrator(1.0));
		bf.Assemble();
		bf.Finalize();
		return toEigen(*bf.SpMat().ToDenseMatrix());
	}

	DynamicMatrix assembleBoundaryFluxMatrix(FiniteElementSpace& fes)
	{
		BilinearForm bf(&fes);
		bf.AddBdrFaceIntegrator(new mfemExtension::HesthavenFluxIntegrator(1.0));
		bf.Assemble();
		bf.Finalize();
		return toEigen(*bf.SpMat().ToDenseMatrix());
	}

	void appendConnectivityMapsFromInteriorFace(const FaceElementTransformations& trans, FiniteElementSpace& globalFES, FiniteElementSpace& smFES, GlobalConnectivity& map, ElementId e)
	{
		auto matrix{ assembleInteriorFluxMatrix(smFES) };
		auto maps{ mapConnectivity(matrix) };
		Array<int> dofs1, dofs2;
		globalFES.GetElementDofs(trans.Elem1No, dofs1);
		globalFES.GetElementDofs(trans.Elem2No, dofs2);

		ConnectivityVector sortingVector;
		if (trans.Elem1No == e) {
			for (auto v{ 0 }; v < maps.first.size(); v++) {
				sortingVector.push_back(std::pair(dofs1[maps.first[v]], dofs2[maps.second[v] - dofs1.Size()]));
			}
			sort(sortingVector.begin(), sortingVector.end());
		}
		else if (trans.Elem2No == e){
			for (auto v{ 0 }; v < maps.first.size(); v++) {
				sortingVector.push_back(std::pair(dofs2[maps.second[v] - dofs1.Size()], dofs1[maps.first[v]]));
			}
			sort(sortingVector.begin(), sortingVector.end());
			//std::sort(sortingVector.begin(), sortingVector.end(), [](const std::pair<int, int>& left, const std::pair<int, int>& right) {
			//	return left.second < right.second;
			//});
		}
		else {
			throw std::runtime_error("incorrect element id to element ids in FaceElementTransformation");
		}


		for (auto v{ 0 }; v < sortingVector.size(); v++) {
			map.push_back(sortingVector[v]);
		}
	}

	void appendConnectivityMapsFromBoundaryFace(FiniteElementSpace& globalFES, FiniteElementSpace& submeshFES, const DynamicMatrix& surfaceMatrix, GlobalConnectivity& map)
	{
		GridFunction gfParent(&globalFES);
		GridFunction gfChild(&submeshFES);
		auto mesh = dynamic_cast<SubMesh*>(submeshFES.GetMesh());
		auto transferMap = mesh->CreateTransferMap(gfChild, gfParent);
		ConstantCoefficient zero(0.0);
		double tol{ 1e-8 };
		for (auto r{ 0 }; r < surfaceMatrix.rows(); r++) {
			if (abs(surfaceMatrix(r, 0)) > tol) {
				gfChild.ProjectCoefficient(zero);
				gfParent.ProjectCoefficient(zero);
				gfChild[r] = 1.0;
				transferMap.Transfer(gfChild, gfParent); 
				ConnectivityVector sortingVector;
				for (auto v{ 0 }; v < gfParent.Size(); v++) {
					if (gfParent[v] != 0.0) {
						sortingVector.push_back(std::make_pair(v, v));
						break;
					}
				}
				sort(sortingVector.begin(), sortingVector.end());
				for (auto v{ 0 }; v < sortingVector.size(); v++) {
					map.push_back(sortingVector[0]);
				}
			}
		}
	}

	void tagBdrAttributesForSubMesh(const int edge, SubMesh& sm)
	{
		for (auto b{ 0 }; b < sm.bdr_attributes.Size(); b++) {
			sm.bdr_attributes[b] = 1;
			sm.SetBdrAttribute(b, 1);
		}
		sm.SetBdrAttribute(edge, hesthavenMeshingTag);
		sm.bdr_attributes.Append(hesthavenMeshingTag);
	}

	GlobalConnectivity assembleGlobalConnectivityMap(Mesh& m, const L2_FECollection* fec)
	{
		GlobalConnectivity res;
		auto mesh{ Mesh(m) };
		FiniteElementSpace globalFES(&mesh, fec);

		std::map<FaceId, bool> global_face_is_interior;
		int numFaces;
		m.Dimension() == 2 ? numFaces = mesh.GetNEdges() : numFaces = mesh.GetNFaces();
		for (auto f{ 0 }; f < numFaces; f++) {
			global_face_is_interior[f] = mesh.FaceIsInterior(f);
		}

		Table globalElementToFace;
		m.Dimension() == 2 ? globalElementToFace = mesh.ElementToEdgeTable() : globalElementToFace = mesh.ElementToFaceTable();

		Array<int> volumeMarker;
		volumeMarker.Append(hesthavenMeshingTag);

		Array<int> boundaryMarker(hesthavenMeshingTag);
		boundaryMarker = 0;
		boundaryMarker[hesthavenMeshingTag - 1] = 1;

		auto attMap{ mapOriginalAttributes(mesh) };


		for (auto e{ 0 }; e < mesh.GetNE(); e++) {

			Array<int> localFaceIndexToGlobalFaceIndex;
			globalElementToFace.GetRow(e, localFaceIndexToGlobalFaceIndex);

			int numLocalFaces;
			mesh.Dimension() == 2 ? numLocalFaces = mesh.GetElement(e)->GetNEdges() : numLocalFaces = mesh.GetElement(e)->GetNFaces();
			for (auto localFace{ 0 }; localFace < numLocalFaces; localFace++) {

				if (!global_face_is_interior[localFaceIndexToGlobalFaceIndex[localFace]]) {

					mesh.SetAttribute(e, hesthavenMeshingTag);
					auto sm = SubMesh::CreateFromDomain(mesh, volumeMarker);
					restoreOriginalAttributesAfterSubMeshing(e, mesh, attMap);
					tagBdrAttributesForSubMesh(localFace, sm);
					FiniteElementSpace smFES(&sm, fec);
					appendConnectivityMapsFromBoundaryFace(
						globalFES,
						smFES,
						assembleConnectivityFaceMassMatrix(smFES, boundaryMarker),
						res);

				}
				else {

					FaceElementTransformations* faceTrans = mesh.GetFaceElementTransformations(localFaceIndexToGlobalFaceIndex[localFace]);
					auto sm = assembleInteriorFaceSubMesh(mesh, *faceTrans, attMap);
					restoreOriginalAttributesAfterSubMeshing(faceTrans, mesh, attMap);
					FiniteElementSpace smFES(&sm, fec);
					appendConnectivityMapsFromInteriorFace(
						*faceTrans,
						globalFES,
						smFES,
						res,
						e);

				}
			}
		}
		return res;
	}
}