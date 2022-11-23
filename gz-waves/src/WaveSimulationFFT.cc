// Copyright (C) 2022  Rhys Mainwaring
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


// The non-vectorised time-dependent update step labelled 'non-vectorised reference version'
// in WaveSimulationFFTImpl.ComputeCurrentAmplitudesReference is based on the Curtis Mobley's
// IDL code cgAnimate_2D_SeaSurface.py

//***************************************************************************************************
//* This code is copyright (c) 2016 by Curtis D. Mobley.                                            *
//* Permission is hereby given to reproduce and use this code for non-commercial academic research, *
//* provided that the user suitably acknowledges Curtis D. Mobley in any presentations, reports,    *
//* publications, or other works that make use of the code or its output.  Depending on the extent  *
//* of use of the code or its outputs, suitable acknowledgement can range from a footnote to offer  *
//* of coauthorship.  Further questions can be directed to curtis.mobley@sequoiasci.com.            *
//***************************************************************************************************

#include "gz/waves/WaveSimulationFFT.hh"

#include <complex>
#include <random>

#include <Eigen/Dense>

#include <fftw3.h>

#include <gz/common.hh>

#include "gz/waves/WaveSpectrum.hh"
#include "gz/waves/WaveSpreadingFunction.hh"
#include "WaveSimulationFFTImpl.hh"

namespace gz
{
namespace waves
{
  //////////////////////////////////////////////////
  WaveSimulationFFTImpl::~WaveSimulationFFTImpl()
  {
    DestroyFFTWPlans();
  }

  //////////////////////////////////////////////////
  WaveSimulationFFTImpl::WaveSimulationFFTImpl(
    double lx, double ly, int nx, int ny) :
    nx_(nx),
    ny_(ny),
    lx_(lx),
    ly_(ly),
    lambda_(0.6)
  {
    ComputeBaseAmplitudes();
    CreateFFTWPlans();
  }

  //////////////////////////////////////////////////
  void WaveSimulationFFTImpl::SetUseVectorised(bool value)
  {
    use_vectorised_ = value;
    ComputeBaseAmplitudes();
  }

  //////////////////////////////////////////////////
  void WaveSimulationFFTImpl::SetLambda(double value)
  {
    lambda_ = value;
    ComputeBaseAmplitudes();
  }

  //////////////////////////////////////////////////
  void WaveSimulationFFTImpl::SetWindVelocity(double ux, double uy)
  {
    // Update wind velocity and recompute base amplitudes.
    u10_ = sqrt(ux*ux + uy *uy);
    phi10_ = atan2(uy, ux);

    ComputeBaseAmplitudes();
  }

  //////////////////////////////////////////////////
  void WaveSimulationFFTImpl::SetTime(double time)
  {
    ComputeCurrentAmplitudes(time);
  }

  //////////////////////////////////////////////////
  void WaveSimulationFFTImpl::ComputeElevation(
    Eigen::Ref<Eigen::MatrixXd> h)
  {
    // run the FFT
    fftw_execute(fft_plan0_);

    // change from row to column major storage
    size_t n2 = nx_ * ny_;
    h = fft_out0_.reshaped<Eigen::ColMajor>(n2, 1).real();
  }

  //////////////////////////////////////////////////
  void WaveSimulationFFTImpl::ComputeElevationDerivatives(
    Eigen::Ref<Eigen::MatrixXd> dhdx,
    Eigen::Ref<Eigen::MatrixXd> dhdy)
  {
    // run the FFTs
    fftw_execute(fft_plan1_);
    fftw_execute(fft_plan2_);

    // change from row to column major storage
    size_t n2 = nx_ * ny_;
    dhdy = fft_out1_.reshaped<Eigen::ColMajor>(n2, 1).real();
    dhdx = fft_out2_.reshaped<Eigen::ColMajor>(n2, 1).real();
  }

  //////////////////////////////////////////////////
  void WaveSimulationFFTImpl::ComputeDisplacements(
    Eigen::Ref<Eigen::MatrixXd> sx,
    Eigen::Ref<Eigen::MatrixXd> sy)
  {
    // run the FFTs
    fftw_execute(fft_plan3_);
    fftw_execute(fft_plan4_);

    // change from row to column major storage
    size_t n2 = nx_ * ny_;
    sy = fft_out3_.reshaped<Eigen::ColMajor>(n2, 1).real() * lambda_ * -1.0;
    sx = fft_out4_.reshaped<Eigen::ColMajor>(n2, 1).real() * lambda_ * -1.0;
  }

  //////////////////////////////////////////////////
  void WaveSimulationFFTImpl::ComputeDisplacementsDerivatives(
    Eigen::Ref<Eigen::MatrixXd> dsxdx,
    Eigen::Ref<Eigen::MatrixXd> dsydy,
    Eigen::Ref<Eigen::MatrixXd> dsxdy)
  {
    // run the FFTs
    fftw_execute(fft_plan5_);
    fftw_execute(fft_plan6_);
    fftw_execute(fft_plan7_);

    // change from row to column major storage
    size_t n2 = nx_ * ny_;
    dsydy = fft_out5_.reshaped<Eigen::ColMajor>(n2, 1).real() * lambda_ * -1.0;
    dsxdx = fft_out6_.reshaped<Eigen::ColMajor>(n2, 1).real() * lambda_ * -1.0;
    dsxdy = fft_out7_.reshaped<Eigen::ColMajor>(n2, 1).real() * lambda_ *  1.0;
  }

  //////////////////////////////////////////////////
  void WaveSimulationFFTImpl::ComputeBaseAmplitudes()
  {
    if (use_vectorised_)
      ComputeBaseAmplitudesVectorised();
    else
      ComputeBaseAmplitudesNonVectorised();
  }

  //////////////////////////////////////////////////
  void WaveSimulationFFTImpl::ComputeCurrentAmplitudes(double time)
  {
    if (use_vectorised_)
      ComputeCurrentAmplitudesVectorised(time);
    else
      ComputeCurrentAmplitudesNonVectorised(time);
  }

  //////////////////////////////////////////////////
  void WaveSimulationFFTImpl::ComputeBaseAmplitudesNonVectorised()
  {
    InitFFTCoeffStorage();
    InitWaveNumbers();

    // initialise arrays
    size_t n2 = nx_ * ny_;
    if (cap_psi_2s_root_.size() == 0)
    {
      cap_psi_2s_root_  = Eigen::VectorXd::Zero(n2);
      rho_              = Eigen::VectorXd::Zero(n2);
      sigma_            = Eigen::VectorXd::Zero(n2);
      omega_k_          = Eigen::VectorXd::Zero(n2);
    }

    // spectrum and spreading functions
    gz::waves::ECKVWaveSpectrum spectrum;
    spectrum.SetGravity(gravity_);
    spectrum.SetU10(u10_);
    spectrum.SetCapOmegaC(cap_omega_c_);

    // standing waves - symmetric spreading function
    gz::waves::ECKVSpreadingFunction spreadingFn1;
    spreadingFn1.SetGravity(gravity_);
    spreadingFn1.SetU10(u10_);
    spreadingFn1.SetCapOmegaC(cap_omega_c_);

    // travelling waves - asymmetric spreading function
    gz::waves::Cos2sSpreadingFunction spreadingFn2;
    spreadingFn2.SetSpread(s_param_);

    // continuous two-sided elevation variance spectrum
    Eigen::VectorXd cap_psi_2s_math = Eigen::VectorXd::Zero(nx_ * ny_);

    // calculate spectrum in math-order (not vectorised)
    for (int ikx = 0; ikx < nx_; ++ikx)
    {
      // kx: fftfreq and ifftshift
      const double kx = (ikx - nx_/2) * kx_f_;
      const double kx2 = kx*kx;

      for (int iky = 0; iky < ny_; ++iky)
      {
        // ky: fftfreq and ifftshift
        const double ky = (iky - ny_/2) * ky_f_;
        const double ky2 = ky*ky;
        
        const double k = sqrt(kx2 + ky2);
        const double phi = atan2(ky, kx);

        // index for flattened array
        int idx = ikx * ny_ + iky;

        if (k == 0.0)
        {
          cap_psi_2s_math[idx] = 0.0;
        }
        else
        {
          double cap_psi = 0.0;
          if (use_symmetric_spreading_fn_)
          {
            // standing waves - symmetric spreading function
            cap_psi = spreadingFn1.Evaluate(phi, phi10_, k);
          }
          else
          {
            // travelling waves - asymmetric spreading function
            cap_psi = spreadingFn2.Evaluate(phi, phi10_, k);
          }
          double cap_s = spectrum.Evaluate(k);
          cap_psi_2s_math[idx] = cap_s * cap_psi / k;
        }
      }
    }

    // convert to fft-order
    Eigen::VectorXd cap_psi_2s_fft = Eigen::VectorXd::Zero(nx_ * ny_);
    for (int ikx = 0; ikx < nx_; ++ikx)
    {
      int ikx_fft = (ikx + nx_/2) % nx_;
      for (int iky = 0; iky < ny_; ++iky)
      {
        int iky_fft = (iky + ny_/2) % ny_;

        // index for flattened array
        int idx = ikx * ny_ + iky;
        int idx_fft = ikx_fft * ny_ + iky_fft;

        cap_psi_2s_fft[idx_fft] = cap_psi_2s_math[idx];
      }
    }

    // square-root of two-sided discrete elevation variance spectrum
    double cap_psi_norm = 0.5;
    double delta_kx = kx_f_;
    double delta_ky = ky_f_;
    // double c1 = cap_psi_norm * sqrt(delta_kx * delta_ky);

    // iid random normals for real and imaginary parts of the amplitudes
    auto seed = std::default_random_engine::default_seed;
    std::default_random_engine generator(seed);
    std::normal_distribution<double> distribution(0.0, 1.0);

    for (int i = 0; i < n2; ++i)
    {
      // cap_psi_2s_root[i] = c1 * sqrt(cap_psi_2s_fft[i]);
      cap_psi_2s_root_[i] =
          cap_psi_norm * sqrt(cap_psi_2s_fft[i] * delta_kx * delta_ky);

      rho_[i] = distribution(generator);
      sigma_[i] = distribution(generator);
    }

    // angular temporal frequency for time-dependent (from dispersion)
    for (int ikx = 0; ikx < nx_; ++ikx)
    {
      double kx = kx_fft_[ikx];
      double kx2 = kx*kx;
      for (int iky = 0; iky < ny_; ++iky)
      {
        double ky = ky_fft_[iky];
        double ky2 = ky*ky;
        double k = sqrt(kx2 + ky2);

        // index for flattened array
        int idx = ikx * ny_ + iky;
        omega_k_[idx] = sqrt(gravity_ * k);
      }
    }
  }

  //////////////////////////////////////////////////
  void WaveSimulationFFTImpl::ComputeCurrentAmplitudesNonVectorised(
      double time)
  {
    // alias
    const Eigen::Ref<const Eigen::VectorXd>& r = rho_;
    const Eigen::Ref<const Eigen::VectorXd>& s = sigma_;
    const Eigen::Ref<const Eigen::VectorXd>& psi_root = cap_psi_2s_root_;

    // time update
    Eigen::VectorXd cos_omega_k = Eigen::VectorXd::Zero(nx_ * ny_);
    Eigen::VectorXd sin_omega_k = Eigen::VectorXd::Zero(nx_ * ny_);
    for (int ikx = 0; ikx < nx_; ++ikx)
    {
      for (int iky = 0; iky < ny_; ++iky)
      {
        // index for flattened array
        int idx = ikx * ny_ + iky;

        double omega_t = omega_k_[idx] * time;
        cos_omega_k(idx) = cos(omega_t);
        sin_omega_k(idx) = sin(omega_t);
      }
    }

    // flattened index version
    Eigen::VectorXcd zhat = Eigen::VectorXcd::Zero(nx_ * ny_);
    for (int ikx = 1; ikx < nx_; ++ikx)
    {
      for (int iky = 1; iky < ny_; ++iky)
      {
        // index for flattened array (ikx, iky)
        int idx = ikx * ny_ + iky;

        // index for conjugate (nx_-ikx, ny_-iky)
        int cdx = (nx_-ikx) * ny_ + (ny_-iky);

        zhat[idx] = complex(
            + ( r(idx) * psi_root(idx) + r(cdx) * psi_root(cdx) ) * cos_omega_k(idx)
            + ( s(idx) * psi_root(idx) + s(cdx) * psi_root(cdx) ) * sin_omega_k(idx),
            - ( r(idx) * psi_root(idx) - r(cdx) * psi_root(cdx) ) * sin_omega_k(idx)
            + ( s(idx) * psi_root(idx) - s(cdx) * psi_root(cdx) ) * cos_omega_k(idx));
      }
    }

    for (int iky = 1; iky < ny_/2+1; ++iky)
    {
      int ikx = 0;

      // index for flattened array (ikx, iky)
      int idx = ikx * ny_ + iky;

      // index for conjugate (ikx, ny_-iky)
      int cdx = ikx * ny_ + (ny_-iky);

      zhat[idx] = complex(
          + ( r(idx) * psi_root(idx) + r(cdx) * psi_root(cdx) ) * cos_omega_k(idx)
          + ( s(idx) * psi_root(idx) + s(cdx) * psi_root(cdx) ) * sin_omega_k(idx),
          - ( r(idx) * psi_root(idx) - r(cdx) * psi_root(cdx) ) * sin_omega_k(idx)
          + ( s(idx) * psi_root(idx) - s(cdx) * psi_root(cdx) ) * cos_omega_k(idx));
      zhat[cdx] = std::conj(zhat[idx]);
    }

    for (int ikx = 1; ikx < nx_/2+1; ++ikx)
    {
      int iky = 0;

      // index for flattened array (ikx, iky)
      int idx = ikx * ny_ + iky;

      // index for conjugate (nx_-ikx, iky)
      int cdx = (nx_-ikx) * ny_ + iky;

      zhat[idx] = complex(
          + ( r(idx) * psi_root(idx) + r(cdx) * psi_root(cdx) ) * cos_omega_k(idx)
          + ( s(idx) * psi_root(idx) + s(cdx) * psi_root(cdx) ) * sin_omega_k(idx),
          - ( r(idx) * psi_root(idx) - r(cdx) * psi_root(cdx) ) * sin_omega_k(idx)
          + ( s(idx) * psi_root(idx) - s(cdx) * psi_root(cdx) ) * cos_omega_k(idx));
      zhat[cdx] = std::conj(zhat[idx]);
    }

    zhat[0] = complex(0.0, 0.0);

    // write into fft_h_, fft_h_ikx_, fft_h_iky_, etc.
    const complex iunit(0.0, 1.0);
    const complex czero(0.0, 0.0);
    for (int ikx = 0; ikx < nx_; ++ikx)
    {
      double kx = kx_fft_[ikx];
      double kx2 = kx*kx;
      for (int iky = 0; iky < ny_; ++iky)
      {
        double ky = ky_fft_[iky];
        double ky2 = ky*ky;
        double k = sqrt(kx2 + ky2);
        double ook = 1.0 / k;

        // index for flattened arrays
        int idx = ikx * ny_ + iky;

        complex h  = zhat[idx];
        complex hi = h * iunit;
        complex hok = h * ook;
        complex hiok = hi * ook;

        // height (amplitude)
        fft_h_(ikx, iky) = h;

        // height derivatives
        complex hikx = hi * kx;
        complex hiky = hi * ky;

        fft_h_ikx_(ikx, iky) = hi * kx;
        fft_h_iky_(ikx, iky) = hi * ky;

        // displacement and derivatives
        if (std::abs(k) < 1.0E-8)
        {          
          fft_sx_(ikx, iky)     = czero;
          fft_sy_(ikx, iky)     = czero;
          fft_h_kxkx_(ikx, iky) = czero;
          fft_h_kyky_(ikx, iky) = czero;
          fft_h_kxky_(ikx, iky) = czero;
        }
        else
        {
          complex dx  = - hiok * kx;
          complex dy  = - hiok * ky;
          complex hkxkx = hok * kx2;
          complex hkyky = hok * ky2;
          complex hkxky = hok * kx * ky;
          
          fft_sx_(ikx, iky)     = dx;
          fft_sy_(ikx, iky)     = dy;
          fft_h_kxkx_(ikx, iky) = hkxkx;
          fft_h_kyky_(ikx, iky) = hkyky;
          fft_h_kxky_(ikx, iky) = hkxky;
        }
      }
    }
  }

  //////////////////////////////////////////////////
  void WaveSimulationFFTImpl::ComputeBaseAmplitudesVectorised()
  {
    // initialise storage
    InitFFTCoeffStorage();
    InitWaveNumbers();

    // initialise arrays
    if (cap_psi_2s_root_vec_.size() == 0)
    {
      cap_psi_2s_root_vec_ = Eigen::MatrixXd::Zero(nx_, ny_);
      rho_vec_             = Eigen::MatrixXd::Zero(nx_, ny_);
      sigma_vec_           = Eigen::MatrixXd::Zero(nx_, ny_);
      omega_k_vec_         = Eigen::MatrixXd::Zero(nx_, ny_);
    }

    // spectrum and spreading functions
    gz::waves::ECKVWaveSpectrum spectrum;
    spectrum.SetGravity(gravity_);
    spectrum.SetU10(u10_);
    spectrum.SetCapOmegaC(cap_omega_c_);

    // standing waves - symmetric spreading function
    gz::waves::ECKVSpreadingFunction spreadingFn1;
    spreadingFn1.SetGravity(gravity_);
    spreadingFn1.SetU10(u10_);
    spreadingFn1.SetCapOmegaC(cap_omega_c_);

    // travelling waves - asymmetric spreading function
    gz::waves::Cos2sSpreadingFunction spreadingFn2;
    spreadingFn2.SetSpread(s_param_);

    // broadcast (fft) wavenumbers to arrays (aka meshgrid)
    Eigen::MatrixXd kx = Eigen::MatrixXd::Zero(nx_, ny_);
    Eigen::MatrixXd ky = Eigen::MatrixXd::Zero(nx_, ny_);
    kx.colwise() += kx_fft_;
    ky.rowwise() += ky_fft_.transpose();

    // wavenumber and wave angle arrays
    Eigen::MatrixXd kx2 = Eigen::pow(kx.array(), 2.0);
    Eigen::MatrixXd ky2 = Eigen::pow(ky.array(), 2.0);
    Eigen::MatrixXd k   = Eigen::sqrt(kx2.array() + ky2.array());
    Eigen::MatrixXd theta = ky.binaryExpr(
        kx, [] (double y, double x) { return std::atan2(y, x);}
    );

    // evaluate spectrum
    Eigen::MatrixXd cap_s = Eigen::MatrixXd::Zero(nx_, ny_);
    spectrum.Evaluate(cap_s, k);

    Eigen::MatrixXd cap_psi = Eigen::MatrixXd::Zero(nx_, ny_);
    if (use_symmetric_spreading_fn_)
      spreadingFn1.Evaluate(cap_psi, theta, phi10_, k);
    else
      spreadingFn2.Evaluate(cap_psi, theta, phi10_, k);

    // array k1 has no zero elements
    Eigen::MatrixXd k1 = (k.array() == 0).select(
        Eigen::MatrixXd::Ones(nx_, ny_), k);

    // evaluate continuous two-sided elevation variance spectrum for k1 != 0
    Eigen::MatrixXd cap_psi_2s_fft =
        cap_s.array() * cap_psi.array() / k1.array();

    // apply filter for k == 0
    cap_psi_2s_fft = (k.array() == 0).select(
        Eigen::MatrixXd::Zero(nx_, ny_), cap_psi_2s_fft);

    // square-root of two-sided discrete elevation variance spectrum
    double cap_psi_norm = 0.5;
    double delta_kx = kx_f_;
    double delta_ky = ky_f_;
    cap_psi_2s_root_vec_ = cap_psi_norm * Eigen::sqrt(
        cap_psi_2s_fft.array() * delta_kx * delta_ky);

    /// \note vectorising the initialisation of rho and sigma will
    ///       alter the order, and break the cross checks.
    // iid random normals for real and imaginary parts of the amplitudes
    auto seed = std::default_random_engine::default_seed;
    std::default_random_engine generator(seed);
    std::normal_distribution<double> distribution(0.0, 1.0);
    for (int ikx = 0; ikx < nx_; ++ikx)
    {
      for (int iky = 0; iky < ny_; ++iky)
      {
        rho_vec_(ikx, iky) = distribution(generator);
        sigma_vec_(ikx, iky) = distribution(generator);
      }
    }

    // angular temporal frequency for time-dependent (from dispersion)
    omega_k_vec_ = Eigen::sqrt(gravity_ * k.array());

    // calculate zhat0
    const Eigen::Ref<const Eigen::MatrixXd>& r = rho_vec_;
    const Eigen::Ref<const Eigen::MatrixXd>& s = sigma_vec_;
    const Eigen::Ref<const Eigen::MatrixXd>& psi_root = cap_psi_2s_root_vec_;

    zhat0_rc_ = Eigen::MatrixXd::Zero(nx_, ny_);
    zhat0_rs_ = Eigen::MatrixXd::Zero(nx_, ny_);
    zhat0_ic_ = Eigen::MatrixXd::Zero(nx_, ny_);
    zhat0_is_ = Eigen::MatrixXd::Zero(nx_, ny_);
    for (int ikx = 1; ikx < nx_; ++ikx)
    {
      for (int iky = 1; iky < ny_; ++iky)
      {
        zhat0_rc_(ikx, iky) = + ( r(ikx, iky) * psi_root(ikx, iky) + r(nx_-ikx, ny_-iky) * psi_root(nx_-ikx, ny_-iky) ); 
        zhat0_rs_(ikx, iky) = + ( s(ikx, iky) * psi_root(ikx, iky) + s(nx_-ikx, ny_-iky) * psi_root(nx_-ikx, ny_-iky) ); 
        zhat0_is_(ikx, iky) = - ( r(ikx, iky) * psi_root(ikx, iky) - r(nx_-ikx, ny_-iky) * psi_root(nx_-ikx, ny_-iky) );
        zhat0_ic_(ikx, iky) = + ( s(ikx, iky) * psi_root(ikx, iky) - s(nx_-ikx, ny_-iky) * psi_root(nx_-ikx, ny_-iky) ); 
      }
    }

    for (int iky = 1; iky < ny_/2+1; ++iky)
    {
      int ikx = 0;
      zhat0_rc_(ikx, iky) = + ( r(ikx, iky) * psi_root(ikx, iky) + r(ikx, ny_-iky) * psi_root(ikx, ny_-iky) );
      zhat0_rs_(ikx, iky) = + ( s(ikx, iky) * psi_root(ikx, iky) + s(ikx, ny_-iky) * psi_root(ikx, ny_-iky) );
      zhat0_is_(ikx, iky) = - ( r(ikx, iky) * psi_root(ikx, iky) - r(ikx, ny_-iky) * psi_root(ikx, ny_-iky) );
      zhat0_ic_(ikx, iky) = + ( s(ikx, iky) * psi_root(ikx, iky) - s(ikx, ny_-iky) * psi_root(ikx, ny_-iky) );
    }

    for (int ikx = 1; ikx < nx_/2+1; ++ikx)
    {
      int iky = 0;
      zhat0_rc_(ikx, iky) = + ( r(ikx, iky) * psi_root(ikx, iky) + r(nx_-ikx, iky) * psi_root(nx_-ikx, iky) );
      zhat0_rs_(ikx, iky) = + ( s(ikx, iky) * psi_root(ikx, iky) + s(nx_-ikx, iky) * psi_root(nx_-ikx, iky) );
      zhat0_is_(ikx, iky) = - ( r(ikx, iky) * psi_root(ikx, iky) - r(nx_-ikx, iky) * psi_root(nx_-ikx, iky) );
      zhat0_ic_(ikx, iky) = + ( s(ikx, iky) * psi_root(ikx, iky) - s(nx_-ikx, iky) * psi_root(nx_-ikx, iky) );
    }
  }

#define VECTORISE_ZHAT_CALCS 0

  //////////////////////////////////////////////////
  void WaveSimulationFFTImpl::ComputeCurrentAmplitudesVectorised(
      double time)
  {
    // // time update
    Eigen::MatrixXd wt = omega_k_vec_.array() * time;
    Eigen::MatrixXd cos_omega_k = Eigen::cos(wt.array());
    Eigen::MatrixXd sin_omega_k = Eigen::sin(wt.array());

    // non-vectorised reference version
    Eigen::MatrixXcdRowMajor zhat = Eigen::MatrixXcd::Zero(nx_, ny_);
    for (int ikx = 1; ikx < nx_; ++ikx)
    {
      for (int iky = 1; iky < ny_; ++iky)
      {
        zhat(ikx, iky) = complex(
            + zhat0_rc_(ikx, iky) * cos_omega_k(ikx, iky)
            + zhat0_rs_(ikx, iky) * sin_omega_k(ikx, iky),
            + zhat0_is_(ikx, iky) * sin_omega_k(ikx, iky)
            + zhat0_ic_(ikx, iky) * cos_omega_k(ikx, iky));
      }
    }

    for (int iky = 1; iky < ny_/2+1; ++iky)
    {
      int ikx = 0;
      zhat(ikx, iky) = complex(
          + zhat0_rc_(ikx, iky) * cos_omega_k(ikx, iky)
          + zhat0_rs_(ikx, iky) * sin_omega_k(ikx, iky),
          + zhat0_is_(ikx, iky) * sin_omega_k(ikx, iky)
          + zhat0_ic_(ikx, iky) * cos_omega_k(ikx, iky));
      zhat(ikx, ny_-iky) = std::conj(zhat(ikx, iky));
    }

    for (int ikx = 1; ikx < nx_/2+1; ++ikx)
    {
      int iky = 0;
      zhat(ikx, iky) = complex(
          + zhat0_rc_(ikx, iky) * cos_omega_k(ikx, iky)
          + zhat0_rs_(ikx, iky) * sin_omega_k(ikx, iky),
          + zhat0_is_(ikx, iky) * sin_omega_k(ikx, iky)
          + zhat0_ic_(ikx, iky) * cos_omega_k(ikx, iky));
      zhat(nx_-ikx, iky) = std::conj(zhat(ikx, iky));
    }

    zhat(0, 0) = complex(0.0, 0.0);

    // write into fft_h_, fft_h_ikx_, fft_h_iky_, etc.
    const complex iunit(0.0, 1.0);
    const complex czero(0.0, 0.0);

/// \note array version is not faster than the loop.
#if VECTORISE_ZHAT_CALCS
    { // vectorised version: note: ook_ evaluates to zero when abs(k) < 1.0E-8
      fft_h_      = zhat; //h
      fft_h_ikx_  = zhat.array() * iunit * kx_.array(); //hikx
      fft_h_iky_  = zhat.array() * iunit * ky_.array(); //hiky
      fft_sx_     = zhat.array() * iunit * ook_.array() * kx_.array() * -1; //dx
      fft_sy_     = zhat.array() * iunit * ook_.array() * ky_.array() * -1; //dy
      fft_h_kxkx_ = zhat.array() * ook_.array() * kx2_.array(); //hkxkx
      fft_h_kyky_ = zhat.array() * ook_.array() * ky2_.array(); //hkyky
      fft_h_kxky_ = zhat.array() * ook_.array() * kx_.array() * ky_.array(); //hkxky
    }
#else
    // loop version
    for (int ikx = 0; ikx < nx_; ++ikx)
    {
      double kx = kx_fft_[ikx];
      double kx2 = kx*kx;
      for (int iky = 0; iky < ny_; ++iky)
      {
        double ky = ky_fft_[iky];
        double ky2 = ky*ky;
        double k = sqrt(kx2 + ky2);

        // elevation
        complex h = zhat(ikx, iky);
        complex hi = h * iunit;

        // elevation derivatives
        complex hikx = hi * kx;
        complex hiky = hi * ky;

        fft_h_(ikx, iky) = h;
        fft_h_ikx_(ikx, iky) = hikx;
        fft_h_iky_(ikx, iky) = hiky;

        // displacement and derivatives
        if (std::abs(k) < 1.0E-8)
        {          
          fft_sx_(ikx, iky) = czero;
          fft_sy_(ikx, iky) = czero;
          fft_h_kxkx_(ikx, iky) = czero;
          fft_h_kyky_(ikx, iky) = czero;
          fft_h_kxky_(ikx, iky) = czero;
        }
        else
        {
          // displacements
          double ook = 1.0 / k;
          complex hok = h * ook;
          complex hiok = hi * ook;
          complex dx = - hiok * kx;
          complex dy = - hiok * ky;

          // displacements derivatives
          complex hkxkx = hok * kx2;
          complex hkyky = hok * ky2;
          complex hkxky = hok * kx * ky;
          
          fft_sx_(ikx, iky) = dx;
          fft_sy_(ikx, iky) = dy;
          fft_h_kxkx_(ikx, iky) = hkxkx;
          fft_h_kyky_(ikx, iky) = hkyky;
          fft_h_kxky_(ikx, iky) = hkxky;
        }
      }
    }
#endif
  }

  //////////////////////////////////////////////////
  void WaveSimulationFFTImpl::InitFFTCoeffStorage()
  {
    // initialise storage for Fourier coefficients
    fft_h_      = Eigen::MatrixXcdRowMajor::Zero(nx_, ny_);
    fft_h_ikx_  = Eigen::MatrixXcdRowMajor::Zero(nx_, ny_);
    fft_h_iky_  = Eigen::MatrixXcdRowMajor::Zero(nx_, ny_);
    fft_sx_     = Eigen::MatrixXcdRowMajor::Zero(nx_, ny_);
    fft_sy_     = Eigen::MatrixXcdRowMajor::Zero(nx_, ny_);
    fft_h_kxkx_ = Eigen::MatrixXcdRowMajor::Zero(nx_, ny_);
    fft_h_kyky_ = Eigen::MatrixXcdRowMajor::Zero(nx_, ny_);
    fft_h_kxky_ = Eigen::MatrixXcdRowMajor::Zero(nx_, ny_);
  }

  //////////////////////////////////////////////////
  void WaveSimulationFFTImpl::InitWaveNumbers()
  {
    kx_fft_  = Eigen::VectorXd::Zero(nx_);
    ky_fft_  = Eigen::VectorXd::Zero(ny_);
    kx_math_ = Eigen::VectorXd::Zero(nx_);
    ky_math_ = Eigen::VectorXd::Zero(ny_);

    // wavenumbers in fft and math ordering
    for(int ikx = 0; ikx < nx_; ++ikx)
    {
      double kx = (ikx - nx_/2) * kx_f_;
      kx_math_(ikx) = kx;
      kx_fft_((ikx + nx_/2) % nx_) = kx;
    }

    for(int iky = 0; iky < ny_; ++iky)
    {
      double ky = (iky - ny_/2) * ky_f_;
      ky_math_(iky) = ky;
      ky_fft_((iky + ny_/2) % ny_) = ky;
    }

#if VECTORISE_ZHAT_CALCS
    // broadcast (fft) wavenumbers to arrays (aka meshgrid)
    kx_ = Eigen::MatrixXd::Zero(nx_, ny_);
    ky_ = Eigen::MatrixXd::Zero(nx_, ny_);
    kx_.colwise() += kx_fft_;
    ky_.rowwise() += ky_fft_.transpose();

    // wavenumber and wave angle arrays
    kx2_ = Eigen::pow(kx_.array(), 2.0);
    ky2_ = Eigen::pow(ky_.array(), 2.0);
    k_   = Eigen::sqrt(kx2_.array() + ky2_.array());
    theta_ = ky_.binaryExpr(
        kx_, [] (double y, double x) { return std::atan2(y, x);}
    );

    // array k_plus_ has no elements where abs(k_plus_) < 1.0E-8
    k_plus_ = (Eigen::abs(k_.array()) < 1.0E-8).select(
        Eigen::MatrixXd::Ones(nx_, ny_), k_);

    // set 1/k to zero when abs(k) < 1.0E-8 as the quantities it multiplies
    // have zero as the limit as k -> 0. 
    ook_ = (Eigen::abs(k_.array()) < 1.0E-8).select(
        Eigen::MatrixXd::Zero(nx_, ny_), 1.0 / k_plus_.array());
#endif
  }

  //////////////////////////////////////////////////
  void WaveSimulationFFTImpl::CreateFFTWPlans()
  {
    // elevation
    fft_out0_ = Eigen::MatrixXcdRowMajor::Zero(nx_, ny_);
    fft_out1_ = Eigen::MatrixXcdRowMajor::Zero(nx_, ny_);
    fft_out2_ = Eigen::MatrixXcdRowMajor::Zero(nx_, ny_);

    // xy-displacements
    fft_out3_ = Eigen::MatrixXcdRowMajor::Zero(nx_, ny_);
    fft_out4_ = Eigen::MatrixXcdRowMajor::Zero(nx_, ny_);
    fft_out5_ = Eigen::MatrixXcdRowMajor::Zero(nx_, ny_);
    fft_out6_ = Eigen::MatrixXcdRowMajor::Zero(nx_, ny_);
    fft_out7_ = Eigen::MatrixXcdRowMajor::Zero(nx_, ny_);

    // elevation
    fft_plan0_ = fftw_plan_dft_2d(nx_, ny_,
        reinterpret_cast<fftw_complex*>(fft_h_.data()),
        reinterpret_cast<fftw_complex*>(fft_out0_.data()),
        FFTW_BACKWARD, FFTW_ESTIMATE);
    fft_plan1_ = fftw_plan_dft_2d(nx_, ny_,
        reinterpret_cast<fftw_complex*>(fft_h_ikx_.data()),
        reinterpret_cast<fftw_complex*>(fft_out1_.data()),
        FFTW_BACKWARD, FFTW_ESTIMATE);
    fft_plan2_ = fftw_plan_dft_2d(nx_, ny_,
        reinterpret_cast<fftw_complex*>(fft_h_iky_.data()),
        reinterpret_cast<fftw_complex*>(fft_out2_.data()),
        FFTW_BACKWARD, FFTW_ESTIMATE);

    // xy-displacements
    fft_plan3_ = fftw_plan_dft_2d(nx_, ny_,
        reinterpret_cast<fftw_complex*>(fft_sx_.data()),
        reinterpret_cast<fftw_complex*>(fft_out3_.data()),
        FFTW_BACKWARD, FFTW_ESTIMATE);
    fft_plan4_ = fftw_plan_dft_2d(nx_, ny_,
        reinterpret_cast<fftw_complex*>(fft_sy_.data()),
        reinterpret_cast<fftw_complex*>(fft_out4_.data()),
        FFTW_BACKWARD, FFTW_ESTIMATE);
    fft_plan5_ = fftw_plan_dft_2d(nx_, ny_,
        reinterpret_cast<fftw_complex*>(fft_h_kxkx_.data()),
        reinterpret_cast<fftw_complex*>(fft_out5_.data()),
        FFTW_BACKWARD, FFTW_ESTIMATE);
    fft_plan6_ = fftw_plan_dft_2d(nx_, ny_,
        reinterpret_cast<fftw_complex*>(fft_h_kyky_.data()),
        reinterpret_cast<fftw_complex*>(fft_out6_.data()),
        FFTW_BACKWARD, FFTW_ESTIMATE);
    fft_plan7_ = fftw_plan_dft_2d(nx_, ny_,
        reinterpret_cast<fftw_complex*>(fft_h_kxky_.data()),
        reinterpret_cast<fftw_complex*>(fft_out7_.data()),
        FFTW_BACKWARD, FFTW_ESTIMATE);
  }

  //////////////////////////////////////////////////
  void WaveSimulationFFTImpl::DestroyFFTWPlans()
  {
    fftw_destroy_plan(fft_plan0_);
    fftw_destroy_plan(fft_plan1_);
    fftw_destroy_plan(fft_plan2_);
    fftw_destroy_plan(fft_plan3_);
    fftw_destroy_plan(fft_plan4_);
    fftw_destroy_plan(fft_plan5_);
    fftw_destroy_plan(fft_plan6_);
    fftw_destroy_plan(fft_plan7_);
  }

  //////////////////////////////////////////////////
  //////////////////////////////////////////////////
  WaveSimulationFFT::~WaveSimulationFFT()
  {
  }

  //////////////////////////////////////////////////
  WaveSimulationFFT::WaveSimulationFFT(
    double lx, double ly, int nx, int ny) :
    impl_(new WaveSimulationFFTImpl(lx, ly, nx, ny))
  {
  }

  //////////////////////////////////////////////////
  void WaveSimulationFFT::SetUseVectorised(bool value)
  {
    impl_->SetUseVectorised(value);
  }

  //////////////////////////////////////////////////
  void WaveSimulationFFT::SetLambda(double value)
  {
    impl_->SetLambda(value);
  }

  //////////////////////////////////////////////////
  void WaveSimulationFFT::SetWindVelocity(double ux, double uy)
  {
    impl_->SetWindVelocity(ux, uy);
  }

  //////////////////////////////////////////////////
  void WaveSimulationFFT::SetTime(double value)
  {
    impl_->SetTime(value);
  }

  //////////////////////////////////////////////////
  void WaveSimulationFFT::ComputeElevation(
    Eigen::Ref<Eigen::MatrixXd> h)
  {
    impl_->ComputeElevation(h);
  }

  //////////////////////////////////////////////////
  void WaveSimulationFFT::ComputeElevationDerivatives(
    Eigen::Ref<Eigen::MatrixXd> dhdx,
    Eigen::Ref<Eigen::MatrixXd> dhdy)
  {
    impl_->ComputeElevationDerivatives(dhdx, dhdy);
  }

  //////////////////////////////////////////////////
  void WaveSimulationFFT::ComputeDisplacements(
    Eigen::Ref<Eigen::MatrixXd> sx,
    Eigen::Ref<Eigen::MatrixXd> sy)
  {
    impl_->ComputeDisplacements(sx, sy);
  }

  //////////////////////////////////////////////////
  void WaveSimulationFFT::ComputeDisplacementsDerivatives(
    Eigen::Ref<Eigen::MatrixXd> dsxdx,
    Eigen::Ref<Eigen::MatrixXd> dsydy,
    Eigen::Ref<Eigen::MatrixXd> dsxdy)
  {
    impl_->ComputeDisplacementsDerivatives(dsxdx, dsxdy, dsxdy);
  }

  //////////////////////////////////////////////////
  void WaveSimulationFFT::ComputeDisplacementsAndDerivatives(
    Eigen::Ref<Eigen::MatrixXd> h,
    Eigen::Ref<Eigen::MatrixXd> sx,
    Eigen::Ref<Eigen::MatrixXd> sy,
    Eigen::Ref<Eigen::MatrixXd> dhdx,
    Eigen::Ref<Eigen::MatrixXd> dhdy,
    Eigen::Ref<Eigen::MatrixXd> dsxdx,
    Eigen::Ref<Eigen::MatrixXd> dsydy,
    Eigen::Ref<Eigen::MatrixXd> dsxdy)
  {
    impl_->ComputeElevation(h);
    impl_->ComputeElevationDerivatives(dhdx, dhdy);
    impl_->ComputeDisplacements(sx, sy);
    impl_->ComputeDisplacementsDerivatives(dsxdx, dsydy, dsxdy);
  }
}
}