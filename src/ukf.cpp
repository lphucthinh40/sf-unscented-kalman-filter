#include "ukf.h"
#include "Eigen/Dense"
#include <iostream>
using Eigen::MatrixXd;
using Eigen::VectorXd;

using namespace std;
/**
 * Initializes Unscented Kalman filter
 */
UKF::UKF() {
  // if this is false, laser measurements will be ignored (except during init)
  use_laser_ = true;

  // if this is false, radar measurements will be ignored (except during init)
  use_radar_ = true;

  // initial state vector
  x_ = VectorXd(5);

  // Process noise standard deviation longitudinal acceleration in m/s^2
  std_a_ = 3;

  // Process noise standard deviation yaw acceleration in rad/s^2
  std_yawdd_ = 2.5;
  
  /**
   * DO NOT MODIFY measurement noise values below.
   * These are provided by the sensor manufacturer.
   */

  // Laser measurement noise standard deviation position1 in m
  std_laspx_ = 0.15;

  // Laser measurement noise standard deviation position2 in m
  std_laspy_ = 0.15;

  // Radar measurement noise standard deviation radius in m
  std_radr_ = 0.3;

  // Radar measurement noise standard deviation angle in rad
  std_radphi_ = 0.03;

  // Radar measurement noise standard deviation radius change in m/s
  std_radrd_ = 0.3;
  
  /**
   * End DO NOT MODIFY section for measurement noise values 
   */
  
  /**
   * TODO: Complete the initialization. See ukf.h for other member properties.
   * Hint: one or more values initialized above might be wildly off...
   */

  // Initialize Process Covariance Matrix
  P_ = MatrixXd::Identity(5, 5);
  P_(0,0) = std_laspx_*std_laspx_; 
  P_(1,1) = std_laspy_*std_laspy_; 
  P_(2,2) = 1000; 

  // State dimension
  n_x_ = 5;
  // Augmented state dimension
  n_aug_ = 7;
  // Sigma point spreading parameter
  lambda_ = 3 - n_x_;
  // NIS
  nis_ = 0.0;
  
  // Initialize sigma X prediction 
  Xsig_pred_ = MatrixXd(n_x_, 1+2*n_aug_);
  // Initialize weights
  weights_ = VectorXd(1+2*n_aug_);
  weights_(0) = static_cast<double> (lambda_) / (lambda_+n_aug_);
  for (int i=1; i<2*n_aug_+1; ++i) {  // 2n+1 weights
    weights_(i) = 0.5/(n_aug_+lambda_);
  }

  // State will be initialized by the first measurement
  is_initialized_ = false;
}

UKF::~UKF() {}

void UKF::ProcessMeasurement(MeasurementPackage meas_package) {
  /**
   * TODO: Complete this function! Make sure you switch between lidar and radar
   * measurements.
   */
  // Initialize object state using first-time measurement
  if (is_initialized_ == false)
  { 
    if (meas_package.sensor_type_ == MeasurementPackage::LASER)
    { 
      x_ << meas_package.raw_measurements_(0),
            meas_package.raw_measurements_(1),
            0, 0, 0;
    }
    else if (meas_package.sensor_type_ == MeasurementPackage::RADAR)
    { 
      double rho = meas_package.raw_measurements_(0);
      double phi = meas_package.raw_measurements_(1);
      x_ << rho * cos(phi),
            rho * sin(phi),
            0, 0, 0;
    }
    
    time_us_ = meas_package.timestamp_;
    is_initialized_ = true;
  }
  // Otherwise, update state with the new measurement
  else
  {
    // Predict
    double delta_t = (meas_package.timestamp_ - time_us_) / 1000000.0;
    Prediction(delta_t);

    // Update
    switch (meas_package.sensor_type_)
    {
      case MeasurementPackage::LASER: { if (use_laser_) {
                                            
                                            UpdateLidar(meas_package); 
                                            use_laser_ = false;
                                            use_radar_ = true;
                                        }
                                      }
                                        break;
      case MeasurementPackage::RADAR: { if (use_radar_) {
                                            UpdateRadar(meas_package); 
                                            use_radar_ = false;
                                            use_laser_ = true;
                                        }
                                      }
                                        break;
    }

    // Record current state time
    time_us_ = meas_package.timestamp_;
  }
}

void UKF::Prediction(double delta_t) {
  /**
   * TODO: Complete this function! Estimate the object's location. 
   * Modify the state vector, x_. Predict sigma points, the state, 
   * and the state covariance matrix.
   */

  // ________________________________
  //
  //      GENERATE SIGMA POINTS
  // ________________________________

  // create augmented mean vector
  VectorXd x_aug = VectorXd(7);

  // create augmented state covariance
  MatrixXd P_aug = MatrixXd(7, 7);

  // create sigma point matrix
  MatrixXd Xsig_aug = MatrixXd(n_aug_, 2 * n_aug_ + 1);

  // create augmented mean state
  x_aug.head(5) = x_;
  x_aug(5) = 0;
  x_aug(6) = 0;

  // create augmented covariance matrix
  P_aug.fill(0.0);
  P_aug.topLeftCorner(5,5) = P_;
  P_aug(5,5) = std_a_*std_a_;
  P_aug(6,6) = std_yawdd_*std_yawdd_;

  // create square root matrix
  MatrixXd L = P_aug.llt().matrixL();

  // create augmented sigma points
  Xsig_aug.col(0)  = x_aug;
  for (int i = 0; i< n_aug_; ++i) {
    Xsig_aug.col(i+1)       = x_aug + sqrt(lambda_+n_aug_) * L.col(i);
    Xsig_aug.col(i+1+n_aug_) = x_aug - sqrt(lambda_+n_aug_) * L.col(i);
  }

  // ________________________________
  //
  //   PREDICT WITH SIGMA POINTS
  // ________________________________
  double px, py, v, yaw, yaw_rate, nu_a, nu_yaw;

  for (int i=0; i<1+n_aug_*2; ++i)
  { 
    px = Xsig_aug(0, i);
    py = Xsig_aug(1, i);
    v = Xsig_aug(2, i);
    yaw = Xsig_aug(3, i);
    yaw_rate = Xsig_aug(4, i);
    nu_a = Xsig_aug(5, i);
    nu_yaw = Xsig_aug(6, i);
    
    if (fabs(yaw_rate)>0.001)
    {
        Xsig_pred_(0, i) = px + v/yaw_rate * (sin(yaw+delta_t*yaw_rate) - sin(yaw)) + 0.5 * nu_a * cos(yaw) * delta_t * delta_t;
        Xsig_pred_(1, i) = py + v/yaw_rate * (-cos(yaw+delta_t*yaw_rate) + cos(yaw)) + 0.5 * nu_a * sin(yaw) * delta_t * delta_t;
        
    }
    else
    {
        Xsig_pred_(0, i) = px + v * cos(yaw) * delta_t + 0.5 * nu_a * cos(yaw) * delta_t * delta_t;
        Xsig_pred_(1, i) = py + v * sin(yaw) * delta_t + 0.5 * nu_a * sin(yaw) * delta_t * delta_t;
    }

    Xsig_pred_(2, i) = v + delta_t * nu_a;
    Xsig_pred_(3, i) = yaw + delta_t * yaw_rate + 0.5 * delta_t * delta_t * nu_yaw;
    Xsig_pred_(4, i) = yaw_rate + delta_t * nu_yaw;      
  }

  // ________________________________
  //
  //  PREDICT MEAN X & COVARIANCE P
  // ________________________________

  // predicted state mean
  x_.fill(0.0);
  for (int i = 0; i < 2 * n_aug_ + 1; ++i) {  // iterate over sigma points
    x_ = x_ + weights_(i) * Xsig_pred_.col(i);
  }

  // predicted state covariance matrix
  P_.fill(0.0);
  for (int i = 0; i < 2 * n_aug_ + 1; ++i) {  // iterate over sigma points
    // state difference
    VectorXd x_diff = Xsig_pred_.col(i) - x_;
    // angle normalization
    while (x_diff(3)> M_PI) x_diff(3)-=2.*M_PI;
    while (x_diff(3)<-M_PI) x_diff(3)+=2.*M_PI;

    P_ = P_ + weights_(i) * x_diff * x_diff.transpose() ;
  }
}

void UKF::UpdateLidar(MeasurementPackage meas_package) {
  /**
   * TODO: Complete this function! Use lidar data to update the belief 
   * about the object's position. Modify the state vector, x_, and 
   * covariance, P_.
   * You can also calculate the lidar NIS, if desired.
   */

  // measurement vector
  VectorXd z = VectorXd(2);
  z << meas_package.raw_measurements_(0), meas_package.raw_measurements_(1);

  // measurement matrix
  MatrixXd H = MatrixXd(2, 5);  // n_x_ = 5
  H <<  1, 0, 0, 0, 0,
        0, 1, 0, 0, 0;

  // measurement covariance
  MatrixXd R = MatrixXd(2, 2);
  R <<  std_laspx_*std_laspx_, 0,
        0, std_laspy_*std_laspy_;

  // predicted measurement
  VectorXd z_pred = H * x_;

  // measurement residual (innovation)
  VectorXd y = z - z_pred;

  // calculate Kalman Gain
  MatrixXd Ht = H.transpose();
  MatrixXd S = H * P_ * Ht + R;
  MatrixXd Si = S.inverse();
  MatrixXd PHt = P_ * Ht;
  MatrixXd K = PHt * Si;

  //new estimate
  x_ = x_ + (K * y);
  MatrixXd I = MatrixXd::Identity(n_x_, n_x_);
  P_ = (I - K * H) * P_;

  //calculate NIS
  double nis_ = y.transpose() * Si * y;

}

void UKF::UpdateRadar(MeasurementPackage meas_package) {
  /**
   * TODO: Complete this function! Use radar data to update the belief 
   * about the object's position. Modify the state vector, x_, and 
   * covariance, P_.
   * You can also calculate the radar NIS, if desired.
   */

  // ________________________________
  //
  //  GENERATE PREDICT MEASUREMENT
  // ________________________________

  // create matrix for sigma points in measurement space
  MatrixXd Zsig = MatrixXd(3, 2 * n_aug_ + 1);

  // mean predicted measurement
  VectorXd z_pred = VectorXd(3);
  
  // measurement covariance matrix S
  MatrixXd S = MatrixXd(3,3);

  // transform sigma points into measurement space
  for (int i = 0; i < 2 * n_aug_ + 1; ++i) {  // 2n+1 simga points
    double p_x = Xsig_pred_(0,i);
    double p_y = Xsig_pred_(1,i);
    double  v  = Xsig_pred_(2,i);
    double yaw = Xsig_pred_(3,i);

    double v_x = cos(yaw)*v;
    double v_y = sin(yaw)*v;

    // measurement model
    Zsig(0,i) = sqrt(p_x*p_x + p_y*p_y);                         // r
    Zsig(1,i) = atan2(p_y,p_x);                                  // phi
    Zsig(2,i) = (p_x*v_x + p_y*v_y) / sqrt(p_x*p_x + p_y*p_y);   // r_dot
  }

  // mean predicted measurement
  z_pred.fill(0.0);
  for (int i=0; i < 2*n_aug_+1; ++i) {
    z_pred = z_pred + weights_(i) * Zsig.col(i);
  }

  // innovation covariance matrix S
  S.fill(0.0);
  for (int i = 0; i < 2*n_aug_+1; ++i) {  // 2n+1 simga points
    // residual
    VectorXd z_diff = Zsig.col(i) - z_pred;

    // angle normalization
    while (z_diff(1)> M_PI) z_diff(1)-=2.*M_PI;
    while (z_diff(1)<-M_PI) z_diff(1)+=2.*M_PI;

    S = S + weights_(i) * z_diff * z_diff.transpose();
  }

  // add measurement noise covariance matrix
  MatrixXd R = MatrixXd(3,3);
  R <<  std_radr_*std_radr_, 0, 0,
        0, std_radphi_*std_radphi_, 0,
        0, 0,std_radrd_*std_radrd_;
  S = S + R;

  // ________________________________
  //
  //       UPDATE MEASUREMENT
  // ________________________________

  // measurement vector
  VectorXd z = VectorXd(3);
  z <<  meas_package.raw_measurements_(0),
        meas_package.raw_measurements_(1),
        meas_package.raw_measurements_(2);

  // matrix for cross correlation Tc
  MatrixXd Tc = MatrixXd(5, 3); // n_x_ = 5, n_z_ = 3

  // calculate cross correlation matrix
  Tc.fill(0.0);
  for (int i = 0; i < 2 * n_aug_ + 1; ++i) {  // 2n+1 simga points
    // residual
    VectorXd z_diff = Zsig.col(i) - z_pred;
    // angle normalization
    while (z_diff(1)> M_PI) z_diff(1)-=2.*M_PI;
    while (z_diff(1)<-M_PI) z_diff(1)+=2.*M_PI;

    // state difference
    VectorXd x_diff = Xsig_pred_.col(i) - x_;
    // angle normalization
    while (x_diff(3)> M_PI) x_diff(3)-=2.*M_PI;
    while (x_diff(3)<-M_PI) x_diff(3)+=2.*M_PI;

    Tc = Tc + weights_(i) * x_diff * z_diff.transpose();
  }

  // Kalman gain K;
  MatrixXd K = Tc * S.inverse();

  // residual
  VectorXd z_diff = z - z_pred;

  // angle normalization
  while (z_diff(1)> M_PI) z_diff(1)-=2.*M_PI;
  while (z_diff(1)<-M_PI) z_diff(1)+=2.*M_PI;

  // update state mean and covariance matrix
  x_ = x_ + K * z_diff;
  P_ = P_ - K*S*K.transpose();

  // calculate NIS
  MatrixXd Si = S.inverse();
  double nis_ = z_diff.transpose() * Si * z_diff;
}