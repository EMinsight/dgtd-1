#include "Types.h"
#include "mfem.hpp"
#include "Model.h"
#include "mfemExtension/BilinearIntegrators.h"
#include "mfemExtension/BilinearForm_IBFI.hpp"
#include "mfemExtension/LinearIntegrators.h"

namespace maxwell {

using namespace mfem;
using FiniteElementOperator = std::unique_ptr<BilinearForm>;
using FiniteElementVector = std::unique_ptr<LinearForm>;

FiniteElementOperator buildByMult				(const BilinearForm& op1,const BilinearForm& op2, FiniteElementSpace&);
FiniteElementOperator buildInverseMassMatrix	(const FieldType&, const Model&, FiniteElementSpace&);
FiniteElementOperator buildDerivativeOperator	(const Direction&, FiniteElementSpace&);
FiniteElementOperator buildFluxOperator			(const FieldType&, const std::vector<Direction>&, bool usePenaltyCoefficients, Model&, FiniteElementSpace&, const MaxwellEvolOptions&);
FiniteElementOperator buildFluxJumpOperator		(const FieldType&, const std::vector<Direction>&, bool usePenaltyCoefficients, Model&, FiniteElementSpace&, const MaxwellEvolOptions&);
FiniteElementOperator buildFluxOperator			(const FieldType&, const std::vector<Direction>&, Model&, FiniteElementSpace&);
FiniteElementOperator buildPenaltyOperator		(const FieldType&, const std::vector<Direction>&, Model&, FiniteElementSpace&, const MaxwellEvolOptions&);
FiniteElementOperator buildFunctionOperator		(const FieldType&, const std::vector<Direction>&, Model&, FiniteElementSpace&);

FiniteElementOperator buildFluxOperator1D		(const FieldType&, const std::vector<Direction>&, Model&, FiniteElementSpace&);
FiniteElementOperator buildPenaltyOperator1D	(const FieldType&, const std::vector<Direction>&, Model&, FiniteElementSpace&, const MaxwellEvolOptions&);
FiniteElementOperator buildFunctionOperator1D	(const FieldType&, Model&, FiniteElementSpace&);
FiniteElementVector   buildBoundaryFunctionVector1D     (const FieldType&, Model&, FiniteElementSpace&);

FluxCoefficient interiorFluxCoefficient();
FluxCoefficient interiorPenaltyFluxCoefficient	(const MaxwellEvolOptions&);
FluxCoefficient boundaryFluxCoefficient			(const FieldType&, const BdrCond&);
FluxCoefficient boundaryPenaltyFluxCoefficient	(const FieldType&, const BdrCond&, const MaxwellEvolOptions&);

FieldType altField(const FieldType& f);

}