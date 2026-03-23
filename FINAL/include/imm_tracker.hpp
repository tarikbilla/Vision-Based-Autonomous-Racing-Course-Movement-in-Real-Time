/*──────────────────────────────────────────────────────────────────────
 *  imm_tracker.hpp  –  Interacting Multiple Model (IMM) tracker
 *                       CV (constant velocity) + CT (coordinated turn)
 *──────────────────────────────────────────────────────────────────────*/
#ifndef VISIONRC_IMM_TRACKER_HPP
#define VISIONRC_IMM_TRACKER_HPP

#include <opencv2/opencv.hpp>
#include <array>

namespace vrc {

/* ── Kalman filter constants ───────────────────────────────────── */
constexpr double CV_ACC_SIGMA      = 200.0;
constexpr double CT_ACC_SIGMA      =  30.0;
constexpr double CT_OMEGA_SIGMA    =   0.3;
constexpr double CT_OMEGA_MAX      =   6.0;
constexpr double CT_OMEGA_INIT_VAR =   0.01;
constexpr double MEAS_SIGMA_PX     =   8.0;
constexpr double IMM_P_STAY_CV     =   0.97;
constexpr double IMM_P_STAY_CT     =   0.92;

/* ── blending / gating ─────────────────────────────────────────── */
constexpr double MEAS_GATE_PX           = 70.0;
constexpr double MEAS_ALPHA_SLOW        = 0.65;
constexpr double MEAS_ALPHA_FAST        = 0.88;
constexpr double FAST_SPEED_PX_S        = 450.0;
constexpr int    MOTION_WINDOW          = 1;
constexpr double MIN_STEP_DIST          = 1.2;
constexpr double EMA_ALPHA_SLOW         = 0.28;
constexpr double EMA_ALPHA_FAST         = 0.70;
constexpr double PREDICT_DT_MIN         = 0.060;
constexpr double PREDICT_DT_MAX         = 0.180;
constexpr double MIN_SPEED_VEL_HEADING  = 8.0;
constexpr double MIN_SPEED_PREDICT      = 5.0;
constexpr int    NEAREST_WIN            = 40;
constexpr int    HEADING_LEN            = 90;

/* ── CV model ──────────────────────────────────────────────────── */
class CVModel {
public:
    static constexpr int DIM = 4;
    cv::Mat x = cv::Mat::zeros(4,1,CV_64F);
    cv::Mat P = cv::Mat::eye(4,4,CV_64F) * 500.0;
    static cv::Mat F(double dt);
    static cv::Mat Q(double dt);
};

/* ── CT model ──────────────────────────────────────────────────── */
class CTModel {
public:
    static constexpr int DIM = 5;
    cv::Mat x = cv::Mat::zeros(5,1,CV_64F);
    cv::Mat P = cv::Mat::eye(5,5,CV_64F) * 500.0;
    CTModel();
    static cv::Mat F(double dt, double omega);
    static cv::Mat Q(double dt);
};

/* ── IMM tracker ───────────────────────────────────────────────── */
class IMMTracker {
public:
    static constexpr int N = 2;
    IMMTracker();

    bool inited() const { return inited_; }
    void init_at(double x, double y);

    std::array<double,4> correct_always(double zx, double zy, double dt);
    std::array<double,4> hold();

    double mu_cv() const { return mu_.at<double>(0,0); }
    double mu_ct() const { return mu_.at<double>(1,0); }
    double omega_deg_s() const;

private:
    std::pair<cv::Mat,cv::Mat> to5(int idx) const;
    std::array<double,4> fuse();
    CVModel cv_;
    CTModel ct_;
    cv::Mat mu_;
    cv::Mat Pi_;
    bool    inited_     = false;
    double  last_omega_ = 0.0;
};

/* ── heading / alpha helpers ───────────────────────────────────── */
double ema_angle(std::optional<double> prev, double now_a, double alpha);
double adaptive_alpha(double speed);
double adaptive_meas_alpha(double speed);

cv::Mat Hfor(int dim);

}  // namespace vrc

#endif  // VISIONRC_IMM_TRACKER_HPP
