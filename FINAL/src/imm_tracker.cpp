/*──────────────────────────────────────────────────────────────────────
 *  imm_tracker.cpp  –  IMM Kalman tracker (CV + CT)
 *──────────────────────────────────────────────────────────────────────*/
#include "imm_tracker.hpp"
#include "types.hpp"

#include <cmath>
#include <optional>

namespace vrc {

static const std::array<double,2> IMM_MU0 = {0.8, 0.2};

/* ================================================================
 *  CV model
 * ================================================================*/
cv::Mat CVModel::F(double dt) {
    return (cv::Mat_<double>(4,4) <<
        1,0,dt,0,  0,1,0,dt,  0,0,1,0,  0,0,0,1);
}
cv::Mat CVModel::Q(double dt) {
    double dt2=dt*dt, dt3=dt2*dt, dt4=dt3*dt;
    double q = CV_ACC_SIGMA * CV_ACC_SIGMA;
    return (cv::Mat_<double>(4,4) <<
        dt4/4, 0, dt3/2, 0,
        0, dt4/4, 0, dt3/2,
        dt3/2, 0, dt2, 0,
        0, dt3/2, 0, dt2) * q;
}

/* ================================================================
 *  CT model
 * ================================================================*/
CTModel::CTModel() { P.at<double>(4,4) = CT_OMEGA_INIT_VAR; }

cv::Mat CTModel::F(double dt, double omega) {
    if (std::abs(omega) < 1e-4)
        return (cv::Mat_<double>(5,5) <<
            1,0,dt,0,0,  0,1,0,dt,0,  0,0,1,0,0,  0,0,0,1,0,  0,0,0,0,1);
    double s=std::sin(omega*dt), c=std::cos(omega*dt), o=omega;
    return (cv::Mat_<double>(5,5) <<
        1,0,s/o,-(1-c)/o,0,
        0,1,(1-c)/o,s/o,0,
        0,0,c,-s,0,
        0,0,s,c,0,
        0,0,0,0,1);
}
cv::Mat CTModel::Q(double dt) {
    double dt2=dt*dt, dt3=dt2*dt, dt4=dt3*dt;
    double qa = CT_ACC_SIGMA * CT_ACC_SIGMA;
    cv::Mat Q5 = cv::Mat::zeros(5,5,CV_64F);
    cv::Mat t = (cv::Mat_<double>(4,4) <<
        dt4/4,0,dt3/2,0,  0,dt4/4,0,dt3/2,
        dt3/2,0,dt2,0,    0,dt3/2,0,dt2) * qa;
    t.copyTo(Q5(cv::Rect(0,0,4,4)));
    Q5.at<double>(4,4) = (CT_OMEGA_SIGMA * dt) * (CT_OMEGA_SIGMA * dt);
    return Q5;
}

/* ================================================================
 *  H matrix
 * ================================================================*/
cv::Mat Hfor(int dim) {
    if (dim == 4)
        return (cv::Mat_<double>(2,4) << 1,0,0,0, 0,1,0,0);
    return (cv::Mat_<double>(2,5) << 1,0,0,0,0, 0,1,0,0,0);
}

/* ================================================================
 *  IMMTracker
 * ================================================================*/
IMMTracker::IMMTracker() {
    mu_ = cv::Mat(2,1,CV_64F);
    mu_.at<double>(0,0) = IMM_MU0[0];
    mu_.at<double>(1,0) = IMM_MU0[1];
    Pi_ = (cv::Mat_<double>(2,2) <<
        IMM_P_STAY_CV, 1.0-IMM_P_STAY_CV,
        1.0-IMM_P_STAY_CT, IMM_P_STAY_CT);
}

void IMMTracker::init_at(double x, double y) {
    cv_.x = cv::Mat::zeros(4,1,CV_64F);
    cv_.x.at<double>(0,0)=x; cv_.x.at<double>(1,0)=y;
    cv_.P = cv::Mat::eye(4,4,CV_64F)*200.0;

    ct_.x = cv::Mat::zeros(5,1,CV_64F);
    ct_.x.at<double>(0,0)=x; ct_.x.at<double>(1,0)=y;
    ct_.P = cv::Mat::eye(5,5,CV_64F)*200.0;
    ct_.P.at<double>(4,4) = CT_OMEGA_INIT_VAR;
    inited_ = true;
}

std::pair<cv::Mat,cv::Mat> IMMTracker::to5(int idx) const {
    if (idx == 0) {
        cv::Mat x5 = cv::Mat::zeros(5,1,CV_64F);
        cv_.x.copyTo(x5.rowRange(0,4));
        x5.at<double>(4,0) = last_omega_;
        cv::Mat P5 = cv::Mat::zeros(5,5,CV_64F);
        cv_.P.copyTo(P5(cv::Rect(0,0,4,4)));
        P5.at<double>(4,4) = CT_OMEGA_INIT_VAR;
        return {x5, P5};
    }
    return {ct_.x.clone(), ct_.P.clone()};
}

std::array<double,4> IMMTracker::fuse() {
    cv::Mat x5 = cv::Mat::zeros(5,1,CV_64F);
    auto [x05, p05] = to5(0);
    auto [x15, p15] = to5(1);
    x5 += mu_.at<double>(0,0) * x05;
    x5 += mu_.at<double>(1,0) * x15;
    return {x5.at<double>(0,0), x5.at<double>(1,0),
            x5.at<double>(2,0), x5.at<double>(3,0)};
}

double IMMTracker::omega_deg_s() const {
    return ct_.x.rows >= 5 ? ct_.x.at<double>(4,0) * 180.0 / kPi : 0.0;
}

std::array<double,4> IMMTracker::correct_always(double zx, double zy,
                                                 double dt) {
    dt = clampv(dt, 1e-3, 0.2);
    if (!inited_) init_at(zx, zy);

    cv::Mat z = (cv::Mat_<double>(2,1) << zx, zy);
    cv::Mat R = cv::Mat::eye(2,2,CV_64F) * (MEAS_SIGMA_PX*MEAS_SIGMA_PX);

    cv::Mat c = Pi_.t() * mu_;
    cv::Mat mu_ij = cv::Mat::zeros(2,2,CV_64F);
    for (int i=0;i<N;++i)
        for (int j=0;j<N;++j)
            mu_ij.at<double>(i,j) =
                Pi_.at<double>(i,j)*mu_.at<double>(i,0)/(c.at<double>(j,0)+1e-300);

    std::array<cv::Mat,2> mx, mP;
    for (int j=0;j<N;++j) {
        cv::Mat x5j = cv::Mat::zeros(5,1,CV_64F);
        for (int i=0;i<N;++i) {
            auto [xi5,Pi5] = to5(i);
            x5j += mu_ij.at<double>(i,j) * xi5;
        }
        cv::Mat P5j = cv::Mat::zeros(5,5,CV_64F);
        for (int i=0;i<N;++i) {
            auto [xi5,Pi5] = to5(i);
            cv::Mat d = xi5 - x5j;
            P5j += mu_ij.at<double>(i,j) * (Pi5 + d * d.t());
        }
        int dj = (j==0) ? 4 : 5;
        mx[j] = x5j.rowRange(0,dj).clone();
        mP[j] = P5j(cv::Rect(0,0,dj,dj)).clone();
    }

    cv::Mat liks = cv::Mat::zeros(2,1,CV_64F);

    // ── CV update ────────────────────────────────────────────────
    {
        cv_.x = mx[0].clone(); cv_.P = mP[0].clone();
        cv::Mat Fmat = CVModel::F(dt), Qmat = CVModel::Q(dt);
        cv::Mat xp = Fmat * cv_.x;
        cv::Mat Pp = Fmat * cv_.P * Fmat.t() + Qmat;
        cv::Mat H = Hfor(4);
        cv::Mat S = H*Pp*H.t() + R;
        cv::Mat K = Pp*H.t() * S.inv();
        cv::Mat innov = z - H*xp;
        cv_.x = xp + K*innov;
        cv::Mat IKH = cv::Mat::eye(4,4,CV_64F) - K*H;
        cv_.P = IKH*Pp*IKH.t() + K*R*K.t();
        double detS = std::max(cv::determinant(S), 1e-300);
        cv::Mat tmp = innov.t()*S.inv()*innov;
        double mahal2 = tmp.at<double>(0,0);
        liks.at<double>(0,0) =
            std::exp(std::max(-0.5*mahal2, -500.0)) /
            std::sqrt(std::pow(2*kPi, 2) * detS);
    }

    // ── CT update ────────────────────────────────────────────────
    {
        ct_.x = mx[1].clone(); ct_.P = mP[1].clone();
        double omega = clampv(ct_.x.at<double>(4,0), -CT_OMEGA_MAX, CT_OMEGA_MAX);
        ct_.x.at<double>(4,0) = omega;
        last_omega_ = omega;
        cv::Mat Fmat = CTModel::F(dt, omega), Qmat = CTModel::Q(dt);
        cv::Mat xp = Fmat * ct_.x;
        cv::Mat Pp = Fmat * ct_.P * Fmat.t() + Qmat;
        cv::Mat H = Hfor(5);
        cv::Mat S = H*Pp*H.t() + R;
        cv::Mat K = Pp*H.t() * S.inv();
        cv::Mat innov = z - H*xp;
        ct_.x = xp + K*innov;
        cv::Mat IKH = cv::Mat::eye(5,5,CV_64F) - K*H;
        ct_.P = IKH*Pp*IKH.t() + K*R*K.t();
        last_omega_ = ct_.x.at<double>(4,0);
        double detS = std::max(cv::determinant(S), 1e-300);
        cv::Mat tmp = innov.t()*S.inv()*innov;
        double mahal2 = tmp.at<double>(0,0);
        liks.at<double>(1,0) =
            std::exp(std::max(-0.5*mahal2, -500.0)) /
            std::sqrt(std::pow(2*kPi, 2) * detS);
    }

    // ── mode probability update ──────────────────────────────────
    mu_ = c.mul(liks);
    double d = cv::sum(mu_)[0];
    if (d < 1e-300) { mu_.at<double>(0,0)=IMM_MU0[0]; mu_.at<double>(1,0)=IMM_MU0[1]; }
    else mu_ /= d;

    return fuse();
}

std::array<double,4> IMMTracker::hold() { return fuse(); }

/* ================================================================
 *  Heading / alpha utilities
 * ================================================================*/
double ema_angle(std::optional<double> prev, double now_a, double alpha) {
    if (!prev.has_value()) return now_a;
    double mx = (1.0-alpha)*std::cos(*prev) + alpha*std::cos(now_a);
    double my = (1.0-alpha)*std::sin(*prev) + alpha*std::sin(now_a);
    return std::atan2(my, mx);
}

double adaptive_alpha(double speed) {
    double t = clampv(speed / FAST_SPEED_PX_S, 0.0, 1.0);
    return (1.0-t)*EMA_ALPHA_SLOW + t*EMA_ALPHA_FAST;
}

double adaptive_meas_alpha(double speed) {
    double t = clampv(speed / FAST_SPEED_PX_S, 0.0, 1.0);
    return (1.0-t)*MEAS_ALPHA_SLOW + t*MEAS_ALPHA_FAST;
}

}  // namespace vrc
