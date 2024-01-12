#include <gtest/gtest.h>

#define _USE_MATH_DEFINES
#include <math.h>

#include <mfem.hpp>

#include "TestUtils.h"

using namespace mfem;

double xPositionFunction(const Vector& pos)
{
	return pos[0];
};

double negFunction(const Vector& pos)
{

	double normalizedPos = 0.0;
	double leftBoundary = 0.0, rightBoundary = 5.0;
	double length = rightBoundary - leftBoundary;
	normalizedPos = (pos[0] - leftBoundary) / length;
	
	return -2 * normalizedPos;
}

double linearDummyFunction(const Vector& v, double time)
{
	return v[0] + time;
}

double RotatedGaussianFunction(Vector& pos, double time) {
	return 1.0 *
		exp(
			-pow(pos[0] - (3.5 * cos(-M_PI / 4.0)) + pos[1] - (3.5 * -sin(-M_PI / 4.0)), 2.0) /
			(2.0 * pow(0.2, 2.0))
		);
}


class GridFunctionTest : public ::testing::Test {
protected:

	typedef std::size_t Direction;

	void SetUp() override
	{
		mesh_ = Mesh::MakeCartesian1D(1);
		fec_ = std::make_unique<DG_FECollection>(1, 1, BasisType::GaussLobatto);
		fes_ = std::make_unique<FiniteElementSpace>(&mesh_, fec_.get());
	}

	void setFES1D(
		const int order,
		const int elements = 1,
		const double length = 1.0,
		const decltype(BasisType::GaussLobatto)& bt = BasisType::GaussLobatto)
	{
		mesh_ = Mesh::MakeCartesian1D(elements, length);
		fec_ = std::make_unique<DG_FECollection>(order, 1, bt);
		fes_ = std::make_unique<FiniteElementSpace>(&mesh_, fec_.get());

	}

	void setFES2D(
		const int order,
		const int xElem = 1,
		const int yElem = 1)
	{
		mesh_ = Mesh::MakeCartesian2D(xElem, yElem, Element::Type::QUADRILATERAL);
		fec_ = std::make_unique<DG_FECollection>(order, mesh_.Dimension(), BasisType::GaussLobatto);
		fes_ = std::make_unique<FiniteElementSpace>(&mesh_, fec_.get());
	}

	Mesh mesh_;
	std::unique_ptr<FiniteElementCollection> fec_;
	std::unique_ptr<FiniteElementSpace> fes_;

	Array<int> elArray_;
	Array<IntegrationPoint> ipArray_;

	double tol_ = 1e-5;

};

TEST_F(GridFunctionTest, ElementIDFinder1D)
{
	setFES1D(2, 5, 5.0);
	
	DenseMatrix pointMat_({{0.3},{2.5},{4.0}});
	pointMat_.Transpose();
	mesh_.FindPoints(pointMat_,elArray_,ipArray_);

	Array<int> expectedID({ 0, 2, 3 });

	for (int i = 0; i < elArray_.Size(); i++) {
		EXPECT_EQ(expectedID[i], elArray_[i]);
	}

}

TEST_F(GridFunctionTest, IntegrationPointFinder1D)
{
	setFES1D(2, 5, 5.0);

	DenseMatrix pointMat({ {0.3},{2.5},{4.0} });
	pointMat.Transpose();
	mesh_.FindPoints(pointMat, elArray_, ipArray_);

	IntegrationPoint a, b, c;
	a.Set(0.3, 0.0, 0.0, 1.0);
	b.Set(0.5, 0.0, 0.0, 1.0);
	c.Set(1.0, 0.0, 0.0, 1.0);
	Array<IntegrationPoint> expectedIP({ a,b,c });

	for (int i = 0; i < elArray_.Size(); i++) {
		EXPECT_EQ(expectedIP[i].x, ipArray_[i].x);
	}

}

TEST_F(GridFunctionTest, GetValuesAtPoints1D)
{
	auto m{ Mesh::MakeCartesian1D(4) };
	DG_FECollection fec{ 2, 1, BasisType::GaussLobatto };
	FiniteElementSpace fes{ &m, &fec };

	DenseMatrix pointMat{ 
		{ 
			{0.0}, // Point at left boundary.
			{0.1}, // Point within an element in the mesh.
			{0.5}, // Point in the boundary between two elements.
			{1.0}, // Point at right boundary.
			{1.1}  // Point out of the mesh.
		} 
	};
	pointMat.Transpose();

	Array<int> elArray;
	Array<IntegrationPoint> ipArray;
	m.FindPoints(pointMat, elArray, ipArray);

	GridFunction gridFunction{ &fes };
	FunctionCoefficient fc{ xPositionFunction };
	gridFunction.ProjectCoefficient(fc);

	Array<int> expectedID{
		{
			0, // Id of first element (at left boundary).
			0, // Id of first element.
			1, // Lower Id of the neighbouring elements.
			3, // Id of last element (at right boundary).
			-1 // Negative for failure to find.
		}
	};
	for (int i = 0; i < elArray.Size(); i++) {
		EXPECT_EQ(expectedID[i], elArray[i]);
	}
	Vector expectedValues({ 0.0, 0.1, 0.5, 1.0, 999.0 });
	for (int i = 0; i < elArray.Size(); i++) {
		if (elArray[i] < 0) {
			continue;
		}
		EXPECT_EQ(gridFunction.GetValue(elArray[i], ipArray[i]), expectedValues[i]);
	}

}

TEST_F(GridFunctionTest, SetValuesAtPoints1D)
{
	/*Create a 1D FES playground with the following form
	
	   0      1      2      3      4
	|-----||-----||-----||-----||-----|
	0     12     34     56     78     9
	      -+                   +-  
	
	Where the goal is to project certain Coefficients on a GridFunction
	which has assigned the FES we've built. The DoF with a + sign
	will have a positive addition to their initially projected
	value (0.5) and the - sign will have a negative
	value added in the form of (-0.5).
	
	This is a prelude to PlaneWave implementation.*/
	
	setFES1D(1, 5, 5.0);

	Array<int> positiveDoF{ {2,7} }, negativeDoF{ {1,8} };

	GridFunction gridBase(fes_.get());
	ConstantCoefficient oneCoeff{ 1.0 };
	gridBase.ProjectCoefficient(oneCoeff);

	GridFunction adder(fes_.get()), subber(fes_.get());
	adder = 0.0; 
	subber = 0.0;
	ConstantCoefficient halfCoeff{ 0.5 };
	adder.ProjectCoefficient(halfCoeff, positiveDoF);
	ConstantCoefficient minusHalfCoeff{ -0.5 };
	subber.ProjectCoefficient(minusHalfCoeff, negativeDoF);

	gridBase.operator+=(adder);
	gridBase.operator+=(subber);

	EXPECT_EQ(0.5, gridBase.Elem(1));
	EXPECT_EQ(1.5, gridBase.Elem(2));
	EXPECT_EQ(1.5, gridBase.Elem(7));
	EXPECT_EQ(0.5, gridBase.Elem(8));
}

TEST_F(GridFunctionTest, TimeDependentGridFunction)
{
	setFES1D(1, 5, 5.0);
	GridFunction f{ fes_.get() };
	FunctionCoefficient function(linearDummyFunction);
	
	f.ProjectCoefficient(function);
	EXPECT_EQ(0.0, f[0]);

	for (auto time{ 0.0 }; time <= 5.0; time += 1.0) {
		function.SetTime(time);
		f.ProjectCoefficient(function);
	}
	EXPECT_EQ(5.0, f[0]);
}

TEST_F(GridFunctionTest, ProjectBdrFunction)
{
	const auto order{ 1 };
	
	auto mesh{ Mesh::MakeCartesian1D(5, 5.0) };
	mesh.AddBdrPoint(2, 2);

	H1_FECollection fec{ order, 1, BasisType::GaussLobatto };
	FiniteElementSpace fes{ &mesh, &fec };

	GridFunction f{ &fes };
	f = 0.0;
	FunctionCoefficient function(linearDummyFunction);
	Array<int> attr{ 2 };
	f.ProjectBdrCoefficient(function, attr);

	EXPECT_EQ(0.0, f[0]);
	EXPECT_EQ(2.0, f[2]);
}

TEST_F(GridFunctionTest, ProjectFunctionOnMesh)
{
	int order{ 4 };
	
	auto mesh{ 
		Mesh::LoadFromFile(
			(mfemMeshes2DFolder() + "severalrotatedquadsnormalised.mesh").c_str(), 1, 0
		) 
	};
	DG_FECollection fec{ order, 2, BasisType::GaussLobatto };
	FiniteElementSpace fes{ &mesh, &fec };

	GridFunction proj{ &fes };
	FunctionCoefficient RotatedGaussian(RotatedGaussianFunction);
	proj.ProjectCoefficient(RotatedGaussian);

	DenseMatrix pointMat{
		{
			{3.5 * cos(-M_PI / 4.0), 3.5 * -sin(-M_PI / 4.0) } // Center of Gaussian.
		}
	};
	pointMat.Transpose();

	Array<int> elArray;
	Array<IntegrationPoint> ipArray;
	mesh.FindPoints(pointMat, elArray, ipArray);

	Vector expectedValue({ 1.0 });
	EXPECT_NEAR(proj.GetValue(elArray[0], ipArray[0]), expectedValue[0], 1e-5);
}

TEST_F(GridFunctionTest, ProjectBetweenDifferentBasis)
{
	auto mesh{ Mesh::MakeCartesian2D(2, 2, Element::QUADRILATERAL, true) };

	auto dgfec{ DG_FECollection(1, 2) };
	auto ndfec{ ND_FECollection(1, 2) };

	auto dgfes{ FiniteElementSpace(&mesh, &dgfec) };
	auto ndfes{ FiniteElementSpace(&mesh, &ndfec) };

	GridFunction dg_gf_x(&dgfes);
	GridFunction dg_gf_y(&dgfes);
	GridFunction nd_gf_x(&ndfes);
	GridFunction nd_gf_y(&ndfes);	
	GridFunction nd_gf_global(&ndfes);
	dg_gf_x = 1.0;
	dg_gf_y = 1.0;
	nd_gf_global = 0.0;

	dg_gf_x.SetTrueVector();
	dg_gf_x.SetFromTrueVector();
	dg_gf_y.SetTrueVector();
	dg_gf_y.SetFromTrueVector();

	BilinearForm mass(&ndfes);
	MixedBilinearForm dg_nd_mbf_x(&dgfes, &ndfes);
	MixedBilinearForm dg_nd_mbf_y(&dgfes, &ndfes);

	ConstantCoefficient one(1.0);
	Vector x{ {1.0, 0.0} };
	VectorConstantCoefficient vcc_x(x);
	Vector y{ {0.0, 1.0} };
	VectorConstantCoefficient vcc_y(y);

	dg_nd_mbf_x.AddDomainIntegrator(new MixedVectorProductIntegrator(vcc_x));
	dg_nd_mbf_y.AddDomainIntegrator(new MixedVectorProductIntegrator(vcc_y));

	mass.Assemble();
	mass.Finalize();
	dg_nd_mbf_x.Assemble();
	dg_nd_mbf_x.Finalize();
	dg_nd_mbf_y.Assemble();
	dg_nd_mbf_y.Finalize();

	SparseMatrix& mixed_x = dg_nd_mbf_x.SpMat();
	mixed_x.Mult(dg_gf_x, nd_gf_x);
	SparseMatrix& mixed_y = dg_nd_mbf_y.SpMat();
	mixed_y.Mult(dg_gf_y, nd_gf_y);

	nd_gf_global += nd_gf_x;
	nd_gf_global += nd_gf_y;

	ParaViewDataCollection* pd = NULL;
	pd = new ParaViewDataCollection("Example", &mesh);
	pd->SetPrefixPath("Nedelec");
	pd->RegisterField("Galerkin Solution X", &dg_gf_x);
	pd->RegisterField("Galerkin Solution Y", &dg_gf_y);
	pd->RegisterField("Nedelec Solution X", &nd_gf_x);
	pd->RegisterField("Nedelec Solution Y", &nd_gf_y);
	pd->RegisterField("Nedelec Solution Global", &nd_gf_global);
	pd->SetLevelsOfDetail(1);
	pd->SetDataFormat(VTKFormat::BINARY);
	pd->SetHighOrderOutput(true);
	pd->SetCycle(0);
	pd->SetTime(0.0);
	pd->Save();
	
}