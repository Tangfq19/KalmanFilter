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
#include "ekf_ctrv.h"
#include "kf_Fusion.h"
#include "ekf_ctrv_geo.h"
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
int KFSimulation() {
	//check_arguments(argc, argv);

	std::string in_file_name_ = "../data/data_synthetic.txt";
	std::ifstream in_file_(in_file_name_.c_str(), std::ifstream::in);

	std::string out_file_name_ = "../data/output.txt";
	std::ofstream out_file_(out_file_name_.c_str(), std::ofstream::out);

	std::string out_file_name2_ = "../data/output2.txt";
	std::ofstream out_file2_(out_file_name2_.c_str(), std::ofstream::out);

	check_files(in_file_, in_file_name_, out_file_, out_file_name_);

	std::vector<MeasurementPackage> measurement_pack_list;//����ֵ���ݰ�
	std::vector<GroundTruthPackage> gt_pack_list;//��ʵֵ

	std::string line;

	// prep the measurement packages (each line represents a measurement at a
	// timestamp)
	double x;
	double y;
	double ro;
	double phi;
	double ro_dot;
	double x_gt;
	double y_gt;
	double vx_gt;
	double vy_gt;
	while (getline(in_file_, line)) {

		std::string sensor_type;
		MeasurementPackage meas_package;
		GroundTruthPackage gt_package;
		std::istringstream iss(line);
		long long timestamp;

		// reads first element from the current line
		iss >> sensor_type;
		//if (sensor_type.compare("L") == 0) {
		//	// LASER MEASUREMENT

		//	// read measurements at this timestamp
		//	meas_package.sensor_type_ = MeasurementPackage::LASER;
		//	meas_package.raw_measurements_ = Eigen::VectorXd(2);
		//	iss >> x;
		//	iss >> y;
		//	meas_package.raw_measurements_ << x, y;
		//	iss >> timestamp;
		//	meas_package.timestamp_ = static_cast<double>(timestamp)/ 1000000.0;
		//	measurement_pack_list.push_back(meas_package);

		//	// read ground truth data to compare later
		//	iss >> x_gt;
		//	iss >> y_gt;
		//	iss >> vx_gt;
		//	iss >> vy_gt;
		//	gt_package.gt_values_ = Eigen::VectorXd(4);
		//	gt_package.gt_values_ << x_gt, y_gt, vx_gt, vy_gt;
		//	gt_pack_list.push_back(gt_package);
		//}
		//else 
		if (sensor_type.compare("R") == 0) {
			// RADAR MEASUREMENT
			meas_package.sensor_type_ = MeasurementPackage::RADAR;
			meas_package.raw_measurements_ = Eigen::VectorXd(4);
			iss >> ro;
			iss >> phi;
			iss >> ro_dot;

			//���Ƕȹ�һ������-�У��С�
			while (phi > M_PI)
				phi -= DoublePI;
			while (phi < -M_PI)
				phi += DoublePI;
			meas_package.raw_measurements_ << ro * cos(phi), ro * sin(phi), ro_dot* cos(phi), ro_dot* sin(phi);
			iss >> timestamp;
			meas_package.timestamp_ = static_cast<double>(timestamp) / 1000000.0;
			measurement_pack_list.push_back(meas_package);

			// read ground truth data to compare later
			iss >> x_gt;
			iss >> y_gt;
			iss >> vx_gt;
			iss >> vy_gt;
			gt_package.gt_values_ = Eigen::VectorXd(4);
			gt_package.gt_values_ << x_gt, y_gt, vx_gt, vy_gt;
			gt_pack_list.push_back(gt_package);
		}

	}

	// Create a Fusion EKF instance
	KF_FUSION ekf;
	//EKF ekf;
	// used to compute the RMSE later
	std::vector<Eigen::VectorXd> estimations;
	std::vector<Eigen::VectorXd> ground_truth;

	//Call the EKF-based fusion
	size_t N = measurement_pack_list.size();
	for (size_t k = 0; k < N; ++k) {
		// start filtering from the second frame (the speed is unknown in the first
		// frame)
		ekf.ProcessMeasurement(measurement_pack_list[k]);
		Eigen::VectorXd x_t = Eigen::VectorXd(4);
		ekf.getState(x_t);
		double p_x = x_t(0);
		double p_y = x_t(1);
		double v_x = x_t(2);
		double v_y = x_t(3);

		VectorXd estimate(4);

		estimate(0) = p_x;
		estimate(1) = p_y;
		estimate(2) = v_x;
		estimate(3) = v_y;

		// output the estimation
		out_file_ << p_x << " ";
		out_file_ << p_y << " ";
		out_file_ << v_x << " ";
		out_file_ << v_y << " ";

		// output the measurements
		if (measurement_pack_list[k].sensor_type_ == MeasurementPackage::LASER) {
			out_file_ << measurement_pack_list[k].raw_measurements_(0) << " ";
			out_file_ << measurement_pack_list[k].raw_measurements_(1) << " ";
		}
		else if (measurement_pack_list[k].sensor_type_ == MeasurementPackage::RADAR) {
			double ro = measurement_pack_list[k].raw_measurements_(0);
			double phi = measurement_pack_list[k].raw_measurements_(1);
			out_file_ << measurement_pack_list[k].raw_measurements_(0) << " ";
			out_file_ << measurement_pack_list[k].raw_measurements_(1) << " ";
		}

		// output the ground truth packages
		out_file_ << gt_pack_list[k].gt_values_(0) << " ";
		out_file_ << gt_pack_list[k].gt_values_(1) << " ";
		out_file_ << gt_pack_list[k].gt_values_(2) << " ";
		out_file_ << gt_pack_list[k].gt_values_(3) << "\n";

		out_file2_ << k << " ";
		out_file2_ << p_x - gt_pack_list[k].gt_values_(0) << " ";
		out_file2_ << p_y - gt_pack_list[k].gt_values_(1) << " ";
		out_file2_ << v_x - gt_pack_list[k].gt_values_(2) << " ";
		out_file2_ << v_y - gt_pack_list[k].gt_values_(3) << "\n";

		estimations.push_back(estimate);
		ground_truth.push_back(gt_pack_list[k].gt_values_);
	}

	// compute the accuracy (RMSE)
	std::cout << "Accuracy - RMSE:" << std::endl << CalculateRMSE(estimations, ground_truth) << std::endl;

	// close files
	if (out_file_.is_open()) {
		out_file_.close();
	}
	if (out_file2_.is_open()) {
		out_file2_.close();
	}
	if (in_file_.is_open()) {
		in_file_.close();
	}

	return 0;
}

int EKFSimulation() {

	std::string in_file_name_ = "../data/data_synthetic.txt";
	std::ifstream in_file_(in_file_name_.c_str(), std::ifstream::in);

	std::string out_file_name_ = "../data/output.txt";
	std::ofstream out_file_(out_file_name_.c_str(), std::ofstream::out);

	std::string out_file_name2_ = "../data/output2.txt";
	std::ofstream out_file2_(out_file_name2_.c_str(), std::ofstream::out);

	check_files(in_file_, in_file_name_, out_file_, out_file_name_);

	std::vector<MeasurementPackage> measurement_pack_list;//����ֵ���ݰ�
	std::vector<GroundTruthPackage> gt_pack_list;//��ʵֵ

	std::string line;

	// prep the measurement packages (each line represents a measurement at a
	// timestamp)
	double x;
	double y;
	double ro;
	double phi;
	double ro_dot;
	double x_gt;
	double y_gt;
	double vx_gt;
	double vy_gt;
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
			meas_package.timestamp_ = static_cast<double>(timestamp) / 1000000.0;
			measurement_pack_list.push_back(meas_package);
			// read ground truth data to compare later
			iss >> x_gt;
			iss >> y_gt;
			iss >> vx_gt;
			iss >> vy_gt;
			gt_package.gt_values_ = Eigen::VectorXd(4);
			gt_package.gt_values_ << x_gt, y_gt, vx_gt, vy_gt;
			gt_pack_list.push_back(gt_package);
		}
		else if (sensor_type.compare("R") == 0) {
				// RADAR MEASUREMENT
				meas_package.sensor_type_ = MeasurementPackage::RADAR;
				meas_package.raw_measurements_ = Eigen::VectorXd(3);
				iss >> ro;
				iss >> phi;
				iss >> ro_dot;
				meas_package.raw_measurements_ << ro, phi, ro_dot;
				iss >> timestamp;
				meas_package.timestamp_ = static_cast<double>(timestamp) / 1000000.0;
				measurement_pack_list.push_back(meas_package);
				// read ground truth data to compare later
				iss >> x_gt;
				iss >> y_gt;
				iss >> vx_gt;
				iss >> vy_gt;
				gt_package.gt_values_ = Eigen::VectorXd(4);
				gt_package.gt_values_ << x_gt, y_gt, vx_gt, vy_gt;
				gt_pack_list.push_back(gt_package);
			}
	}

	// Create a Fusion EKF instance
	EKF_CTRV ekf;
	//EKF ekf;
	// used to compute the RMSE later
	std::vector<Eigen::VectorXd> estimations;
	std::vector<Eigen::VectorXd> ground_truth;

	//Call the EKF-based fusion
	size_t N = measurement_pack_list.size();
	for (size_t k = 0; k < N; ++k) {
		// start filtering from the second frame (the speed is unknown in the first
		// frame)
		ekf.ProcessMeasurement(measurement_pack_list[k]);
		Eigen::VectorXd x_t = Eigen::VectorXd(6);
		ekf.getState(x_t);
		double p_x = x_t(0);
		double p_y = x_t(1);
		double v_x = x_t(2);
		double v_y = x_t(3);
		double theta = x_t(5);

		VectorXd estimate(4);

		estimate(0) = p_x;
		estimate(1) = p_y;
		estimate(2) = v_x;
		estimate(3) = v_y;

		// output the estimation
		out_file_ << p_x << " ";
		out_file_ << p_y << " ";
		out_file_ << v_x << " ";
		out_file_ << v_y << " ";

		// output the measurements
		if (measurement_pack_list[k].sensor_type_ == MeasurementPackage::LASER) {
			out_file_ << measurement_pack_list[k].raw_measurements_(0) << " ";
			out_file_ << measurement_pack_list[k].raw_measurements_(1) << " ";
		}
		else if (measurement_pack_list[k].sensor_type_ == MeasurementPackage::RADAR) {
			double ro = measurement_pack_list[k].raw_measurements_(0);
			double phi = measurement_pack_list[k].raw_measurements_(1);
			out_file_ << ro*cos(phi) << " ";
			out_file_ << ro*sin(phi) << " ";
			//out_file_ << measurement_pack_list[k].raw_measurements_(0) << " ";
			//out_file_ << measurement_pack_list[k].raw_measurements_(1) << " ";
		}

		// output the ground truth packages
		out_file_ << gt_pack_list[k].gt_values_(0) << " ";
		out_file_ << gt_pack_list[k].gt_values_(1) << " ";
		out_file_ << gt_pack_list[k].gt_values_(2) << " ";
		out_file_ << gt_pack_list[k].gt_values_(3) << "\n";

		out_file2_ << k << " ";
		out_file2_ << p_x - gt_pack_list[k].gt_values_(0) << " ";
		out_file2_ << p_y - gt_pack_list[k].gt_values_(1) << " ";
		out_file2_ << v_x - gt_pack_list[k].gt_values_(2) << " ";
		out_file2_ << v_y - gt_pack_list[k].gt_values_(3) << " ";
		out_file2_ << theta << "\n";

		estimations.push_back(estimate);
		ground_truth.push_back(gt_pack_list[k].gt_values_);
	}

	// compute the accuracy (RMSE)
	std::cout << "Accuracy - RMSE:" << std::endl << CalculateRMSE(estimations, ground_truth) << std::endl;

	// close files
	if (out_file_.is_open()) {
		out_file_.close();
	}
	if (out_file2_.is_open()) {
		out_file2_.close();
	}
	if (in_file_.is_open()) {
		in_file_.close();
	}

	return 0;
}

int lidar_kf() {

	std::string in_file_name_ = "../data/Trajectory-lidar.txt";
	std::ifstream in_file_(in_file_name_.c_str(), std::ifstream::in);

	std::string out_file_name_ = "../data/output.txt";
	std::ofstream out_file_(out_file_name_.c_str(), std::ofstream::out);

	std::string out_file_name2_ = "../data/output2.txt";
	std::ofstream out_file2_(out_file_name2_.c_str(), std::ofstream::out);

	check_files(in_file_, in_file_name_, out_file_, out_file_name_);

	std::vector<MeasurementPackage> measurement_pack_list;//����ֵ���ݰ�
	std::vector<GroundTruthPackage> gt_pack_list;//��ʵֵ

	std::string line;

	// prep the measurement packages (each line represents a measurement at a
	// timestamp)
	double x;
	double y;
	double ro;
	double phi;
	double ro_dot;
	double x_gt;
	double y_gt;
	double vx_gt;
	double vy_gt;
	while (getline(in_file_, line)) {

		std::string sensor_type;
		MeasurementPackage meas_package;
		GroundTruthPackage gt_package;
		std::istringstream iss(line);
		double timestamp;

		// reads first element from the current line
//		iss >> sensor_type;
//		if (sensor_type.compare("L") == 0) {
			// LASER MEASUREMENT

		// read measurements at this timestamp
		meas_package.sensor_type_ = MeasurementPackage::LASER;
		meas_package.raw_measurements_ = Eigen::VectorXd(2);

		//iss >> x;
		//iss >> x;
		//iss >> x;

		iss >> x;
		iss >> y;
		meas_package.raw_measurements_ << x , y;
		iss >> timestamp;
		iss >> timestamp;
		meas_package.timestamp_ = timestamp;
		measurement_pack_list.push_back(meas_package);
			// read ground truth data to compare later
			//iss >> x_gt;
			//iss >> y_gt;
			//iss >> vx_gt;
			//iss >> vy_gt;
			//gt_package.gt_values_ = Eigen::VectorXd(4);
			//gt_package.gt_values_ << x_gt, y_gt, vx_gt, vy_gt;
			//gt_pack_list.push_back(gt_package);
		//}
		//else
		//	if (sensor_type.compare("R") == 0) {
		//		// RADAR MEASUREMENT
		//		meas_package.sensor_type_ = MeasurementPackage::RADAR;
		//		meas_package.raw_measurements_ = Eigen::VectorXd(3);
		//		iss >> ro;
		//		iss >> phi;
		//		iss >> ro_dot;
		//		meas_package.raw_measurements_ << ro, phi, ro_dot;
		//		iss >> timestamp;
		//		meas_package.timestamp_ = timestamp;
		//		measurement_pack_list.push_back(meas_package);
		//		// read ground truth data to compare later
		//		iss >> x_gt;
		//		iss >> y_gt;
		//		iss >> vx_gt;
		//		iss >> vy_gt;
		//		gt_package.gt_values_ = Eigen::VectorXd(4);
		//		gt_package.gt_values_ << x_gt, y_gt, vx_gt, vy_gt;
		//		gt_pack_list.push_back(gt_package);
		//	}
	}

	// Create a Fusion EKF instance
	EKF_CTRV ekf;
	//EKF ekf;
	// used to compute the RMSE later
	std::vector<Eigen::VectorXd> estimations;
	std::vector<Eigen::VectorXd> ground_truth;

	//Call the EKF-based fusion
	size_t N = measurement_pack_list.size();
	for (size_t k = 0; k < N; ++k) {
		// start filtering from the second frame (the speed is unknown in the first
		// frame)
		ekf.ProcessMeasurement(measurement_pack_list[k]);
		Eigen::VectorXd x_t = Eigen::VectorXd(5);
		ekf.getState(x_t);
		double p_x = x_t(0);
		double p_y = x_t(1);
		double v_x = x_t(2);
		double v_y = x_t(3);
		double theta = x_t(4);

		VectorXd estimate(4);

		estimate(0) = p_x;
		estimate(1) = p_y;
		estimate(2) = v_x;
		estimate(3) = v_y;

		// output the estimation
		out_file_ << k << " ";
		out_file_ << p_x << " ";
		out_file_ << p_y << " ";


		// output the measurements
		if (measurement_pack_list[k].sensor_type_ == MeasurementPackage::LASER) {
			out_file_ << measurement_pack_list[k].raw_measurements_(0) << " ";
			out_file_ << measurement_pack_list[k].raw_measurements_(1) << " ";
		}
		else if (measurement_pack_list[k].sensor_type_ == MeasurementPackage::RADAR) {
			double ro = measurement_pack_list[k].raw_measurements_(0);
			double phi = measurement_pack_list[k].raw_measurements_(1);
			out_file_ << ro*cos(phi) << " ";
			out_file_ << ro*sin(phi) << " ";
		}
		out_file_ << v_x << " ";
		out_file_ << v_y << " ";
		out_file_ << theta << "\n";
		// output the ground truth packages
		//out_file_ << gt_pack_list[k].gt_values_(0) << " ";
		//out_file_ << gt_pack_list[k].gt_values_(1) << " ";
		//out_file_ << gt_pack_list[k].gt_values_(2) << " ";
		//out_file_ << gt_pack_list[k].gt_values_(3) << "\n";
		//out_file2_ << k << " ";
		//out_file2_ << p_x - gt_pack_list[k].gt_values_(0) << " ";
		//out_file2_ << p_y - gt_pack_list[k].gt_values_(1) << " ";
		//out_file2_ << v_x - gt_pack_list[k].gt_values_(2) << " ";
		//out_file2_ << v_y - gt_pack_list[k].gt_values_(3) << " ";
		//out_file2_ << theta << "\n";
		//estimations.push_back(estimate);
		//ground_truth.push_back(gt_pack_list[k].gt_values_);
	}

	// compute the accuracy (RMSE)
	//std::cout << "Accuracy - RMSE:" << std::endl << CalculateRMSE(estimations, ground_truth) << std::endl;

	// close files
	if (out_file_.is_open()) {
		out_file_.close();
	}
	if (out_file2_.is_open()) {
		out_file2_.close();
	}
	if (in_file_.is_open()) {
		in_file_.close();
	}

	return 0;
}

int radar_EKF() {

	std::string in_file_name_ = "../data/Trajectory-radar-3-1.txt";
	std::ifstream in_file_(in_file_name_.c_str(), std::ifstream::in);

	std::string out_file_name_ = "../data/output.txt";
	std::ofstream out_file_(out_file_name_.c_str(), std::ofstream::out);

	std::string out_file_name2_ = "../data/output2.txt";
	std::ofstream out_file2_(out_file_name2_.c_str(), std::ofstream::out);

	check_files(in_file_, in_file_name_, out_file_, out_file_name_);

	std::vector<MeasurementPackage> measurement_pack_list;//����ֵ���ݰ�
	std::vector<GroundTruthPackage> gt_pack_list;//��ʵֵ

	std::string line;

	// prep the measurement packages (each line represents a measurement at a
	// timestamp)
	double x;
	double y;
	double ro;
	double phi;
	double ro_dot;
	double x_gt;
	double y_gt;
	double vx_gt;
	double vy_gt;
	while (getline(in_file_, line)) {

		std::string sensor_type;
		MeasurementPackage meas_package;
		GroundTruthPackage gt_package;
		std::istringstream iss(line);
		double timestamp;

		meas_package.sensor_type_ = MeasurementPackage::RADAR;
		meas_package.raw_measurements_ = Eigen::VectorXd(3);

		iss >> x;
		iss >> x;
		iss >> x;
		iss >> x;
		iss >> y;

		iss >> ro_dot;
		iss >> phi;
		iss >> ro;

		meas_package.raw_measurements_ << ro, phi*M_PI, ro_dot;
		iss >> timestamp;
		meas_package.timestamp_ = timestamp;
		measurement_pack_list.push_back(meas_package);
	}

	// Create a Fusion EKF instance
	EKF_CTRV ekf;
	//EKF ekf;
	// used to compute the RMSE later
	std::vector<Eigen::VectorXd> estimations;
	std::vector<Eigen::VectorXd> ground_truth;

	//Call the EKF-based fusion
	size_t N = measurement_pack_list.size();
	for (size_t k = 0; k < N; ++k) {
		// start filtering from the second frame (the speed is unknown in the first
		// frame)
		ekf.ProcessMeasurement(measurement_pack_list[k]);
		Eigen::VectorXd x_t = Eigen::VectorXd(5);
		ekf.getState(x_t);
		double p_x = x_t(0);
		double p_y = x_t(1);
		double v_x = x_t(2);
		double v_y = x_t(3);
		double theta = x_t(4);

		VectorXd estimate(4);

		estimate(0) = p_x;
		estimate(1) = p_y;
		estimate(2) = v_x;
		estimate(3) = v_y;

		// output the estimation
		out_file_ << k << " ";
		out_file_ << p_x << " ";
		out_file_ << p_y << " ";


		// output the measurements
		if (measurement_pack_list[k].sensor_type_ == MeasurementPackage::LASER) {
			out_file_ << measurement_pack_list[k].raw_measurements_(0) << " ";
			out_file_ << measurement_pack_list[k].raw_measurements_(1) << " ";
		}
		else if (measurement_pack_list[k].sensor_type_ == MeasurementPackage::RADAR) {
			double ro = measurement_pack_list[k].raw_measurements_(0);
			double phi = measurement_pack_list[k].raw_measurements_(1);
			out_file_ << ro*cos(phi) << " ";
			out_file_ << ro*sin(phi) << " ";
		}
		out_file_ << v_x << " ";
		out_file_ << v_y << " ";
		out_file_ << theta << "\n";

	}

	// compute the accuracy (RMSE)
	//std::cout << "Accuracy - RMSE:" << std::endl << CalculateRMSE(estimations, ground_truth) << std::endl;

	// close files
	if (out_file_.is_open()) {
		out_file_.close();
	}
	if (out_file2_.is_open()) {
		out_file2_.close();
	}
	if (in_file_.is_open()) {
		in_file_.close();
	}

	return 0;
}
int UKFMeasured() {

	std::string in_file_name_ = "../data/Trajectory_9.04_car2.txt";
	std::ifstream in_file_(in_file_name_.c_str(), std::ifstream::in);

	std::string out_file_name_ = "../data/output.txt";
	std::ofstream out_file_(out_file_name_.c_str(), std::ofstream::out);

	check_files(in_file_, in_file_name_, out_file_, out_file_name_);

	std::vector<MeasurementPackage> measurement_pack_list;//����ֵ���ݰ�
	std::vector<GroundTruthPackage> gt_pack_list;//��ʵֵ

	std::string line;

	// prep the measurement packages (each line represents a measurement at a
	// timestamp)
	double x;
	double y;
	double ro;
	double phi;
	double ro_dot;
	double timestamp;
	double tmp;
	int FramNum = 0;
	int type = 0;
	while (getline(in_file_, line)) {

		std::string sensor_type;
		MeasurementPackage meas_package;
		GroundTruthPackage gt_package;
		std::istringstream iss(line);

		//ȫ������
		iss >> x;
		iss >> x;
		iss >> x;
		//�ֲ�����
		iss >> x;
		iss >> y;
		//�״����ֵ
		iss >> ro_dot;
		iss >> phi;
		iss >> ro;


		//ʱ��
		iss >> timestamp;
		iss >> tmp;
		FramNum = static_cast<int>(tmp);
		iss >> tmp;
		type = static_cast<int>(tmp);

		if (type == 1) {
			meas_package.sensor_type_ = MeasurementPackage::LASER;
			meas_package.raw_measurements_ = Eigen::VectorXd(2);
			meas_package.raw_measurements_ << x, y;
			meas_package.timestamp_ = timestamp;
			measurement_pack_list.push_back(meas_package);
		}
		else if (type == 2) {
			meas_package.sensor_type_ = MeasurementPackage::RADAR;
			meas_package.raw_measurements_ = Eigen::VectorXd(3);
			meas_package.raw_measurements_ << ro, phi, ro_dot;
			meas_package.timestamp_ = timestamp;
			measurement_pack_list.push_back(meas_package);
		}
	}

	// Create a Fusion EKF instance
	UKF ukf;

	//Call the EKF-based fusion
	size_t N = measurement_pack_list.size();
	for (size_t k = 0; k < N; ++k) {
		// start filtering from the second frame (the speed is unknown in the first
		// frame)
		std::cout << k << std::endl;
		ukf.ProcessMeasurement(measurement_pack_list[k]);
		Eigen::VectorXd x_t = Eigen::VectorXd(5);
		ukf.getState(x_t);
		double p_x = x_t(0);
		double p_y = x_t(1);
		double v_x = x_t(2);
		double v_y = x_t(3);
		double theta = x_t(4);

		out_file_ << k << " ";
		out_file_ << p_x << " ";
		out_file_ << p_y << " ";

		// output the measurements
		if (measurement_pack_list[k].sensor_type_ == MeasurementPackage::LASER) {
			out_file_ << measurement_pack_list[k].raw_measurements_(0) << " ";
			out_file_ << measurement_pack_list[k].raw_measurements_(1) << " ";
		}
		else if (measurement_pack_list[k].sensor_type_ == MeasurementPackage::RADAR) {
			double ro = measurement_pack_list[k].raw_measurements_(0);
			double phi = measurement_pack_list[k].raw_measurements_(1);
			out_file_ << ro*cos(phi) << " ";
			out_file_ << ro*sin(phi) << " ";
		}
		out_file_ << v_x << " ";
		out_file_ << v_y << " ";
		out_file_ << theta << "\n";
	}

	// close files
	if (out_file_.is_open()) {
		out_file_.close();
	}
	if (in_file_.is_open()) {
		in_file_.close();
	}

	return 0;
}


int EKFMeasured() {

	std::string in_file_name_ = "../data/1_obstacle_R.txt";
	std::ifstream in_file_(in_file_name_.c_str(), std::ifstream::in);

	std::string out_file_name_ = "../data/output.txt";
	std::ofstream out_file_(out_file_name_.c_str(), std::ofstream::out);
	out_file_.precision(18);
	check_files(in_file_, in_file_name_, out_file_, out_file_name_);

	std::vector<MeasurementPackage> measurement_pack_list;//����ֵ���ݰ�
	std::vector<GroundTruthPackage> gt_pack_list;//��ʵֵ

	std::string line;

	// prep the measurement packages (each line represents a measurement at a
	// timestamp)
	double x;
	double y;
	double ro;
	double phi;
	double ro_dot;
	double timestamp;
	double tmp;
	int FramNum=0;
	int type=0;
	while (getline(in_file_, line)) {

		std::string sensor_type;
		MeasurementPackage meas_package;
		GroundTruthPackage gt_package;
		std::istringstream iss(line);

		//ȫ������
		iss >> x;
		iss >> y;
		iss >> tmp; 

		////�ֲ�����
		iss >> x; iss >> y; iss >> tmp;
		//�״����ֵ
		iss >> ro_dot;
		iss >> phi;
		iss >> ro;
		iss >> tmp;
		
		//ʱ��
		iss >> timestamp;
		iss >> tmp;
		FramNum = static_cast<int>(tmp);
		iss >> tmp;
		type = static_cast<int>(tmp);

		if (type == 1) {
			meas_package.sensor_type_ = MeasurementPackage::LASER;
			meas_package.raw_measurements_ = Eigen::VectorXd(2);
			meas_package.raw_measurements_ << x, y;
			meas_package.timestamp_ = timestamp;
			measurement_pack_list.push_back(meas_package);
		}
		else if (type == 2) {
			meas_package.sensor_type_ = MeasurementPackage::RADAR;
			meas_package.raw_measurements_ = Eigen::VectorXd(3);
			meas_package.raw_measurements_ << ro, phi, ro_dot;
			//meas_package.raw_measurements_ << x, y, ro_dot,phi;
			meas_package.timestamp_ = timestamp;
			measurement_pack_list.push_back(meas_package);
			}
	}

	// Create a Fusion EKF instance
	EKF_CTRV ekf;

	//Call the EKF-based fusion
	size_t N = measurement_pack_list.size();
	for (size_t k = 0; k < N; ++k) {
		// start filtering from the second frame (the speed is unknown in the first
		// frame)
		ekf.ProcessMeasurement(measurement_pack_list[k]);
		Eigen::VectorXd x_t = Eigen::VectorXd(6);
		ekf.getState(x_t);
		double p_x = x_t(0);
		double p_y = x_t(1);
		double v_x = x_t(2);
		double v_y = x_t(3);
		double v = x_t(4);
		double theta = x_t(5);

		out_file_ << k << " ";
		out_file_ << p_x << " ";
		out_file_ << p_y << " ";

		// output the measurements
		if (measurement_pack_list[k].sensor_type_ == MeasurementPackage::LASER) {
			out_file_ << measurement_pack_list[k].raw_measurements_(0) << " ";
			out_file_ << measurement_pack_list[k].raw_measurements_(1) << " ";
		}
		else if (measurement_pack_list[k].sensor_type_ == MeasurementPackage::RADAR) {
			double ro = measurement_pack_list[k].raw_measurements_(0);
			double phi = measurement_pack_list[k].raw_measurements_(1);
			out_file_ << ro*cos(phi) << " ";
			out_file_ << ro*sin(phi) << " ";
			//out_file_ << measurement_pack_list[k].raw_measurements_(0) << " ";
			//out_file_ << measurement_pack_list[k].raw_measurements_(1) << " ";
		}
		out_file_ << v_x << " ";
		out_file_ << v_y << " ";
		out_file_ << v << " ";
		out_file_ << theta << "\n";
	}

	// close files
	if (out_file_.is_open()) {
		out_file_.close();
	}
	if (in_file_.is_open()) {
		in_file_.close();
	}

	return 0;
}


int EKFGEOMeasured() {

	std::string in_file_name_ = "../data/1_obstacle.txt";
	std::ifstream in_file_(in_file_name_.c_str(), std::ifstream::in);

	std::string out_file_name_ = "../data/output.txt";
	std::ofstream out_file_(out_file_name_.c_str(), std::ofstream::out);
	out_file_.precision(18);
	check_files(in_file_, in_file_name_, out_file_, out_file_name_);

	std::vector<MeasurementPackage> measurement_pack_list;//����ֵ���ݰ�
	std::vector<GroundTruthPackage> gt_pack_list;//��ʵֵ

	std::string line;

	// prep the measurement packages (each line represents a measurement at a
	// timestamp)
	double x;
	double y;
	double yaw;
	double ro;
	double phi;
	double ro_dot;
	double timestamp;
	double tmp;
	int FramNum = 0;
	int type = 0;
	while (getline(in_file_, line)) {

		std::string sensor_type;
		MeasurementPackage meas_package;
		GroundTruthPackage gt_package;
		std::istringstream iss(line);

		//ȫ������
		iss >> x;
		iss >> y;
		iss >> tmp;

		////�ֲ�����
		iss >> tmp; iss >> tmp;
		//�״����ֵ
		iss >> yaw;//�۲��ߵķ�λ��
		iss >> ro_dot;
		iss >> phi;
		iss >> ro;
		iss >> ro_dot;

		//ʱ��
		iss >> timestamp;
		iss >> tmp;
		FramNum = static_cast<int>(tmp);
		iss >> tmp;
		type = static_cast<int>(tmp);

		if (type == 1) {
			meas_package.sensor_type_ = MeasurementPackage::LASER;
			meas_package.raw_measurements_ = Eigen::VectorXd(2);
			meas_package.raw_measurements_ << x - 252800, y - 3373000;
			meas_package.timestamp_ = timestamp;
			measurement_pack_list.push_back(meas_package);
		}
		else if (type == 2) {
			meas_package.sensor_type_ = MeasurementPackage::RADAR;
			meas_package.raw_measurements_ = Eigen::VectorXd(4);
			meas_package.raw_measurements_ << x, y, ro_dot,yaw;
			meas_package.timestamp_ = timestamp;
			measurement_pack_list.push_back(meas_package);
		}
	}

	// Create a Fusion EKF instance
	EKF_GEO ekf;

	//Call the EKF-based fusion
	size_t N = measurement_pack_list.size();
	for (size_t k = 0; k < N; ++k) {
		// start filtering from the second frame (the speed is unknown in the first
		// frame)
		ekf.ProcessMeasurement(measurement_pack_list[k]);
		Eigen::VectorXd x_t = Eigen::VectorXd(6);
		ekf.getState(x_t);
		double p_x = x_t(0);
		double p_y = x_t(1);
		double v_x = x_t(2);
		double v_y = x_t(3);
		double v = x_t(4);
		double theta = x_t(5);

		out_file_ << k << " ";
		out_file_ << p_x << " ";
		out_file_ << p_y << " ";

		// output the measurements
		if (measurement_pack_list[k].sensor_type_ == MeasurementPackage::LASER) {
			out_file_ << measurement_pack_list[k].raw_measurements_(0) << " ";
			out_file_ << measurement_pack_list[k].raw_measurements_(1) << " ";
		}
		else if (measurement_pack_list[k].sensor_type_ == MeasurementPackage::RADAR) {
			//double ro = measurement_pack_list[k].raw_measurements_(0);
			//double phi = measurement_pack_list[k].raw_measurements_(1);
			//out_file_ << ro*cos(phi) << " ";
			//out_file_ << ro*sin(phi) << " ";
			out_file_ << measurement_pack_list[k].raw_measurements_(0) << " ";
			out_file_ << measurement_pack_list[k].raw_measurements_(1) << " ";
		}
		out_file_ << v_x << " ";
		out_file_ << v_y << " ";
		out_file_ << v << " ";
		out_file_ << theta << "\n";
	}

	// close files
	if (out_file_.is_open()) {
		out_file_.close();
	}
	if (in_file_.is_open()) {
		in_file_.close();
	}

	return 0;
}
int main()
{
	//radar_EKF();
	EKFSimulation();
	//lidar_kf();
	//EKFMeasured();
	//UKFMeasured();
	return 0;
}