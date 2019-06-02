// Copyright (C) 2019  Rhys Mainwaring
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program.  If not, see <https://www.gnu.org/licenses/>.

#include "asv_wave_sim_gazebo_plugins/WaveSimulationSimple.hh"
#include "asv_wave_sim_gazebo_plugins/Physics.hh"

#include <vector>

namespace asv
{

  ///////////////////////////////////////////////////////////////////////////////
  // WaveSimulationSimpleImpl

  class WaveSimulationSimpleImpl
  {
    public: ~WaveSimulationSimpleImpl();

    public: WaveSimulationSimpleImpl(
      int _N,
      double _L);

    public: void SetWindVelocity(double _ux, double _uy);

    public: void SetParameters(double _amplitude, double _period);

    public: void SetTime(double _time);

    public: void ComputeHeights(
      std::vector<double>& _heights);

    public: void ComputeHeightDerivatives(
      std::vector<double>& _dhdx,
      std::vector<double>& _dhdy);

    public: void ComputeDisplacements(
      std::vector<double>& _sx,
      std::vector<double>& _sy);

    public: void ComputeDisplacementDerivatives(
      std::vector<double>& _dsxdx,
      std::vector<double>& _dsydy,
      std::vector<double>& _dsxdy);

    public: void ComputeDisplacementsAndDerivatives(
      std::vector<double>& _h,
      std::vector<double>& _sx,
      std::vector<double>& _sy,
      std::vector<double>& _dhdx,
      std::vector<double>& _dhdy,
      std::vector<double>& _dsxdx,
      std::vector<double>& _dsydy,
      std::vector<double>& _dsxdy);

    private: void ComputeBaseAmplitudes();

    private: void ComputeCurrentAmplitudes(double _time);
    
    private: int N;
    private: int N2;
    private: int NOver2;
    private: double L;
    private: double amplitude;
    private: double period;
    private: double time;
  };

  WaveSimulationSimpleImpl::~WaveSimulationSimpleImpl()
  {
  }

  WaveSimulationSimpleImpl::WaveSimulationSimpleImpl(
    int _N,
    double _L) :
    N(_N),
    N2(_N * _N),
    NOver2(_N / 2),
    L(_L),
    amplitude(2.5),
    period(8.0),
    time(0.0)
  {
  }

  void WaveSimulationSimpleImpl::SetWindVelocity(double _ux, double _uy)
  {
    // @TODO NO IMPLEMENTATION
  }

  void WaveSimulationSimpleImpl::SetParameters(double _amplitude, double _period)
  {
    this->amplitude = _amplitude;
    this->period = _period;
  }

  void WaveSimulationSimpleImpl::SetTime(double _time)
  {
    this->time = _time;
  }

  void WaveSimulationSimpleImpl::ComputeHeights(
    std::vector<double>& _heights)
  {
    // Derived wave properties
    double omega = 2.0 * M_PI / this->period;
    double k = Physics::DeepWaterDispersionToWavenumber(omega);

    // Wave update
    double LOverN = this->L / this->N;
    double LOver2 = this->L / 2.0;
    for (size_t iy=0; iy<this->N; ++iy)
    {
      // Regular grid
      double vy = iy * LOverN - LOver2;

      for (size_t ix=0; ix<this->N; ++ix)
      {
        // Row major index
        size_t idx = iy * this->N + ix;

        // Regular grid
        double vx = ix * LOverN - LOver2;

        // Single wave
        double angle  = k * vx - omega * time;
        double c = std::cos(angle);
        double h = this->amplitude * c;

        _heights[idx] = h;
      }
    }
  }

  void WaveSimulationSimpleImpl::ComputeHeightDerivatives(
    std::vector<double>& _dhdx,
    std::vector<double>& _dhdy)
  {
    // Derived wave properties
    double omega = 2.0 * M_PI / this->period;
    double k = Physics::DeepWaterDispersionToWavenumber(omega);

    // Wave update
    double LOverN = this->L / this->N;
    double LOver2 = this->L / 2.0;
    for (size_t iy=0; iy<this->N; ++iy)
    {
      // Regular grid
      double vy = iy * LOverN - LOver2;

      for (size_t ix=0; ix<this->N; ++ix)
      {
        // Row major index
        size_t idx = iy * this->N + ix;

        // Regular grid
        double vx = ix * LOverN - LOver2;
        double vy = iy * LOverN - LOver2;

        // Single wave
        double angle  = k * vx - omega * time;
        double dangle = k;

        double s = std::sin(angle);
        double dhdx = - dangle * this->amplitude * s;
        double dhdy = 0.0;

        _dhdx[idx] = dhdx;
        _dhdy[idx] = dhdy;
      }
    }
  }

  void WaveSimulationSimpleImpl::ComputeDisplacements(
    std::vector<double>& _sx,
    std::vector<double>& _sy)
  {
    // No xy-displacement
  }

  void WaveSimulationSimpleImpl::ComputeDisplacementDerivatives(
    std::vector<double>& _dsxdx,
    std::vector<double>& _dsydy,
    std::vector<double>& _dsxdy)
  {
    // No xy-displacement
  }

  void WaveSimulationSimpleImpl::ComputeDisplacementsAndDerivatives(
    std::vector<double>& _h,
    std::vector<double>& _sx,
    std::vector<double>& _sy,
    std::vector<double>& _dhdx,
    std::vector<double>& _dhdy,
    std::vector<double>& _dsxdx,
    std::vector<double>& _dsydy,
    std::vector<double>& _dsxdy)
  {
    // Derived wave properties
    double omega = 2.0 * M_PI / this->period;
    double k = Physics::DeepWaterDispersionToWavenumber(omega);

    // Wave update
    double LOverN = this->L / this->N;
    double LOver2 = this->L / 2.0;
    for (size_t iy=0; iy<this->N; ++iy)
    {
      // double vy = iy * LOverN - LOver2;

      for (size_t ix=0; ix<this->N; ++ix)
      {
        // Row major index
        size_t idx = iy * this->N + ix;

        // Regular grid
        double vx = ix * LOverN - LOver2;

        // Single wave
        double angle  = k * vx - omega * time;
        double dangle = k;
        double c = std::cos(angle);
        double s = std::sin(angle);
        double h = this->amplitude * c;
        double dhdx = - dangle * this->amplitude * s;
        double dhdy = 0.0;

        _h[idx] = h;
        _dhdx[idx] = dhdx;
        _dhdy[idx] = dhdy;
      }
    }

  }

  ///////////////////////////////////////////////////////////////////////////////
  // WaveSimulationSimple

  WaveSimulationSimple::~WaveSimulationSimple()
  {
  }

  WaveSimulationSimple::WaveSimulationSimple(
    int _N,
    double _L) :
    impl(new WaveSimulationSimpleImpl(_N, _L))
  {
  }

  void WaveSimulationSimple::SetWindVelocity(double _ux, double _uy)
  {
  }

  void WaveSimulationSimple::SetParameters(double _amplitude, double _period)
  {
    impl->SetParameters(_amplitude, _period);
  }

  void WaveSimulationSimple::SetTime(double _time)
  {
    impl->SetTime(_time);
  }

  void WaveSimulationSimple::ComputeHeights(
    std::vector<double>& _h)
  {
    impl->ComputeHeights(_h);    
  }

  void WaveSimulationSimple::ComputeHeightDerivatives(
    std::vector<double>& _dhdx,
    std::vector<double>& _dhdy)
  {
    impl->ComputeHeightDerivatives(_dhdx, _dhdy);  
  }

  void WaveSimulationSimple::ComputeDisplacements(
    std::vector<double>& _sx,
    std::vector<double>& _sy)
  {
    impl->ComputeDisplacements(_sx, _sy);    
  }

  void WaveSimulationSimple::ComputeDisplacementDerivatives(
    std::vector<double>& _dsxdx,
    std::vector<double>& _dsydy,
    std::vector<double>& _dsxdy)
  {
    impl->ComputeDisplacementDerivatives(_dsxdx, _dsydy, _dsxdy);      
  }

  void WaveSimulationSimple::ComputeDisplacementsAndDerivatives(
    std::vector<double>& _h,
    std::vector<double>& _sx,
    std::vector<double>& _sy,
    std::vector<double>& _dhdx,
    std::vector<double>& _dhdy,
    std::vector<double>& _dsxdx,
    std::vector<double>& _dsydy,
    std::vector<double>& _dsxdy)
  {
    impl->ComputeDisplacementsAndDerivatives(_h, _dhdx, _dhdy, _sx, _sy, _dsxdx, _dsydy, _dsxdy);    
  }

}
