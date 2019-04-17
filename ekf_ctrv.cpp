#include "ekf_ctrv.h"
#include <iostream>


EKF_CTRV::EKF_CTRV() {
	is_initialized_ = false;
	previous_timestamp_ = 0;


	H_laser_ = Eigen::MatrixXd(2, 5);
	H_laser_ << 1.0, 0.0, 0.0, 0.0, 0.0,
		0.0, 1.0, 0.0, 0.0, 0.0;
	double std_laspx_ = 0.15;
	double std_laspy_ = 0.15;
	R_laser_ = Eigen::MatrixXd(2, 2);
	/*����ֵ�Ĳ�ȷ����*/
	R_laser_ << std_laspx_*std_laspx_, 0.0,
		0.0, std_laspy_*std_laspy_;


	double std_radrho_ = 0.02;
	double std_radphi_ = 0.01;
	double std_radrhodot_ = 0.2;
	R_radar_ = Eigen::MatrixXd(3, 3);
	/*����ֵ�Ĳ�ȷ����*/
	R_radar_ << std_radrho_*std_radrho_, 0.0, 0.0,
		0.0, std_radphi_*std_radphi_, 0.0,
		0.0, 0.0, std_radrhodot_*std_radrhodot_;


	//״̬����
	x_ = Eigen::VectorXd(5);
	//ϵͳ״̬��ȷ����
	P_ = Eigen::MatrixXd(5, 5);
	double px = 0.25, py = 0.25, pv = 0.3, ptheta = 0.2, pomiga = 1.0;
	P_ << px, 0.0, 0.0, 0.0, 0.0,
		0.0, py, 0.0, 0.0, 0.0,
		0.0, 0.0, pv, 0.0, 0.0,
		0.0, 0.0, 0.0, ptheta, 0.0,
		0.0, 0.0, 0.0, 0.0, pomiga;
	initial();
	/*����Ҫ״̬ת�ƾ���*/
	Q_ = Eigen::MatrixXd(5, 5);//״̬Э�������
	JA_ = Eigen::MatrixXd(5, 5);//״̬ת�ƾ�����ſ˱Ⱦ���
	HJ_ = Eigen::MatrixXd(3, 5);//Ԥ��ռ䵽�����ռ���ſ˱Ⱦ���
	std_a_ = 2.0;
	std_yawdd_ = 0.3;
}

EKF_CTRV::~EKF_CTRV() {}

void EKF_CTRV::initial()
{
	std::string in_file_name_ = "../config.txt";
	std::ifstream in_file_(in_file_name_.c_str(), std::ifstream::in);
	if (!in_file_.is_open()) {
		std::cerr << "Cannot open input file: " << in_file_name_ << std::endl;
	}
	std::string line;

	double std_laspx_ = 0.15;
	double std_laspy_ = 0.15;

	double std_radrho_ = 0.02;
	double std_radphi_ = 0.01;
	double std_radrhodot_ = 0.2;


	double px = 0.25, py = 0.25, pv = 0.3, ptheta = 0.2, pomiga = 1.0;

	while (getline(in_file_, line))
	{
		std::istringstream iss(line);
		std::string data_type;
		iss >> data_type;
		if (data_type.compare("std_laspx_") == 0) {
			iss >> std_laspx_;
			continue;
		}
		if (data_type.compare("std_laspy_") == 0) {
			iss >> std_laspy_;
			continue;
		}
		if (data_type.compare("std_radrho_") == 0) {
			iss >> std_radrho_;
			continue;
		}
		if (data_type.compare("std_radphi_") == 0) {
			iss >> std_radphi_;
			continue;
		}
		if (data_type.compare("std_radrhodot_") == 0) {
			iss >> std_radrhodot_;
			continue;
		}
		if (data_type.compare("px") == 0) {
			iss >> px;
			continue;
		}
		if (data_type.compare("py") == 0) {
			iss >> py;
			continue;
		}
		if (data_type.compare("pv") == 0) {
			iss >> pv;
			continue;
		}
		if (data_type.compare("ptheta") == 0) {
			iss >> ptheta;
			continue;
		}
		if (data_type.compare("pomiga") == 0) {
			iss >> pomiga;
			continue;
		}
	}


	/*����ֵ�Ĳ�ȷ����*/
	R_laser_ << std_laspx_*std_laspx_, 0.0,
		0.0, std_laspy_*std_laspy_;
	/*����ֵ�Ĳ�ȷ����*/
	R_radar_ << std_radrho_*std_radrho_, 0.0, 0.0,
		0.0, std_radphi_*std_radphi_, 0.0,
		0.0, 0.0, std_radrhodot_*std_radrhodot_;
	P_ << px, 0.0, 0.0, 0.0, 0.0,
		0.0, py, 0.0, 0.0, 0.0,
		0.0, 0.0, pv, 0.0, 0.0,
		0.0, 0.0, 0.0, ptheta, 0.0,
		0.0, 0.0, 0.0, 0.0, pomiga;
}
void EKF_CTRV::StateTransition(double delta_t)
{
	float x = x_[0];
	float y = x_[1];
	double v = x_[2];
	double theta = x_[3];
	double omiga = x_[4];
	double tmpTheta = omiga*delta_t + theta;
	if (abs(omiga) > 0.0001)
	{

		double v_omiga = v / omiga;
		x_[0] = v_omiga*(sin(tmpTheta) - sin(theta)) + x;//x
		x_[1] = v_omiga*(-cos(tmpTheta) + cos(theta)) + y;//y
		x_[2] = v;								   //����ٶ�
		x_[3] = control_psi(tmpTheta);						  //ƫ����
		x_[4] = omiga;							   //ƫ�����ٶ�
	}
	else
	{
		x_[0] = v*cos(theta)*delta_t + x;//x
		x_[1] = v*sin(theta)*delta_t + y;//y
		x_[2] = v;								   //����ٶ�
		x_[3] = theta;                          //ƫ����
		x_[4] = 0.0000001;							   //ƫ�����ٶ�
	}

}

void EKF_CTRV::ProcessQMatrix(double delta_t)
{
	double delta_t2 = delta_t*delta_t;
	double delta_t3 = delta_t2*delta_t;
	double delta_t4 = delta_t3*delta_t;
	double std_a_2 = std_a_*std_a_;
	double std_yawdd_2 = std_yawdd_*std_yawdd_;
	double theta = x_[3];
	//����Q_����
	Q_ = Eigen::MatrixXd(5, 5);

	double q11 = 0.25*delta_t4*std_a_2*cos(theta)*cos(theta);
	double q12 = 0.25*delta_t4*std_a_2*sin(theta)*cos(theta);
	double q13 = 0.5*delta_t3*std_a_2*cos(theta);
	double q14 = 0;
	double q15 = 0;

	double q21 = 0.25*delta_t4*std_a_2*sin(theta)*cos(theta);
	double q22 = 0.25*delta_t4*std_a_2*sin(theta)*sin(theta);
	double q23 = 0.5*delta_t3*std_a_2*sin(theta);
	double q24 = 0;
	double q25 = 0;

	double q31 = 0.5*delta_t3*std_a_2*cos(theta);
	double q32 = 0.5*delta_t3*std_a_2*sin(theta);
	double q33 = delta_t2*std_a_2;
	double q34 = 0;
	double q35 = 0;

	double q41 = 0;
	double q42 = 0;
	double q43 = 0;
	double q44 = 0.25*delta_t4*std_yawdd_2;
	double q45 = 0.5*delta_t3*std_yawdd_2;

	double q51 = 0;
	double q52 = 0;
	double q53 = 0;
	double q54 = 0.5*delta_t3*std_yawdd_2;
	double q55 = delta_t2*std_yawdd_2;
	Q_ << q11, q12, q13, q14, q15,
		q21, q22, q23, q24, q25,
		q31, q32, q33, q34, q35,
		q41, q42, q43, q44, q45,
		q51, q52, q53, q54, q55;
}

void EKF_CTRV::ProcessJAMatrix(double delta_t)
{
	double v = x_[2];
	double theta = x_[3];
	double omiga = x_[4];
	JA_ = Eigen::MatrixXd(5, 5);//״̬ת�ƾ�����ſ˱Ⱦ���

	double q11, q12, q13, q14, q15,
		q21, q22, q23, q24, q25;

	if (abs(omiga) > 0.0001)
	{
		double tmpTheta = omiga*delta_t + theta;
		double v_omiga = v / omiga;
		double _1_omiga = 1.0 / omiga;
		double delta_t_v_omiga = delta_t*v_omiga;
		double v_omiga2 = v / omiga / omiga;
		q11 = 1;
		q12 = 0;
		q13 = _1_omiga*(-sin(theta) + sin(tmpTheta));
		q14 = v_omiga*(-cos(theta) + cos(tmpTheta));
		q15 = delta_t_v_omiga*cos(tmpTheta) - v_omiga2*(-sin(theta) + sin(tmpTheta));

		q21 = 0;
		q22 = 1;
		q23 = _1_omiga*(cos(theta) - cos(tmpTheta));
		q24 = v_omiga*(-sin(theta) + sin(tmpTheta));
		q25 = delta_t_v_omiga*sin(tmpTheta) - v_omiga2*(cos(theta) - cos(tmpTheta));
	}
	else
	{
		q11 = 1;
		q12 = 0;
		q13 = delta_t*cos(theta);
		q14 = -delta_t*v*sin(theta);
		q15 = 0;

		q21 = 0;
		q22 = 1;
		q23 = delta_t*sin(theta);
		q24 = delta_t*v*cos(theta);;
		q25 = 0;
	}
	double q31 = 0.0;
	double q32 = 0.0;
	double q33 = 1.0;
	double q34 = 0.0;
	double q35 = 0.0;

	double q41 = 0.0;
	double q42 = 0.0;
	double q43 = 0.0;
	double q44 = 1.0;
	double q45 = delta_t;

	double q51 = 0.0;
	double q52 = 0.0;
	double q53 = 0.0;
	double q54 = 0.0;
	double q55 = 1.0;
	JA_ << q11, q12, q13, q14, q15,
		q21, q22, q23, q24, q25,
		q31, q32, q33, q34, q35,
		q41, q42, q43, q44, q45,
		q51, q52, q53, q54, q55;
}

void EKF_CTRV::Predict(double delta_t)
{
	/*״̬ת��*/
	StateTransition(delta_t);
	/*����Q��*/
	ProcessQMatrix(delta_t);
	/*����JA����*/
	ProcessJAMatrix(delta_t);
	/*����Ԥ��*/
	P_ = JA_*P_*JA_.transpose() + Q_;
}

void EKF_CTRV::getState(Eigen::VectorXd& x)
{
	x[0] = x_[0];
	x[1] = x_[1];
	double v = x_[2];
	double theta = x_[3];// control_psi(x_[3]);;
	x[2] = v*cos(theta);
	x[3] = v*sin(theta);
	x[4] = v;
	x[5] = theta;
}

double EKF_CTRV::control_psi(double phi)
{
	while ((phi > M_PI) || (phi < -M_PI))
	{
		if (phi > M_PI)
			phi -= DoublePI;
		if (phi < -M_PI)
			phi += DoublePI;
	}
	return phi;
}

void EKF_CTRV::Update(const Eigen::VectorXd &z)
{
	//���ڿ�������״̬���и���
	Eigen::MatrixXd HT = H_.transpose();
	Eigen::MatrixXd S = H_*P_*HT + R_;
	Eigen::MatrixXd Si = S.inverse();
	Eigen::MatrixXd PHT = P_*HT;
	Eigen::MatrixXd K = PHT*Si;

	Eigen::VectorXd z_pred = H_*x_;//״̬�ռ䵽�����ռ��ת��
	Eigen::VectorXd y = z - z_pred;//��ȡ����ֵ��״ֵ̬֮��Ĳ�
	//״̬����
	x_ = x_ + K*y;
	x_[3] = control_psi(x_[3]);
	long x_size = x_.size();
	Eigen::MatrixXd I = Eigen::MatrixXd::Identity(x_size, x_size);
	P_ = (I - K*H_)*P_;
}
Eigen::VectorXd EKF_CTRV::ProcessHJMatrix()
{
	Eigen::VectorXd hx = Eigen::VectorXd(3);
	HJ_ = Eigen::MatrixXd(3, 5);//Ԥ��ռ䵽�����ռ���ſ˱Ⱦ���
	double x = x_[0];
	double y = x_[1];
	double v = x_[2];
	double theta = x_[3];// control_psi(x_[3]);
	double x2y2 = (x*x + y*y);
	double sqrt_x2y2 = (sqrt(x2y2));
	double sqrt23_x2y2 = pow(x2y2, 3.0/2.0);
	double vxvy = v*x*cos(theta) + v*y*sin(theta);
	//------------------------------------------
	double q11 = x / sqrt_x2y2;
	double q12 = y / sqrt_x2y2;
	double q13 = 0.0;
	double q14 = 0.0;
	double q15 = 0.0;
	//------------------------------------------
	double q21 = -y / x2y2;
	double q22 = x / x2y2;
	double q23 = 0.0;
	double q24 = 0.0;
	double q25 = 0.0;
	//------------------------------------------
	double q31 = v*cos(theta) / sqrt_x2y2 - x*vxvy / sqrt23_x2y2;
	double q32 = v*sin(theta) / sqrt_x2y2 - y*vxvy / sqrt23_x2y2;
	double q33 = (x*cos(theta) + y*sin(theta)) / sqrt_x2y2;
	double q34 = (-x*v*sin(theta) + y*v*cos(theta)) / sqrt_x2y2;
	double q35 = 0.0;
	//------------------------------------------
	HJ_ << q11, q12, q13, q14, q15,
		q21, q22, q23, q24, q25,
		q31, q32, q33, q34, q35;
	hx << sqrt_x2y2, atan2(y,x), (v*x*cos(theta) + v*y*sin(theta)) / sqrt_x2y2;
	//hx << x, y, v*cos(theta - phi);
	return hx;
}
void EKF_CTRV::UpdateEKF(const Eigen::VectorXd &z)
{
	Eigen::VectorXd z_pred = ProcessHJMatrix();//״̬�ռ䵽�����ռ��ת��
	if (z_pred[0] < 0.0001)//# if rho is 0
		z_pred[2] = 0.0;
	z_pred[1] = control_psi(z_pred[1]);
	Eigen::VectorXd y = z - z_pred;//��ȡ����ֵ��״ֵ̬֮��Ĳ�
	y[1] = control_psi(y[1]);
	//���ڿ�������״̬���и���
	Eigen::MatrixXd HJ_T = HJ_.transpose();
	Eigen::MatrixXd S = HJ_*P_*HJ_T + R_;
	Eigen::MatrixXd Si = S.inverse();
	Eigen::MatrixXd PHT = P_*HJ_T;
	Eigen::MatrixXd K = PHT*Si;
	//״̬����
	x_ = x_ + K*y;
	x_[3] = control_psi(x_[3]);
	long x_size = x_.size();
	Eigen::MatrixXd I = Eigen::MatrixXd::Identity(x_size, x_size);
	P_ = (I - K*HJ_)*P_;
}

void EKF_CTRV::ProcessMeasurement(const MeasurementPackage &meas_package) {
	clock_t start, finish;
	start = clock();
	if (!is_initialized_)
	{
		/*
		* ��һ�β���ʱ��ʼ��״̬����
		* ����radar�Ĳ�����Ҫ����Ӽ�����ת��Ϊ�ѿ�������ϵ
		*/
		// first measurement
		x_ = Eigen::VectorXd(5);
		x_ << 0.0, 0.0, 0.0, 0.0, 0.0;

		if (meas_package.sensor_type_ == MeasurementPackage::LASER)
		{
			x_[0] = meas_package.raw_measurements_[0];//x
			x_[1] = meas_package.raw_measurements_[1];//y
			x_[2] = 5;								   //����ٶ�
			x_[3] = 0.0;								   //ƫ����
			x_[4] = 0.00000001;     						   //ƫ�����ٶ�
		}
		else if (meas_package.sensor_type_ == MeasurementPackage::RADAR){
			double rho = meas_package.raw_measurements_[0];
			double phi = meas_package.raw_measurements_[1];
			//���Ƕȹ�һ������-�У��С�
			phi = control_psi(phi);
			x_[0] = rho * cos(phi);
			x_[1] = rho * sin(phi);
			x_[2] = 5;								   //����ٶ�
			x_[3] = 0.0;								   //ƫ����
			x_[4] = 0.00000001;
		}
		previous_timestamp_ = meas_package.timestamp_;
		is_initialized_ = true;
		return;
	}
	/*
	* ����ʱ�����״̬ת������F_
	* ʱ����sΪ��λ
	* ���´���������Э�������
	*/
	double delta_t = (double(meas_package.timestamp_) - double(previous_timestamp_)) ;
	//std::cout <<"ԭʼֵ"<< x_[3] / M_PI*180.0 << "   ";
	//1.����Ԥ��--------------------------------------------------------
	Predict(delta_t);
	//std::cout << "Ԥ��ֵ"<<x_[3] / M_PI*180.0 << "   ";
	//2.����״̬����--------------------------------------------------------
	/*
	* ����
	* ���ڴ�����������ѡ����µĲ���
	*/
	if (meas_package.sensor_type_ == MeasurementPackage::RADAR) {
		//radar�ĸ���
		R_ = R_radar_;//����������
		//if (x_[2] < meas_package.raw_measurements_(2) / 5)
		//	x_[2] = meas_package.raw_measurements_(2);
		UpdateEKF(meas_package.raw_measurements_);//����radar���ݲ�����չ�������˲�����
	}
	else if (meas_package.sensor_type_ == MeasurementPackage::LASER) {
		//lidar�ĸ���
		H_ = H_laser_;
		R_ = R_laser_;//����������
		Update(meas_package.raw_measurements_);//����lidar���ݲ������Կ������˲�����
	}
	/*
	* ��ɸ��£�����ʱ��
	*/
	previous_timestamp_ = meas_package.timestamp_;
	finish = clock();
	double totaltime = (double)(finish - start) / CLOCKS_PER_SEC;
	//std::cout <<"���º�Ľ��"<< x_[3]/M_PI*180.0 << std::endl;
	//std::cout << " -------------Current Frame cluster Time:" << totaltime*1000.0 << "ms" << std::endl;
}