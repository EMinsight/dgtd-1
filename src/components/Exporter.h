#pragma once

#include <evolution/Fields.h>
#include <components/Types.h>
#include <components/SubMesher.h>
#include <components/Probes.h>

namespace maxwell {

using namespace mfem;

struct globalFields {

	GridFunction& Ex;
	GridFunction& Ey;
	GridFunction& Ez;
	GridFunction& Hx;
	GridFunction& Hy;
	GridFunction& Hz;

	globalFields(Fields& global) :
		Ex{ global.get(E, X) },
		Ey{ global.get(E, Y) },
		Ez{ global.get(E, Z) },
		Hx{ global.get(H, X) },
		Hy{ global.get(H, Y) },
		Hz{ global.get(H, Z) }
	{}
};

struct TransferMaps {

	TransferMap tMapEx;
	TransferMap tMapEy;
	TransferMap tMapEz;
	TransferMap tMapHx;
	TransferMap tMapHy;
	TransferMap tMapHz;

	TransferMaps(globalFields& src, Fields& dst) :
		tMapEx{ TransferMap(src.Ex, dst.get(E, X)) },
		tMapEy{ TransferMap(src.Ey, dst.get(E, Y)) },
		tMapEz{ TransferMap(src.Ez, dst.get(E, Z)) },
		tMapHx{ TransferMap(src.Hx, dst.get(H, X)) },
		tMapHy{ TransferMap(src.Hy, dst.get(H, Y)) },
		tMapHz{ TransferMap(src.Hz, dst.get(H, Z)) }
	{}

	void transferFields(const globalFields&, Fields&);
};

class NearToFarFieldDataCollection : public DataCollection
{
public:

	NearToFarFieldDataCollection(const NearToFarFieldProbe&, const DG_FECollection& fec, FiniteElementSpace& fes, Fields&);

	NearToFarFieldDataCollection(const NearToFarFieldDataCollection&) = delete;
	NearToFarFieldDataCollection(NearToFarFieldDataCollection&&) = default;

	NearToFarFieldDataCollection& operator=(const NearToFarFieldDataCollection&) = delete;
	NearToFarFieldDataCollection& operator=(NearToFarFieldDataCollection&&) = default;

	~NearToFarFieldDataCollection() = default;

	GridFunction& getCollectionField(const FieldType& f, const Direction& d)  { return fields_.get(f, d) ; }
	void updateFields();

private:

	void assignGlobalFieldsReferences(Fields& global);
	
	NearToFarFieldSubMesher ntff_smsh_;
	std::unique_ptr<FiniteElementSpace> sfes_;
	Fields fields_;
	globalFields gFields_;
	TransferMaps tMaps_;

};

}