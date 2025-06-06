#include <gtest/gtest.h>

#include <mfem.hpp>
#include <mfemExtension/BilinearIntegrators.h>
#include <mfemExtension/LinearIntegrators.h>
#include <TestUtils.h>
#include <components/FarField.h>
#include <components/RCSManager.h>

using namespace maxwell;
using namespace mfem;
using namespace mfemExtension;

class FormTest : public ::testing::Test 
{};

TEST_F(FormTest, LinearForms_1D) 
{

	auto m{ Mesh::MakeCartesian1D(3) };
	auto att = 301;
	m.AddBdrPoint(1, att);
	m.FinalizeMesh();

	auto fec{ DG_FECollection(1,1,BasisType::GaussLobatto) };
	auto fes{ FiniteElementSpace(&m,&fec) };

	auto vec{ Vector(1) }; vec[0] = 5.0;
	auto vcc{ VectorConstantCoefficient(vec) };
	Array<int> int_att_bdr_marker(m.bdr_attributes.Max());
	int_att_bdr_marker[att - 1] = 1;

	auto lf{ LinearForm(&fes) };
	lf = 0.0;
	lf.AddInternalBoundaryFaceIntegrator(
		new BoundaryDGJumpIntegrator(vcc, 1.0), int_att_bdr_marker
	);
	
	lf.Assemble();

}

TEST_F(FormTest, LinearForms_2D) 
{
	
	auto m{ Mesh::LoadFromFile((mfemMeshes2DFolder() + "intbdr_two_quads.mesh").c_str(), 1, 0) };
	auto fec{ DG_FECollection(1,2,BasisType::GaussLegendre) };
	auto fes{ FiniteElementSpace(&m,&fec) };

	auto vec{ Vector(2) }; vec[0] = 5.0; vec[1] = 2.0;
	auto vcc{ VectorConstantCoefficient(vec) };
	Array<int> int_att_bdr_marker(m.bdr_attributes.Max());
	int_att_bdr_marker[m.bdr_attributes.Max() - 1] = 1;

	auto lf{ LinearForm(&fes) };
	lf = 0.0;
	lf.AddInternalBoundaryFaceIntegrator(
		new BoundaryDGJumpIntegrator(vcc, 1.0), int_att_bdr_marker
	);

	lf.Assemble();
	
}

//TEST_F(FormTest, RCSForm_2D)
//{
//	
//	auto m{ Mesh::LoadFromFile((gmshMeshesFolder() + "2D_LinearForm_RCS.msh").c_str(), 1, 0)};
//	auto fec{ DG_FECollection(3, 2) };
//	auto fes{ FiniteElementSpace(&m, &fec) };
//
//	GridFunction gf(&fes);
//	gf = 2.0;
//	LinearForm lf(&fes);
//	FunctionCoefficient fc(buildFC_2D(1e7, 0.0, true));
//	Array<int> bdr_marker(3);
//	bdr_marker = 0;
//	bdr_marker[1] = 1;
//	lf.AddBdrFaceIntegrator(new FarFieldBdrFaceIntegrator(fc, X), bdr_marker);
//	lf.Assemble();
//
//	auto solution = mfem::InnerProduct(lf, gf);
//
//}
//
//TEST_F(FormTest, RCSBdrFaceInt)
//{
//	auto m{ Mesh::LoadFromFile((gmshMeshesFolder() + "2D_BdrIntegratorHalfSize.msh").c_str(), 1, 0) };
//	auto fec{ DG_FECollection(2,2,BasisType::GaussLobatto) };
//	auto fes{ FiniteElementSpace(&m, &fec) };
//
//	LinearForm lf(&fes);
//	FunctionCoefficient fc(buildFC_2D(1e7, 0.0, true));
//	Array<int> bdr_marker(3);
//	bdr_marker = 0;
//	bdr_marker[bdr_marker.Size() - 1] = 1;
//	lf.AddBdrFaceIntegrator(new FarFieldBdrFaceIntegrator(fc, X), bdr_marker);
//	lf.Assemble();
//
//}