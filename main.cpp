#include <iostream>
#include <math.h>
#include "ukf.h"
#include "ekf.h"
#include <fstream>
#include <iostream>
#include <sstream>
#include <stdlib.h>
#include "ground_truth_package.h"
#include "Eigen/Dense"
#include "measurement_package.h"

//using namespace std;
//using Eigen::MatrixXd;
//using Eigen::VectorXd;
//using std::vector;


void check_arguments(int argc, char* argv[]) {
    std::string usage_instructions = "Usage instructions: ";
    usage_instructions += argv[0];
    usage_instructions += " path/to/input.txt output.txt";

    bool has_valid_args = false;

    // make sure the user has provided input and output files
    if (argc == 1) {
        std::cerr << usage_instructions << std::endl;
    } else if (argc == 2) {
		std::cerr << "Please include an output file.\n" << usage_instructions << std::endl;
    } else if (argc == 3) {
        has_valid_args = true;
    } else if (argc > 3) {
		std::cerr << "Too many arguments.\n" << usage_instructions << std::endl;
    }

    if (!has_valid_args) {
        exit(EXIT_FAILURE);
    }
}

void check_files(std::ifstream& in_file, std::string& in_name,
	std::ofstream& out_file, std::string& out_name) {
    if (!in_file.is_open()) {
		std::cerr << "Cannot open input file: " << in_name << std::endl;
        exit(EXIT_FAILURE);
    }

    if (!out_file.is_open()) {
		std::cerr << "Cannot open output file: " << out_name << std::endl;
        exit(EXIT_FAILURE);
    }
}

Eigen::VectorXd CalculateRMSE(const std::vector<Eigen::VectorXd> &estimations,
	const std::vector<Eigen::VectorXd> &ground_truth) {
	Eigen::VectorXd rmse(4);
    rmse << 0,0,0,0;

    // check the validity of the following inputs:
    //  * the estimation vector size should not be zero
    //  * the estimation vector size should equal ground truth vector size
    if(estimations.size() == 0 || estimations.size() != ground_truth.size()){
		std::cout << "the input is not legal!!!" << std::endl;
        return rmse;
    }

    //accumulate squared residuals
    for(int i=0; i < estimations.size(); ++i){
		Eigen::VectorXd residual = estimations[i] - ground_truth[i];
        residual = residual.array() * residual.array();
        rmse = rmse + residual;
    }

    //calculate the mean
    rmse = rmse / estimations.size();

    //calculate the squared root
    rmse = rmse.array().sqrt();

    //return the result
    return rmse;

}

int main(int argc, char* argv[]) {
    //check_arguments(argc, argv);

	std::string in_file_name_ = "D:\\WORK\\KF\\data\\data_synthetic.txt";//argv[1];
	std::ifstream in_file_(in_file_name_.c_str(), std::ifstream::in);

	std::string out_file_name_ = "D:\\WORK\\KF\\data\\output.txt";//argv[2];
	std::ofstream out_file_(out_file_name_.c_str(), std::ofstream::out);

    check_files(in_file_, in_file_name_, out_file_, out_file_name_);

	std::vector<MeasurementPackage> measurement_pack_list;//测量值数据包
	std::vector<GroundTruthPackage> gt_pack_list;//真实值

	std::string line;

    // prep the measurement packages (each line represents a measurement at a
    // timestamp)
	float x;
	float y;
	float ro;
	float phi;
	float ro_dot;
	float x_gt;
	float y_gt;
	float vx_gt;
	float vy_gt;
    while (getline(in_file_, line)) {

		std::string sensor_type;
        MeasurementPackage meas_package;
        GroundTruthPackage gt_package;
		std::istringstream iss(line);
        long long timestamp;

        // reads first element from the current line
        iss >> sensor_type;
        if (sensor_type.compare("L") == 0) {
            // LASER MEASUREMENT

            // read measurements at this timestamp
            meas_package.sensor_type_ = MeasurementPackage::LASER;
			meas_package.raw_measurements_ = Eigen::VectorXd(2);
            iss >> x;
            iss >> y;
            meas_package.raw_measurements_ << x, y;
            iss >> timestamp;
            meas_package.timestamp_ = timestamp;
            measurement_pack_list.push_back(meas_package);
        } else if (sensor_type.compare("R") == 0) {
            // RADAR MEASUREMENT

            // read measurements at this timestamp
            meas_package.sensor_type_ = MeasurementPackage::RADAR;
			meas_package.raw_measurements_ = Eigen::VectorXd(3);
            iss >> ro;
            iss >> phi;
            iss >> ro_dot;
            meas_package.raw_measurements_ << ro, phi, ro_dot;
            iss >> timestamp;
            meas_package.timestamp_ = timestamp;
            measurement_pack_list.push_back(meas_package);
        }

        // read ground truth data to compare later
        iss >> x_gt;
        iss >> y_gt;
        iss >> vx_gt;
        iss >> vy_gt;
		gt_package.gt_values_ = Eigen::VectorXd(4);
        gt_package.gt_values_ << x_gt, y_gt, vx_gt, vy_gt;
        gt_pack_list.push_back(gt_package);
    }

    // Create a Fusion EKF instance
    EKF ekf;

    // used to compute the RMSE later
	std::vector<Eigen::VectorXd> estimations;
	std::vector<Eigen::VectorXd> ground_truth;

    //Call the EKF-based fusion
    size_t N = measurement_pack_list.size();
    for (size_t k = 0; k < N; ++k) {
        // start filtering from the second frame (the speed is unknown in the first
        // frame)
        ekf.ProcessMeasurement(measurement_pack_list[k]);

        double p_x = ekf.ekf_.x_(0);
		double p_y = ekf.ekf_.x_(1);
		double v =   ekf.ekf_.x_(2);
		double yaw = ekf.ekf_.x_(3);

        double v1 = cos(yaw) * v;
        double v2 = sin(yaw) * v;

        VectorXd estimate(4);

        estimate(0) = p_x;
        estimate(1) = p_y;
        estimate(2) = v1;
        estimate(3) = v2;

        // output the estimation
        out_file_ << p_x << " ";
        out_file_ << p_y << " ";
        out_file_ << v1 << " ";
        out_file_ << v2 << " ";

        // output the measurements
        if (measurement_pack_list[k].sensor_type_ == MeasurementPackage::LASER) {
            // output the estimation
            out_file_ << measurement_pack_list[k].raw_measurements_(0) << " ";
            out_file_ << measurement_pack_list[k].raw_measurements_(1) << " ";
        } else if (measurement_pack_list[k].sensor_type_ == MeasurementPackage::RADAR) {
            // output the estimation in the cartesian coordinates
            float ro = measurement_pack_list[k].raw_measurements_(0);
            float phi = measurement_pack_list[k].raw_measurements_(1);
            out_file_ << ro * cos(phi) << " "; // p1_meas
            out_file_ << ro * sin(phi) << " "; // ps_meas
        }

        // output the ground truth packages
        out_file_ << gt_pack_list[k].gt_values_(0) << " ";
        out_file_ << gt_pack_list[k].gt_values_(1) << " ";
        out_file_ << gt_pack_list[k].gt_values_(2) << " ";
        out_file_ << gt_pack_list[k].gt_values_(3) << "\n";

        estimations.push_back(estimate);
        ground_truth.push_back(gt_pack_list[k].gt_values_);
    }

    // compute the accuracy (RMSE)
	std::cout << "Accuracy - RMSE:" << std::endl << CalculateRMSE(estimations, ground_truth) << std::endl;

    // close files
    if (out_file_.is_open()) {
        out_file_.close();
    }

    if (in_file_.is_open()) {
        in_file_.close();
    }

    return 0;
}