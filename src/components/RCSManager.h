#pragma once

#include <mfem.hpp>

#include <iostream>
#include <filesystem>

#include <math.h>
#include <complex>

#include <components/Probes.h>
#include <mfemExtension/LinearIntegrators.h>
#include <adapter/MaxwellAdapter.hpp>

#include <nlohmann/json.hpp>

namespace maxwell {

using namespace mfem;
using Rho = double;
using Phi = double;
using Frequency = double;
using RCSValue = double;
using Freq2Value = std::map<Frequency, RCSValue>;
using SphericalAngles = std::pair<Phi, Rho>;
using ComplexVector = std::vector<std::complex<double>>;
using DFTFreqFieldsComplex = std::vector<ComplexVector>;
using DFTFreqFieldsDouble = std::vector<std::vector<double>>;

struct PlaneWaveData {
	double mean;
	double delay;

	PlaneWaveData(double m, double dl) :
		mean(m),
		delay(dl) {};
};

struct RCSData {
	double RCSvalue;
	double frequency;
	SphericalAngles angles;

	RCSData(double val, double freq, SphericalAngles ang) : 
		RCSvalue(val), 
		frequency(freq),
		angles(ang) 
	{};
};

class RCSManager {
public:

	RCSManager(const std::string& data_path, const std::string& json_path, const std::vector<double>& frequency, const std::vector<SphericalAngles>& angle);

private:

	std::pair<std::complex<double>, std::complex<double>> performRCS2DCalculations(ComplexVector& FAx, ComplexVector& Ay, ComplexVector& Az, const double frequency, const SphericalAngles&);
	DFTFreqFieldsComplex assembleFreqFields(Mesh& mesh, const std::vector<double>& frequencies, const std::string& field);
	void fillPostDataMaps(const std::vector<double>& frequencies, const std::vector<SphericalAngles>& angleVec);
	void getFESFromGF(Mesh& mesh);
	std::vector<double> buildNormalizationTerm(const std::string& json_path, const std::vector<double>& frequencies);
	PlaneWaveData buildPlaneWaveData(const json&);
	
	Mesh m_;
	std::string data_path_;
	std::map<SphericalAngles, Freq2Value> postdata_;
	std::unique_ptr<FiniteElementSpace> fes_;

};

}
