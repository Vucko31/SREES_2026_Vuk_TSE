#include "ConverterCore.h"

#include <algorithm>
#include <cmath>
#include <complex>
#include <cctype>
#include <cstdlib>
#include <fstream>
#include <iomanip>
#include <sstream>
#include <stdexcept>
#include <vector>
#include <dense/Matrix.h>
#include <sparse/IMatrix.h>

namespace
{
constexpr double pi = 3.1415926535897932384626433832795;
constexpr int busColID = 0;
constexpr int busColType = 1;
constexpr int busColVM = 2;
constexpr int busColVA = 3;
constexpr int busColBaseKV = 4;
constexpr int busColPhase = 5;

std::string trim(std::string s)
{
    auto isSpace = [](unsigned char ch) { return std::isspace(ch) != 0; };
    s.erase(s.begin(), std::find_if(s.begin(), s.end(), [&](char ch) { return !isSpace((unsigned char) ch); }));
    s.erase(std::find_if(s.rbegin(), s.rend(), [&](char ch) { return !isSpace((unsigned char) ch); }).base(), s.end());
    return s;
}

std::string lower(std::string s)
{
    std::transform(s.begin(), s.end(), s.begin(), [](unsigned char ch) { return (char) std::tolower(ch); });
    return s;
}

bool endsWithCI(const std::string& value, const std::string& suffix)
{
    if (suffix.size() > value.size())
        return false;
    return lower(value.substr(value.size() - suffix.size())) == lower(suffix);
}

std::string stripMatlabComment(const std::string& line)
{
    bool inString = false;
    for (size_t i = 0; i < line.size(); ++i)
    {
        if (line[i] == '\'')
            inString = !inString;
        else if (!inString && line[i] == '%')
            return line.substr(0, i);
    }
    return line;
}

bool readBusLine(const std::string& line, int& id, int& type, double& vm, double& vaDeg, double& baseKV, int& nCols)
{
    std::istringstream in(line);
    double busID = 0.0;
    double busType = 0.0;
    double pd = 0.0;
    double qd = 0.0;
    double gs = 0.0;
    double bs = 0.0;
    double area = 0.0;
    nCols = 0;

    if (!(in >> busID))
        return false;
    ++nCols;
    if (!(in >> busType))
        return false;
    ++nCols;
    if (!(in >> pd))
        return false;
    ++nCols;
    if (!(in >> qd))
        return false;
    ++nCols;
    if (!(in >> gs))
        return false;
    ++nCols;
    if (!(in >> bs))
        return false;
    ++nCols;
    if (!(in >> area))
        return false;
    ++nCols;
    if (!(in >> vm))
        return false;
    ++nCols;
    if (!(in >> vaDeg))
        return false;
    ++nCols;

    id = (int) busID;
    type = (int) busType;
    baseKV = 0.0;
    if (in >> baseKV)
        ++nCols;
    return true;
}

bool parseMatlabNumber(const std::string& token, double& value)
{
    const size_t slashPos = token.find('/');
    if (slashPos != std::string::npos)
    {
        double num = 0.0;
        double den = 0.0;
        if (!parseMatlabNumber(token.substr(0, slashPos), num) ||
            !parseMatlabNumber(token.substr(slashPos + 1), den) || den == 0.0)
            return false;
        value = num / den;
        return true;
    }

    char* end = nullptr;
    value = std::strtod(token.c_str(), &end);
    return end != token.c_str() && end != nullptr && *end == '\0';
}

std::vector<double> parseMatlabNumericRow(std::string line)
{
    for (char& ch : line)
    {
        if (ch == '[' || ch == ']' || ch == ';' || ch == ',')
            ch = ' ';
    }

    std::vector<double> row;
    std::istringstream in(line);
    std::string token;
    while (in >> token)
    {
        double value = 0.0;
        if (!parseMatlabNumber(token, value))
            return {};
        row.push_back(value);
    }
    return row;
}

bool startsMatpowerSection(const std::string& clean, const std::string& sectionName)
{
    const std::string lhs = "mpc." + sectionName;
    if (clean.rfind(lhs, 0) != 0)
        return false;

    size_t pos = lhs.size();
    while (pos < clean.size() && std::isspace((unsigned char) clean[pos]))
        ++pos;
    return pos < clean.size() && clean[pos] == '=';
}

std::vector<std::vector<double>> readMatpowerSectionRows(const std::string& inputPath,
                                                         const std::string& sectionName,
                                                         std::atomic_bool& cancelRequested,
                                                         int minCols)
{
    std::ifstream in(inputPath);
    if (!in)
        throw std::runtime_error("Ne mogu otvoriti MATPOWER fajl.");

    std::vector<std::vector<double>> rows;
    std::string line;
    bool inSection = false;
    int lineNo = 0;

    while (std::getline(in, line))
    {
        if (cancelRequested)
            throw std::runtime_error("Konverzija je prekinuta.");

        ++lineNo;
        std::string clean = trim(stripMatlabComment(line));
        if (clean.empty())
            continue;

        if (!inSection)
        {
            if (!startsMatpowerSection(clean, sectionName))
                continue;

            const size_t bracketPos = clean.find('[');
            if (bracketPos == std::string::npos)
                continue;

            inSection = true;
            clean = clean.substr(bracketPos + 1);
        }

        const size_t closePos = clean.find(']');
        const bool closesSection = closePos != std::string::npos;
        if (closesSection)
            clean = clean.substr(0, closePos);

        clean = trim(clean);
        if (!clean.empty())
        {
            auto row = parseMatlabNumericRow(clean);
            if ((int) row.size() < minCols)
            {
                std::ostringstream err;
                err << "MATPOWER " << sectionName << " red " << lineNo
                    << " nema minimalno " << minCols << " kolona.";
                throw std::runtime_error(err.str());
            }
            rows.push_back(row);
        }

        if (closesSection)
            return rows;
    }

    return rows;
}

std::complex<double> polarDeg(double mag, double deg)
{
    const double rad = deg * pi / 180.0;
    return {mag * std::cos(rad), mag * std::sin(rad)};
}

std::string replaceExtension(const std::string& path, const std::string& extension)
{
    const size_t slashPos = path.find_last_of("\\/");
    const size_t dotPos = path.find_last_of('.');
    if (dotPos != std::string::npos && (slashPos == std::string::npos || dotPos > slashPos))
        return path.substr(0, dotPos) + extension;
    return path + extension;
}

void report(const ProgressCallback& cb, double value, const std::string& msg)
{
    if (cb)
        cb(std::clamp(value, 0.0, 1.0), msg);
}

char phaseChar(int phase)
{
    return phase == 1 ? 'A' : (phase == 2 ? 'B' : (phase == 3 ? 'C' : 'X'));
}

std::string busSignalName(int busID, int phase)
{
    std::ostringstream name;
    name << busID;
    if (phase >= 1 && phase <= 3)
        name << phase;
    return name.str();
}

std::string busLabel(int busID, int phase)
{
    std::ostringstream label;
    label << "bus " << busID;
    if (phase >= 1 && phase <= 3)
        label << " " << phaseChar(phase);
    return label.str();
}

int busPhaseIndex(dense::DblMatrix& buses, int busID, int phase)
{
    auto bus = buses.getManipulator();
    for (int row = 0; row < (int) buses.getNoOfRows(); ++row)
    {
        if ((int) std::round(bus(row, busColID)) == busID &&
            (int) std::round(bus(row, busColPhase)) == phase)
            return row;
    }
    return -1;
}

double readMatpowerScalar(const std::string& inputPath, const std::string& name, double fallback)
{
    std::ifstream in(inputPath);
    if (!in)
        return fallback;

    const std::string lhs = "mpc." + name;
    std::string line;
    while (std::getline(in, line))
    {
        std::string clean = trim(stripMatlabComment(line));
        if (!startsMatpowerSection(clean, name))
            continue;
        const size_t eq = clean.find('=');
        if (eq == std::string::npos)
            continue;
        clean = trim(clean.substr(eq + 1));
        if (!clean.empty() && clean.back() == ';')
            clean.pop_back();
        double value = fallback;
        if (parseMatlabNumber(trim(clean), value))
            return value;
    }
    return fallback;
}

std::complex<double> cplxFromDense(const dense::CmplxMatrix& matrix, int row, int col)
{
    auto m = const_cast<dense::CmplxMatrix&>(matrix).getManipulator();
    const td::cmplx value = m(row, col);
    return {value.real(), value.imag()};
}

void setCplxDense(dense::CmplxMatrix& matrix, int row, int col, const std::complex<double>& value)
{
    auto m = matrix.getManipulator();
    m(row, col) = td::cmplx(value.real(), value.imag());
}

void addCplxDense(dense::CmplxMatrix& matrix, int row, int col, const std::complex<double>& value)
{
    setCplxDense(matrix, row, col, cplxFromDense(matrix, row, col) + value);
}

bool invert3x3(const dense::CmplxMatrix& zMatrix, dense::CmplxMatrix& yMatrix)
{
    auto z = [&](int row, int col) { return cplxFromDense(zMatrix, row, col); };

    const auto det =
        z(0, 0) * (z(1, 1) * z(2, 2) - z(1, 2) * z(2, 1)) -
        z(0, 1) * (z(1, 0) * z(2, 2) - z(1, 2) * z(2, 0)) +
        z(0, 2) * (z(1, 0) * z(2, 1) - z(1, 1) * z(2, 0));
    if (std::abs(det) < 1e-12)
        return false;

    setCplxDense(yMatrix, 0, 0,  (z(1, 1) * z(2, 2) - z(1, 2) * z(2, 1)) / det);
    setCplxDense(yMatrix, 0, 1, -(z(0, 1) * z(2, 2) - z(0, 2) * z(2, 1)) / det);
    setCplxDense(yMatrix, 0, 2,  (z(0, 1) * z(1, 2) - z(0, 2) * z(1, 1)) / det);
    setCplxDense(yMatrix, 1, 0, -(z(1, 0) * z(2, 2) - z(1, 2) * z(2, 0)) / det);
    setCplxDense(yMatrix, 1, 1,  (z(0, 0) * z(2, 2) - z(0, 2) * z(2, 0)) / det);
    setCplxDense(yMatrix, 1, 2, -(z(0, 0) * z(1, 2) - z(0, 2) * z(1, 0)) / det);
    setCplxDense(yMatrix, 2, 0,  (z(1, 0) * z(2, 1) - z(1, 1) * z(2, 0)) / det);
    setCplxDense(yMatrix, 2, 1, -(z(0, 0) * z(2, 1) - z(0, 1) * z(2, 0)) / det);
    setCplxDense(yMatrix, 2, 2,  (z(0, 0) * z(1, 1) - z(0, 1) * z(1, 0)) / det);
    return true;
}

int yParamIndex(int row, int col, int nStates)
{
    return row * nStates + col + 1;
}

std::string voltageVarName(int row)
{
    std::ostringstream name;
    name << "v" << row + 1;
    return name.str();
}

std::string admittanceParamName(int row, int col)
{
    std::ostringstream name;
    name << "y" << row + 1 << "x" << col + 1;
    return name.str();
}

bool isReferenceBusPhase(const dense::DblMatrix& buses, int row)
{
    auto bus = buses.getManipulator();
    return (int) std::round(bus(row, busColType)) == 3;
}

bool hasExplicitReferenceBusPhase(const dense::DblMatrix& buses)
{
    for (int row = 0; row < (int) buses.getNoOfRows(); ++row)
    {
        if (isReferenceBusPhase(buses, row))
            return true;
    }
    return false;
}

bool isEstimatorReferenceBusPhase(const dense::DblMatrix& buses, int row)
{
    if (isReferenceBusPhase(buses, row))
        return true;
    if (hasExplicitReferenceBusPhase(buses))
        return false;

    auto bus = const_cast<dense::DblMatrix&>(buses).getManipulator();
    return (int) std::round(bus(row, busColID)) == (int) std::round(bus(0, busColID));
}

void appendPowerExpression(std::ostream& out, const dense::CmplxMatrix& ybus, int row, int nStates, bool activePower)
{
    bool first = true;
    for (int col = 0; col < nStates; ++col)
    {
        const std::complex<double> y = cplxFromDense(ybus, row, col);
        if (std::abs(y) < 1e-10)
            continue;

        const int r = row + 1;
        const int c = col + 1;
        const int p = yParamIndex(row, col, nStates);
        if (!first)
            out << " + ";
        first = false;

        if (activePower)
        {
            out << "er" << r << "*(Gn" << p << "*er" << c << "-Bn" << p << "*ei" << c << ")"
                << " + ei" << r << "*(Bn" << p << "*er" << c << "+Gn" << p << "*ei" << c << ")";
        }
        else
        {
            out << "ei" << r << "*(Gn" << p << "*er" << c << "-Bn" << p << "*ei" << c << ")"
                << " - er" << r << "*(Bn" << p << "*er" << c << "+Gn" << p << "*ei" << c << ")";
        }
    }
    if (first)
        out << "0";
}

std::complex<double> busVoltage(const dense::DblMatrix& buses, int row)
{
    auto bus = const_cast<dense::DblMatrix&>(buses).getManipulator();
    return polarDeg(bus(row, busColVM), bus(row, busColVA));
}

std::complex<double> powerInjectionFromYbus(const dense::DblMatrix& buses,
                                            const dense::CmplxMatrix& ybus,
                                            int row)
{
    std::complex<double> current(0.0, 0.0);
    for (int col = 0; col < (int) buses.getNoOfRows(); ++col)
        current += cplxFromDense(ybus, row, col) * busVoltage(buses, col);
    return busVoltage(buses, row) * std::conj(current);
}

// Solve a dense real linear system A*x = b by Gaussian elimination with partial
// pivoting. A and b are modified in place. Returns false if the matrix is
// singular.
bool solveDenseLinear(dense::DblMatrix& A, std::vector<double>& b,
                      std::vector<double>& x)
{
    const int n = (int) b.size();
    auto a = A.getManipulator();
    for (int col = 0; col < n; ++col)
    {
        int piv = col;
        double best = std::abs(a(col, col));
        for (int r = col + 1; r < n; ++r)
        {
            const double candidate = std::abs(a(r, col));
            if (candidate > best)
            {
                best = candidate;
                piv = r;
            }
        }
        if (best < 1e-14)
            return false;
        for (int c = col; c < n; ++c)
        {
            const double tmp = a(piv, c);
            a(piv, c) = a(col, c);
            a(col, c) = tmp;
        }
        std::swap(b[piv], b[col]);
        for (int r = col + 1; r < n; ++r)
        {
            const double factor = a(r, col) / a(col, col);
            for (int c = col; c < n; ++c)
                a(r, c) = a(r, c) - factor * a(col, c);
            b[r] -= factor * b[col];
        }
    }
    x.assign(n, 0.0);
    for (int row = n - 1; row >= 0; --row)
    {
        double s = b[row];
        for (int c = row + 1; c < n; ++c)
            s -= a(row, c) * x[c];
        x[row] = s / a(row, row);
    }
    return true;
}

// Solve the three-phase power flow with a rectangular-coordinate Newton-Raphson
// so the exported "true" state carries the real voltage drop and unbalance.
// Slack bus-phases keep their profile voltage; every other bus-phase is solved
// from its specified complex-power injection sSpec and the Ybus model. Falls
// back to the flat initial guess if the iteration cannot converge.
std::vector<std::complex<double>> solveThreePhasePowerFlow(
    const dense::CmplxMatrix& ybus,
    const std::vector<std::complex<double>>& vInit,
    const std::vector<std::complex<double>>& sSpec,
    const std::vector<bool>& isSlack)
{
    const int n = (int) vInit.size();
    std::vector<std::complex<double>> v = vInit;

    std::vector<int> unknown(n, -1);
    int m = 0;
    for (int i = 0; i < n; ++i)
        if (!isSlack[i])
            unknown[i] = m++;
    if (m == 0)
        return v;

    auto mismatch = [&](const std::vector<std::complex<double>>& vv, std::vector<double>& f)
    {
        f.assign(2 * m, 0.0);
        for (int i = 0; i < n; ++i)
        {
            if (isSlack[i])
                continue;
            std::complex<double> current(0.0, 0.0);
            for (int j = 0; j < n; ++j)
                current += cplxFromDense(ybus, i, j) * vv[j];
            const std::complex<double> d = sSpec[i] - vv[i] * std::conj(current);
            f[2 * unknown[i]] = d.real();
            f[2 * unknown[i] + 1] = d.imag();
        }
    };

    std::vector<double> f;
    mismatch(v, f);
    const double tol = 1e-10;
    const double h = 1e-7;
    for (int iter = 0; iter < 50; ++iter)
    {
        double maxF = 0.0;
        for (double val : f)
        {
            const double a = std::abs(val);
            if (a > maxF)
                maxF = a;
        }
        if (maxF < tol)
            break;

        dense::DblMatrix J(2 * m, 2 * m, nullptr, true);
        auto j = J.getManipulator();
        std::vector<double> fp;
        for (int i = 0; i < n; ++i)
        {
            if (isSlack[i])
                continue;
            const int k = unknown[i];
            std::vector<std::complex<double>> vp = v;
            vp[i] = v[i] + std::complex<double>(h, 0.0);
            mismatch(vp, fp);
            for (int r = 0; r < 2 * m; ++r)
                j(r, 2 * k) = (fp[r] - f[r]) / h;
            vp[i] = v[i] + std::complex<double>(0.0, h);
            mismatch(vp, fp);
            for (int r = 0; r < 2 * m; ++r)
                j(r, 2 * k + 1) = (fp[r] - f[r]) / h;
        }

        std::vector<double> rhs(2 * m);
        for (int r = 0; r < 2 * m; ++r)
            rhs[r] = -f[r];
        std::vector<double> dx;
        if (!solveDenseLinear(J, rhs, dx))
            return vInit;
        for (int i = 0; i < n; ++i)
        {
            if (isSlack[i])
                continue;
            const int k = unknown[i];
            v[i] = v[i] + std::complex<double>(dx[2 * k], dx[2 * k + 1]);
        }
        mismatch(v, f);
    }
    return v;
}

void appendComplexLiteral(std::ostream& out, const std::complex<double>& value)
{
    out << value.real();
    if (value.imag() >= 0.0)
        out << "+";
    out << value.imag() << "i";
}

void appendComplexCurrentSum(std::ostream& out, const dense::CmplxMatrix& ybus, int row, int nStates)
{
    bool first = true;
    for (int col = 0; col < nStates; ++col)
    {
        const std::complex<double> y = cplxFromDense(ybus, row, col);
        if (std::abs(y) < 1e-10)
            continue;

        if (!first)
            out << " + ";
        first = false;

        out << admittanceParamName(row, col) << "*" << voltageVarName(col);
    }
    if (first)
        out << "0";
}

dense::DblMatrix loadMatpowerBuses(const std::string& inputPath,
                                   std::atomic_bool& cancelRequested,
                                   const ProgressCallback& onProgress)
{
    report(onProgress, 0.05, "Ucitavam MATPOWER case");

    auto bus3pRows = readMatpowerSectionRows(inputPath, "bus3p", cancelRequested, 9);
    if (!bus3pRows.empty())
    {
        dense::DblMatrix buses((int) bus3pRows.size() * 3, 6, nullptr, true);
        auto bus = buses.getManipulator();
        int row = 0;
        for (const auto& src : bus3pRows)
        {
            for (int phase = 1; phase <= 3; ++phase)
            {
                bus(row, busColID) = src[0];
                bus(row, busColType) = src[1];
                bus(row, busColVM) = src[2 + phase];
                bus(row, busColVA) = src[5 + phase];
                bus(row, busColBaseKV) = src[2];
                bus(row, busColPhase) = phase;
                ++row;
            }
        }
        return buses;
    }

    auto busRows = readMatpowerSectionRows(inputPath, "bus", cancelRequested, 9);
    if (!busRows.empty())
    {
        dense::DblMatrix buses((int) busRows.size(), 6, nullptr, true);
        auto bus = buses.getManipulator();
        for (int row = 0; row < (int) busRows.size(); ++row)
        {
            const auto& src = busRows[row];
            bus(row, busColID) = src[0];
            bus(row, busColType) = src[1];
            bus(row, busColVM) = src[7];
            bus(row, busColVA) = src[8];
            bus(row, busColBaseKV) = src.size() > 9 ? src[9] : 0.0;
            bus(row, busColPhase) = 0;
        }
        return buses;
    }

    throw std::runtime_error("Nisam pronasao mpc.bus3p ni mpc.bus podatke u MATPOWER fajlu.");
}

ConversionResult convertMatpower(const std::string& inputPath,
                                 const std::string& outputPath,
                                 const ConversionOptions& options,
                                 std::atomic_bool& cancelRequested,
                                 const ProgressCallback& onProgress)
{
    auto buses = loadMatpowerBuses(inputPath, cancelRequested, onProgress);
    auto bus = buses.getManipulator();
    const int nBuses = (int) buses.getNoOfRows();

    if (endsWithCI(outputPath, ".dmodl"))
    {
        std::ofstream out(outputPath);
        if (!out)
            throw std::runtime_error("Ne mogu kreirati dTwin .dmodl fajl.");

        double baseKVA = readMatpowerScalar(inputPath, "basekVA", 0.0);
        if (baseKVA <= 0.0)
            baseKVA = readMatpowerScalar(inputPath, "baseMVA", 1.0) * 1000.0;
        const double freqHz = readMatpowerScalar(inputPath, "freq", 60.0);
        dense::CmplxMatrix ybus(nBuses, nBuses, nullptr, true);
        dense::DblMatrix pqMeas(nBuses, 2, nullptr, true);
        dense::DblMatrix pqEnabled(nBuses, 1, nullptr, true);
        auto pq = pqMeas.getManipulator();
        auto pqFlag = pqEnabled.getManipulator();

        auto lcRows = readMatpowerSectionRows(inputPath, "lc", cancelRequested, 19);
        auto lineRows = readMatpowerSectionRows(inputPath, "line3p", cancelRequested, 6);
        auto xfmrRows = readMatpowerSectionRows(inputPath, "xfmr3p", cancelRequested, 9);
        auto loadRows = readMatpowerSectionRows(inputPath, "load3p", cancelRequested, 9);
        auto genRows = readMatpowerSectionRows(inputPath, "gen3p", cancelRequested, 12);
        auto shuntRows = readMatpowerSectionRows(inputPath, "shunt3p", cancelRequested, 9);

        for (const auto& line : lineRows)
        {
            if ((int) std::round(line[3]) == 0)
                continue;
            const int fbus = (int) std::round(line[1]);
            const int tbus = (int) std::round(line[2]);
            const int lcid = (int) std::round(line[4]);
            const double len = line[5];

            const std::vector<double>* lc = nullptr;
            for (const auto& row : lcRows)
            {
                if ((int) std::round(row[0]) == lcid)
                {
                    lc = &row;
                    break;
                }
            }
            if (!lc)
                continue;

            const double baseKV = busPhaseIndex(buses, fbus, 1) >= 0 ? bus(busPhaseIndex(buses, fbus, 1), busColBaseKV) : 1.0;
            const double zBase = baseKV * baseKV * 1000.0 / baseKVA;
            dense::CmplxMatrix z(3, 3, nullptr, true);
            int rCol[3][3] = {{1, 2, 3}, {2, 4, 5}, {3, 5, 6}};
            int xCol[3][3] = {{7, 8, 9}, {8, 10, 11}, {9, 11, 12}};
            int cCol[3][3] = {{13, 14, 15}, {14, 16, 17}, {15, 17, 18}};
            for (int r = 0; r < 3; ++r)
            {
                for (int c = 0; c < 3; ++c)
                    setCplxDense(z, r, c, std::complex<double>((*lc)[rCol[r][c]], (*lc)[xCol[r][c]]) * len / zBase);
            }

            dense::CmplxMatrix y(3, 3, nullptr, true);
            if (!invert3x3(z, y))
                continue;

            for (int r = 0; r < 3; ++r)
            {
                for (int c = 0; c < 3; ++c)
                {
                    const int fi = busPhaseIndex(buses, fbus, r + 1);
                    const int ti = busPhaseIndex(buses, tbus, r + 1);
                    const int fj = busPhaseIndex(buses, fbus, c + 1);
                    const int tj = busPhaseIndex(buses, tbus, c + 1);
                    const std::complex<double> yValue = cplxFromDense(y, r, c);
                    if (fi >= 0 && fj >= 0)
                        addCplxDense(ybus, fi, fj, yValue);
                    if (ti >= 0 && tj >= 0)
                        addCplxDense(ybus, ti, tj, yValue);
                    if (fi >= 0 && tj >= 0)
                        addCplxDense(ybus, fi, tj, -yValue);
                    if (ti >= 0 && fj >= 0)
                        addCplxDense(ybus, ti, fj, -yValue);

                    const std::complex<double> yCharge(0.0, 2.0 * pi * freqHz * (*lc)[cCol[r][c]] * 1e-9 * len * zBase);
                    if (fi >= 0 && fj >= 0)
                        addCplxDense(ybus, fi, fj, 0.5 * yCharge);
                    if (ti >= 0 && tj >= 0)
                        addCplxDense(ybus, ti, tj, 0.5 * yCharge);
                }
            }
        }

        for (const auto& xf : xfmrRows)
        {
            if ((int) std::round(xf[3]) == 0)
                continue;
            const int fbus = (int) std::round(xf[1]);
            const int tbus = (int) std::round(xf[2]);
            const double ratio = std::abs(xf[8]) > 1e-12 ? xf[8] : 1.0;
            // xfmr3p R,X are per-unit on the transformer's own rating
            // (basekVA=xf[6], basekV=xf[7]), not ohms. Convert that per-unit
            // impedance to the system per-unit base before forming the
            // admittance: Z_sys = Z_xfmr * (Ssys/Sxfmr) * (Vxfmr/Vsys)^2.
            const int fRow = busPhaseIndex(buses, fbus, 1);
            const double sysBaseKV = fRow >= 0 ? bus(fRow, busColBaseKV) : xf[7];
            const double baseScale = (baseKVA / xf[6]) * (xf[7] * xf[7]) / (sysBaseKV * sysBaseKV);
            const std::complex<double> y = 1.0 / (std::complex<double>(xf[4], xf[5]) * baseScale);
            for (int phase = 1; phase <= 3; ++phase)
            {
                const int fi = busPhaseIndex(buses, fbus, phase);
                const int ti = busPhaseIndex(buses, tbus, phase);
                if (fi < 0 || ti < 0)
                    continue;
                addCplxDense(ybus, fi, fi, y / (ratio * ratio));
                addCplxDense(ybus, ti, ti, y);
                addCplxDense(ybus, fi, ti, -y / ratio);
                addCplxDense(ybus, ti, fi, -y / ratio);
            }
        }

        for (const auto& load : loadRows)
        {
            if ((int) std::round(load[2]) == 0)
                continue;
            const int lbus = (int) std::round(load[1]);
            for (int phase = 1; phase <= 3; ++phase)
            {
                const int idx = busPhaseIndex(buses, lbus, phase);
                if (idx < 0)
                    continue;
                const double p = load[2 + phase] / baseKVA;
                const double pf = std::clamp(load[5 + phase], 0.01, 1.0);
                const double q = p * std::tan(std::acos(pf));
                pq(idx, 0) = pq(idx, 0) - p;
                pq(idx, 1) = pq(idx, 1) - q;
                pqFlag(idx, 0) = 1.0;
            }
        }

        for (const auto& gen : genRows)
        {
            if ((int) std::round(gen[2]) == 0)
                continue;
            const int gbus = (int) std::round(gen[1]);
            for (int phase = 1; phase <= 3; ++phase)
            {
                const int idx = busPhaseIndex(buses, gbus, phase);
                if (idx < 0)
                    continue;
                pq(idx, 0) = pq(idx, 0) + gen[5 + phase] / baseKVA;
                pq(idx, 1) = pq(idx, 1) + gen[8 + phase] / baseKVA;
                if ((int) std::round(bus(idx, busColType)) != 3)
                    pqFlag(idx, 0) = 1.0;
            }
        }

        for (const auto& sh : shuntRows)
        {
            if ((int) std::round(sh[2]) == 0)
                continue;
            const int shbus = (int) std::round(sh[1]);
            for (int phase = 1; phase <= 3; ++phase)
            {
                const int idx = busPhaseIndex(buses, shbus, phase);
                if (idx < 0)
                    continue;
                const std::complex<double> y(sh[2 + phase] / baseKVA, sh[5 + phase] / baseKVA);
                addCplxDense(ybus, idx, idx, y);
            }
        }

        dense::DblMatrix referenceMask(nBuses, 1, nullptr, true);
        dense::DblMatrix visitedMask(nBuses, 1, nullptr, true);
        auto reference = referenceMask.getManipulator();
        auto visited = visitedMask.getManipulator();
        for (int start = 0; start < nBuses; ++start)
        {
            if (visited(start, 0) > 0.5)
                continue;

            std::vector<int> component;
            std::vector<int> stack;
            stack.push_back(start);
            visited(start, 0) = 1.0;
            while (!stack.empty())
            {
                const int row = stack.back();
                stack.pop_back();
                component.push_back(row);
                const int rowBus = (int) std::round(bus(row, busColID));
                for (int col = 0; col < nBuses; ++col)
                {
                    if (visited(col, 0) > 0.5)
                        continue;
                    const int colBus = (int) std::round(bus(col, busColID));
                    const bool samePhysicalBus = rowBus == colBus;
                    const bool connectedByY = std::abs(cplxFromDense(ybus, row, col)) > 1e-10 ||
                                              std::abs(cplxFromDense(ybus, col, row)) > 1e-10;
                    if (!samePhysicalBus && !connectedByY)
                        continue;
                    visited(col, 0) = 1.0;
                    stack.push_back(col);
                }
            }

            int refBus = -1;
            for (int row : component)
            {
                if ((int) std::round(bus(row, busColType)) == 3)
                {
                    refBus = (int) std::round(bus(row, busColID));
                    break;
                }
            }
            if (refBus < 0)
            {
                for (int row : component)
                {
                    if ((int) std::round(bus(row, busColType)) == 2)
                    {
                        refBus = (int) std::round(bus(row, busColID));
                        break;
                    }
                }
            }
            if (refBus < 0 && !component.empty())
                refBus = (int) std::round(bus(component.front(), busColID));

            for (int row : component)
            {
                if ((int) std::round(bus(row, busColID)) == refBus)
                    reference(row, 0) = 1.0;
            }
        }

        auto isReferenceRow = [&](int row) { return reference(row, 0) > 0.5; };
        auto isInjectionRow = [&](int row) { return pqFlag(row, 0) > 0.5 && !isReferenceRow(row); };
        auto isZeroInjectionRow = [&](int row) { return !isReferenceRow(row) && !isInjectionRow(row); };

        // MATPOWER bus3p carries the phasor voltage profile; that is only the
        // initial guess and slack reference. The physical operating state (with
        // the real voltage drop and phase unbalance) is obtained by solving the
        // three-phase power flow from the load/gen injections and the Ybus model.
        // The measurements are then generated from that solved state so they are
        // internally consistent with the exported "true" voltages vt.
        std::vector<std::complex<double>> vTrue(nBuses), sMeas(nBuses);
        std::vector<std::complex<double>> vProfile(nBuses), sSpec(nBuses);
        std::vector<bool> isSlackVec(nBuses);
        for (int row = 0; row < nBuses; ++row)
        {
            vProfile[row] = busVoltage(buses, row);
            sSpec[row] = std::complex<double>(pq(row, 0), pq(row, 1));
            isSlackVec[row] = isReferenceRow(row);
        }
        vTrue = solveThreePhasePowerFlow(ybus, vProfile, sSpec, isSlackVec);
        for (int row = 0; row < nBuses; ++row)
        {
            std::complex<double> current(0.0, 0.0);
            for (int col = 0; col < nBuses; ++col)
                current += cplxFromDense(ybus, row, col) * vTrue[col];
            sMeas[row] = vTrue[row] * std::conj(current);
        }

        out << std::setprecision(12);
        out << "Header:\n";
        out << "\tmaxIter=50\n";
        out << "\treport=AllDetails\n";
        out << "end\n";
        out << "// Three-phase WLS state-estimation model generated from MATPOWER three-phase data\n";
        out << "// Native complex-coordinate estimator: v1, v2, ... are complex bus-phase voltages.\n";
        out << "// Uses bus3p, lc, line3p, xfmr3p, load3p, gen3p and shunt3p.\n";
        out << "Model [type=WLS domain=cmplx eps=1e-5 name=\"Three Phase Complex WLS State Estimation\"]:\n";
        out << "Vars [out=true]:\n";
        for (int row = 0; row < nBuses; ++row)
        {
            out << "\t" << voltageVarName(row) << "=init" << row + 1 << "\n";
        }
        out << "Params:\n";
        out << "\tw_inj=1111.1112 [type=real]\n";
        out << "\tw_zi=1000000 [type=real]\n";
        out << "\tw_ref=1000000 [type=real]\n";
        for (int row = 0; row < nBuses; ++row)
        {
            out << "\tinit" << row + 1 << "=";
            appendComplexLiteral(out, vTrue[row]);
            out << "\n";
        }
        for (int row = 0; row < nBuses; ++row)
        {
            if (!isReferenceRow(row))
                continue;
            out << "\tsl" << row + 1 << "=";
            appendComplexLiteral(out, vTrue[row]);
            out << "\n";
        }
        for (int row = 0; row < nBuses; ++row)
        {
            for (int col = 0; col < nBuses; ++col)
            {
                const std::complex<double> y = cplxFromDense(ybus, row, col);
                if (std::abs(y) < 1e-10)
                    continue;
                out << "\t" << admittanceParamName(row, col) << "=";
                appendComplexLiteral(out, y);
                out << "\n";
            }
        }
        for (int row = 0; row < nBuses; ++row)
        {
            out << "\tvt" << row + 1 << "=" << std::abs(vTrue[row]) << " [type=real]\n";
            out << "\tvm" << row + 1 << "=vt" << row + 1 << " [type=real out=true]\n";
            out << "\tst" << row + 1 << "=";
            appendComplexLiteral(out, sMeas[row]);
            out << "\n";
            out << "\tsm" << row + 1 << "=st" << row + 1 << " [out=true]\n";
        }
        for (int row = 0; row < nBuses; ++row)
        {
            out << "\tvest" << row + 1 << "=vt" << row + 1 << " [type=real out=true]\n";
        }
        out << "Distribs:\n";
        out << "\tgs [type=Gauss mean=0 dev=0.01 width=0.04]\n";
        out << "Stats:\n";
        out << "\tstat\n";
        out << "PreProc:\n";
        for (int row = 0; row < nBuses; ++row)
        {
            if (!isInjectionRow(row))
                continue;
            out << "\tsm" << row + 1 << " = st" << row + 1 << " + rnd(gs)\n";
        }
        out << "WLSEs:\n";
        for (int row = 0; row < nBuses; ++row)
        {
            if (!isReferenceRow(row))
                continue;
            out << "\t[w=w_ref]\t" << voltageVarName(row) << " = sl" << row + 1 << "\n";
            out << "\t[w=w_ref]\tconj(" << voltageVarName(row) << ") = conj(sl" << row + 1 << ")\n";
        }
        for (int row = 0; row < nBuses; ++row)
        {
            if (isReferenceRow(row))
                continue;
            out << "\t[w=" << (isZeroInjectionRow(row) ? "w_zi" : "w_inj") << "]\t"
                << voltageVarName(row) << "*conj(";
            appendComplexCurrentSum(out, ybus, row, nBuses);
            out << ") = sm" << row + 1 << "\n";
            out << "\t[w=" << (isZeroInjectionRow(row) ? "w_zi" : "w_inj") << "]\tconj("
                << voltageVarName(row) << ")*(";
            appendComplexCurrentSum(out, ybus, row, nBuses);
            out << ") = conj(sm" << row + 1 << ")\n";
        }
        out << "PostProc:\n";
        for (int row = 0; row < nBuses; ++row)
        {
            out << "\tvest" << row + 1 << " = abs(" << voltageVarName(row) << ")\n";
        }
        out << "end\n";
        const std::string visualPath = replaceExtension(outputPath, ".vmodl");
        std::ofstream visual(visualPath);
        if (!visual)
            throw std::runtime_error("Ne mogu kreirati dTwin .vmodl fajl.");

        visual << "Header:\n";
        visual << "\tnewTab = false\n";
        visual << "\tdrawPlots = true\n";
        visual << "end\n";
        const char* colorL[] = {
            "black", "red", "blue", "darkGreen", "orange", "magenta",
            "darkCyan", "purple", "saddleBrown", "gold", "lime", "coral",
            "forestGreen", "crimson", "cyan", "darkYellow", "chocolate", "hotPink",
            "firebrick", "navy", "olive", "deepSkyBlue", "indigo", "seaGreen",
            "brown", "violet", "darkMagenta", "tangerine", "darkBlue", "green",
            "yellow", "maroon", "turquoise", "dimGray", "royalBlue", "darkRed"
        };
        const char* colorD[] = {
            "white", "orange", "cyan", "lime", "yellow", "hotPink",
            "aqua", "violet", "coral", "gold", "springGreen", "salmon",
            "springGreen", "magenta", "turquoise", "lemonChiffon", "tangerine", "pink",
            "lightSalmon", "lightBlue", "lightYellow", "dodgerBlue", "plum", "mediumSpringGreen",
            "skyBlue", "orchid", "mint", "paleTurquoise", "cyan", "lime",
            "yellow", "lightCyan", "mediumTurquoise", "white", "deepSkyBlue", "red"
        };
        const int colorCount = (int) (sizeof(colorL) / sizeof(colorL[0]));

        visual << "Model [name=\"Three-phase WLS state-estimation results\"]:\n";
        visual << "Plots [backColor=auto]:\n";
        visual << "\tlinePlot [maxY=2 xLabel=\"Iterations\" yLabel=\"Log(eps)\" name=\"Precision\" legend=true nCols=1 anchor=TR anchorX=40 anchorY=30]:\n";
        visual << "\t\t@x << baseIter#\n";
        visual << "\t\t@y << log(obtEps# + 1e-20) [colorL=green colorD=yellow width=4 pattern=solid name=\"eps\"]\n";
        visual << "\t\t@cond -> repeat# == 0\n";
        visual << "\tend\n";
        visual << "\tlinePlot [minY=0.75 maxY=1.2 xLabel=\"solution\" yLabel=\"Voltage [p.u.]\" name=\"Estimated phase voltage magnitudes\" anchor=TR legend=true nCols=2 anchorX=45 anchorY=145]:\n";
        visual << "\t\t@x << baseIter#\n";
        for (int row = 0; row < nBuses; ++row)
        {
            const int busID = (int) std::round(bus(row, busColID));
            const int phase = (int) std::round(bus(row, busColPhase));
            visual << "\t\t@y << abs(" << voltageVarName(row) << ") [width=2 pattern=solid"
                   << " colorL=" << colorL[row % colorCount] << " colorD=" << colorD[row % colorCount]
                   << " name=\"V" << busSignalName(busID, phase) << "\"]\n";
        }
        visual << "\tend\n";
        visual << "\tlinePlot [xLabel=\"solution\" yLabel=\"Angle [rad]\" name=\"Estimated phase voltage angles\" anchor=TR legend=true nCols=2 anchorX=45 anchorY=145]:\n";
        visual << "\t\t@x << baseIter#\n";
        for (int row = 0; row < nBuses; ++row)
        {
            const int busID = (int) std::round(bus(row, busColID));
            const int phase = (int) std::round(bus(row, busColPhase));
            visual << "\t\t@y << atg2(" << voltageVarName(row) << ") [width=2 pattern=solid"
                   << " colorL=" << colorL[row % colorCount] << " colorD=" << colorD[row % colorCount]
                   << " name=\"ang" << busSignalName(busID, phase) << "\"]\n";
        }
        visual << "\tend\n";
        visual << "end\n";

        std::ostringstream preview;
        preview << "MATPOWER three-phase data -> dTwin complex-coordinate WLS state-estimation model\n";
        preview << "Digital model: " << outputPath << "\n";
        preview << "Visual model: " << visualPath << "\n";
        preview << "Noise: Gaussian g_s for complex power measurements\n";
        preview << "Plots: precision, estimated voltage magnitudes and estimated voltage angles\n";
        preview << "Auto-open je ukljucen; model se otvara u dTwinu nakon konverzije.\n";
        preview << "Broj bus/faza zapisa: " << nBuses << "\n";

        ConversionResult res;
        res.ok = true;
        res.message = "MATPOWER case je konvertovan u dTwin WLS .dmodl i .vmodl fajlove sa Gaussovim sumom.";
        res.preview = preview.str();
        res.outputPath = outputPath;
        res.openInDTwin = true;
        report(onProgress, 1.0, "Gotovo");
        return res;
    }

    std::ofstream out(outputPath);
    if (!out)
        throw std::runtime_error("Ne mogu kreirati izlazni fajl.");

    out << std::setprecision(12);
    if (options.writeComments)
    {
        out << "# TSE complex-coordinate sparse vector generated from MATPOWER bus Vm/Va\n";
        out << "# Columns: row col real imag  // row maps to bus-phase state\n";
    }
    out << "complex tse_voltage_vector\n";
    const bool alreadyThreePhase = nBuses > 0 && (int) bus(0, busColPhase) != 0;
    const int nPhases = alreadyThreePhase ? nBuses : nBuses * 3;
    sparse::CmplxMatrixReleaser voltageVector(sparse::createCmplxMatrix(nPhases, 1, nPhases));
    out << nPhases << " 1 " << nPhases << "\n";

    std::ostringstream preview;
    preview << "MATPOWER bus -> trofazni kompleksni vektor\n";
    preview << "Broj bus/faza zapisa: " << nPhases << "\n";

    for (int i = 0; i < nBuses; ++i)
    {
        if (cancelRequested)
            throw std::runtime_error("Konverzija je prekinuta.");

        const int busID = (int) bus(i, busColID);
        const int phase = (int) bus(i, busColPhase);
        const double vm = bus(i, busColVM);
        const double vaDeg = bus(i, busColVA);

        if (alreadyThreePhase)
        {
            const int row = i + 1;
            const auto value = polarDeg(vm, vaDeg);
            voltageVector->addTriple1(row, 1, value);
            out << row << " 1 " << value.real() << " " << value.imag()
                << " # bus=" << busID << " phase=" << phaseChar(phase) << "\n";

            if (i < 8)
                preview << busLabel(busID, phase) << ": (" << value.real() << "," << value.imag() << ")\n";
        }
        else
        {
            const std::complex<double> va = polarDeg(vm, vaDeg);
            const std::complex<double> vb = polarDeg(vm, vaDeg - 120.0);
            const std::complex<double> vc = polarDeg(vm, vaDeg + 120.0);
            const std::complex<double> phases[3] = {va, vb, vc};

            for (int p = 0; p < 3; ++p)
            {
                const int row = i * 3 + p + 1;
                voltageVector->addTriple1(row, 1, phases[p]);
                out << row << " 1 " << phases[p].real() << " " << phases[p].imag()
                    << " # bus=" << busID << " phase=" << (p == 0 ? 'A' : (p == 1 ? 'B' : 'C')) << "\n";
            }

            if (i < 5)
            {
                preview << "bus " << busID << ": A=(" << va.real() << "," << va.imag() << ") "
                        << "B=(" << vb.real() << "," << vb.imag() << ") "
                        << "C=(" << vc.real() << "," << vc.imag() << ")\n";
            }
        }

        if (i % 25 == 0 || i + 1 == nBuses)
        {
            const double prog = 0.20 + 0.75 * (double) (i + 1) / (double) nBuses;
            report(onProgress, prog, "Konvertujem MATPOWER Vm/Va u kompleksne faze");
        }
    }

    ConversionResult res;
    res.ok = true;
    res.message = "MATPOWER case je konvertovan u trofazni kompleksni sparse vektor.";
    res.preview = preview.str();
    res.outputPath = outputPath;
    report(onProgress, 1.0, "Gotovo");
    return res;
}

bool readSparseHeader(const std::string& inputPath,
                      bool& isComplex,
                      std::string& kind,
                      int& rows,
                      int& cols,
                      int& nnz,
                      std::atomic_bool& cancelRequested,
                      const ProgressCallback& onProgress)
{
    std::ifstream in(inputPath);
    if (!in)
        throw std::runtime_error("Ne mogu otvoriti sparse .mtx fajl.");

    std::string line;
    int lineNo = 0;

    report(onProgress, 0.05, "Ucitavam sparse matricu");
    while (std::getline(in, line))
    {
        ++lineNo;
        line = trim(line);
        if (!line.empty() && line[0] != '#')
            break;
    }

    if (line.empty())
        throw std::runtime_error("Prazan sparse .mtx fajl.");

    {
        std::istringstream header(line);
        std::string domain;
        header >> domain >> kind;
        domain = lower(domain);
        if (domain == "real")
            isComplex = false;
        else if (domain == "complex")
            isComplex = true;
        else
            throw std::runtime_error("Nepoznat sparse domen. Ocekivao sam 'real' ili 'complex'.");
    }

    while (std::getline(in, line))
    {
        ++lineNo;
        line = trim(line);
        if (line.empty() || line[0] == '#')
            continue;
        std::istringstream dims(line);
        dims >> rows >> cols >> nnz;
        if (!dims || rows <= 0 || cols <= 0 || nnz < 0)
            throw std::runtime_error("Neispravna dimenzija sparse matrice.");
        return true;
    }
    return false;
}

void loadSparseMTX(const std::string& inputPath,
                   bool isComplex,
                   sparse::ICmplxMatrix& matrix,
                   std::ostream& out,
                   dense::CmplxMatrix& previewEntries,
                   int& previewCount,
                   std::atomic_bool& cancelRequested,
                   const ProgressCallback& onProgress)
{
    std::ifstream in(inputPath);
    if (!in)
        throw std::runtime_error("Ne mogu otvoriti sparse .mtx fajl.");

    std::string line;
    int lineNo = 0;
    bool headerSeen = false;
    bool dimsSeen = false;
    int readEntries = 0;
    auto preview = previewEntries.getManipulator();

    while (std::getline(in, line))
    {
        if (cancelRequested)
            throw std::runtime_error("Konverzija je prekinuta.");

        ++lineNo;
        line = trim(line);
        if (line.empty() || line[0] == '#')
            continue;

        if (!headerSeen)
        {
            headerSeen = true;
            continue;
        }
        if (!dimsSeen)
        {
            dimsSeen = true;
            continue;
        }

        std::istringstream rowStream(line);
        int row = 0;
        int col = 0;
        double real = 0.0;
        double imag = 0.0;
        rowStream >> row >> col >> real;
        if (isComplex)
            rowStream >> imag;
        if (!rowStream)
        {
            std::ostringstream err;
            err << "Neispravan sparse red " << lineNo << ".";
            throw std::runtime_error(err.str());
        }

        const td::cmplx value(real, imag);
        matrix.addTriple1(row, col, value);
        out << row << " " << col << " " << real << " " << imag << "\n";
        if (previewCount < (int) previewEntries.getNoOfRows())
        {
            preview(previewCount, 0) = td::cmplx((double) row, 0.0);
            preview(previewCount, 1) = td::cmplx((double) col, 0.0);
            preview(previewCount, 2) = value;
            ++previewCount;
        }

        ++readEntries;
        if (readEntries % 5000 == 0)
            report(onProgress, 0.20, "Citam sparse koordinate");
    }
}

ConversionResult convertSparse(const std::string& inputPath,
                               const std::string& outputPath,
                               const ConversionOptions& options,
                               std::atomic_bool& cancelRequested,
                               const ProgressCallback& onProgress)
{
    bool isComplex = false;
    std::string kind;
    int rows = 0;
    int cols = 0;
    int nnz = 0;
    if (!readSparseHeader(inputPath, isComplex, kind, rows, cols, nnz, cancelRequested, onProgress))
        throw std::runtime_error("Neispravna ili prazna sparse matrica.");

    sparse::CmplxMatrixReleaser matrix(sparse::createCmplxMatrix(rows, cols, nnz));
    dense::CmplxMatrix previewEntries(8, 3, nullptr, true);
    int previewCount = 0;

    std::ofstream out(outputPath);
    if (!out)
        throw std::runtime_error("Ne mogu kreirati izlazni fajl.");

    out << std::setprecision(12);
    if (options.writeComments)
        out << "# Complex sparse matrix generated by TSE converter plugin\n";
    out << "complex " << (kind.empty() ? "converted" : kind) << "\n";
    out << rows << " " << cols << " " << nnz << "\n";

    loadSparseMTX(inputPath, isComplex, matrix.ref(), out, previewEntries, previewCount, cancelRequested, onProgress);

    std::ostringstream preview;
    preview << "Sparse .mtx -> complex .mtx\n";
    preview << "Dimenzije: " << rows << "x" << cols << ", nnz=" << matrix->getNoOfNonZero() << "\n";
    auto previewData = previewEntries.getManipulator();
    for (int i = 0; i < previewCount; ++i)
    {
        const int row = (int) previewData(i, 0).real();
        const int col = (int) previewData(i, 1).real();
        const auto value = previewData(i, 2);
        preview << row << " " << col << " -> (" << value.real() << "," << value.imag() << ")\n";
    }

    ConversionResult res;
    res.ok = true;
    res.message = "Sparse matrica je zapisana u kompleksnom koordinatnom formatu.";
    res.preview = preview.str();
    res.outputPath = outputPath;
    report(onProgress, 1.0, "Gotovo");
    return res;
}

InputKind detectKind(const std::string& path, InputKind requested)
{
    if (requested != InputKind::Auto)
        return requested;
    if (endsWithCI(path, ".m"))
        return InputKind::Matpower;
    if (endsWithCI(path, ".mtx") || endsWithCI(path, ".txt"))
        return InputKind::SparseMTX;
    return InputKind::Matpower;
}
}

ConversionResult convertToComplexCoordinates(const std::string& inputPath,
                                             const std::string& outputPath,
                                             const ConversionOptions& options,
                                             std::atomic_bool& cancelRequested,
                                             const ProgressCallback& onProgress)
{
    try
    {
        if (inputPath.empty())
            throw std::runtime_error("Nije odabran ulazni fajl.");
        if (outputPath.empty())
            throw std::runtime_error("Nije odabran izlazni fajl.");

        switch (detectKind(inputPath, options.inputKind))
        {
            case InputKind::Matpower:
                return convertMatpower(inputPath, outputPath, options, cancelRequested, onProgress);
            case InputKind::SparseMTX:
                return convertSparse(inputPath, outputPath, options, cancelRequested, onProgress);
            case InputKind::Auto:
            default:
                break;
        }
        throw std::runtime_error("Nepoznat tip ulaznog fajla.");
    }
    catch (const std::exception& e)
    {
        ConversionResult res;
        res.ok = false;
        res.message = e.what();
        res.outputPath = outputPath;
        report(onProgress, 1.0, "Greska");
        return res;
    }
}


