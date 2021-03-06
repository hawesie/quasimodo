#include "core/Util.h"
namespace reglib{

    namespace RigidMotionEstimator5 {
		/// @param Source (one 3D point per column)
		/// @param Target (one 3D point per column)
		/// @param Target normals (one 3D normal per column)
		/// @param Confidence weights
		/// @param Right hand side
		template <typename Derived1, typename Derived2, typename Derived3, typename Derived4, typename Derived5, typename Derived6>
		Eigen::Affine3d point_to_plane(Eigen::MatrixBase<Derived1>& X,
									   Eigen::MatrixBase<Derived2>& Xn,
									   Eigen::MatrixBase<Derived3>& Y,
									   Eigen::MatrixBase<Derived4>& Yn,
									   const Eigen::MatrixBase<Derived5>& w,
									   const Eigen::MatrixBase<Derived6>& u,
									   bool dox,
									   bool doy) {

			if(!dox && !doy){return Eigen::Affine3d::Identity();}

			typedef Eigen::Matrix<double, 6, 6> Matrix66;
			typedef Eigen::Matrix<double, 6, 1> Vector6;
			typedef Eigen::Block<Matrix66, 3, 3> Block33;

			/// Normalize weight vector
			Eigen::VectorXd w_normalized = w/w.sum();
			/// De-mean
			Eigen::Vector3d X_mean;
			for(int i=0; i<3; ++i){X_mean(i) = (X.row(i).array()*w_normalized.transpose().array()).sum();}
			X.colwise() -= X_mean;
			Y.colwise() -= X_mean;
			/// Prepare LHS and RHS


			Matrix66 LHS1 = Matrix66::Zero();
			Vector6 RHS1 = Vector6::Zero();
			if(dox){
				Block33 TL = LHS1.topLeftCorner<3,3>();
				Block33 TR = LHS1.topRightCorner<3,3>();
				Block33 BR = LHS1.bottomRightCorner<3,3>();
				Eigen::MatrixXd C = Eigen::MatrixXd::Zero(3,X.cols());
				#pragma omp parallel
				{
					#pragma omp for
					for(int i=0; i<X.cols(); i++) {
						C.col(i) = X.col(i).cross(Yn.col(i));
					}
					#pragma omp sections nowait
					{
						#pragma omp section
						for(int i=0; i<X.cols(); i++) TL.selfadjointView<Eigen::Upper>().rankUpdate(C.col(i), w(i));
						#pragma omp section
						for(int i=0; i<X.cols(); i++) TR += (C.col(i)*Yn.col(i).transpose())*w(i);
						#pragma omp section
						for(int i=0; i<X.cols(); i++) BR.selfadjointView<Eigen::Upper>().rankUpdate(Yn.col(i), w(i));
						#pragma omp section
						for(int i=0; i<C.cols(); i++) {
							double dist_to_plane = -((X.col(i) - Y.col(i)).dot(Yn.col(i)) - u(i))*w(i);
							RHS1.head<3>() += C.col(i)*dist_to_plane;
							RHS1.tail<3>() += Yn.col(i)*dist_to_plane;
						}
					}
				}
				LHS1 = LHS1.selfadjointView<Eigen::Upper>();
			}

			Matrix66 LHS2 = Matrix66::Zero();
			Vector6 RHS2 = Vector6::Zero();
			if(doy){
				Block33 TL = LHS2.topLeftCorner<3,3>();
				Block33 TR = LHS2.topRightCorner<3,3>();
				Block33 BR = LHS2.bottomRightCorner<3,3>();
				Eigen::MatrixXd C = Eigen::MatrixXd::Zero(3,Y.cols());
				#pragma omp parallel
				{
					#pragma omp for
					for(int i=0; i<Y.cols(); i++) {
						C.col(i) = Y.col(i).cross(Xn.col(i));
					}
					#pragma omp sections nowait
					{
						#pragma omp section
						for(int i=0; i<Y.cols(); i++) TL.selfadjointView<Eigen::Upper>().rankUpdate(C.col(i), w(i));
						#pragma omp section
						for(int i=0; i<Y.cols(); i++) TR += (C.col(i)*Xn.col(i).transpose())*w(i);
						#pragma omp section
						for(int i=0; i<Y.cols(); i++) BR.selfadjointView<Eigen::Upper>().rankUpdate(Xn.col(i), w(i));
						#pragma omp section
						for(int i=0; i<C.cols(); i++) {
							double dist_to_plane = -((Y.col(i) - X.col(i)).dot(Xn.col(i)) - u(i))*w(i);
							RHS2.head<3>() += C.col(i)*dist_to_plane;
							RHS2.tail<3>() += Xn.col(i)*dist_to_plane;
						}
					}
				}
				LHS2 = LHS2.selfadjointView<Eigen::Upper>();
			}

			Matrix66 LHS = LHS1 + LHS2;
			Vector6 RHS = RHS1 - RHS2;
			/// Compute transformation
			Eigen::Affine3d transformation;
			Eigen::LDLT<Matrix66> ldlt(LHS);
			RHS = ldlt.solve(RHS);
			transformation  = Eigen::AngleAxisd(RHS(0), Eigen::Vector3d::UnitX()) *
							  Eigen::AngleAxisd(RHS(1), Eigen::Vector3d::UnitY()) *
							  Eigen::AngleAxisd(RHS(2), Eigen::Vector3d::UnitZ());
			Xn = transformation*Xn;

			transformation.translation() = RHS.tail<3>();
			/// Apply transformation
			X = transformation*X;
			/// Re-apply mean
			X.colwise() += X_mean;
			Y.colwise() += X_mean;
			/// Return transformation
			return transformation;
		}

		/// @param Source (one 3D point per column)
		/// @param Target (one 3D point per column)
		/// @param Target normals (one 3D normal per column)
		/// @param Confidence weights
		template <typename Derived1, typename Derived2, typename Derived3, typename Derived4, typename Derived5>
		inline Eigen::Affine3d point_to_plane(Eigen::MatrixBase<Derived1>& X,
											  Eigen::MatrixBase<Derived2>& Xn,
											  Eigen::MatrixBase<Derived3>& Yp,
											  Eigen::MatrixBase<Derived4>& Yn,
											  const Eigen::MatrixBase<Derived5>& w,
											  bool dox,
											  bool doy) {
			return point_to_plane(X,Xn,Yp, Yn, w, Eigen::VectorXd::Zero(X.cols()),dox,doy);
		}

        /// @param Source (one 3D point per column)
        /// @param Target (one 3D point per column)
        /// @param Confidence weights
        template <typename Derived1, typename Derived2, typename Derived3>
        Eigen::Affine3d point_to_point(Eigen::MatrixBase<Derived1>& X, Eigen::MatrixBase<Derived2>& Y, const Eigen::MatrixBase<Derived3>& w) {
            /// Normalize weight vector
            Eigen::VectorXd w_normalized = w/w.sum();
            /// De-mean
            Eigen::Vector3d X_mean, Y_mean;
            for(int i=0; i<3; ++i) {
                X_mean(i) = (X.row(i).array()*w_normalized.transpose().array()).sum();
                Y_mean(i) = (Y.row(i).array()*w_normalized.transpose().array()).sum();
            }
            X.colwise() -= X_mean;
            Y.colwise() -= Y_mean;
            /// Compute transformation
            Eigen::Affine3d transformation;
            Eigen::Matrix3d sigma = X * w_normalized.asDiagonal() * Y.transpose();
            Eigen::JacobiSVD<Eigen::Matrix3d> svd(sigma, Eigen::ComputeFullU | Eigen::ComputeFullV);
            if(svd.matrixU().determinant()*svd.matrixV().determinant() < 0.0) {
                Eigen::Vector3d S = Eigen::Vector3d::Ones(); S(2) = -1.0;
                transformation.linear().noalias() = svd.matrixV()*S.asDiagonal()*svd.matrixU().transpose();
            } else {
                transformation.linear().noalias() = svd.matrixV()*svd.matrixU().transpose();
            }
            transformation.translation().noalias() = Y_mean - transformation.linear()*X_mean;
            /// Apply transformation
            X = transformation*X;
            /// Re-apply mean
            X.colwise() += X_mean;
            Y.colwise() += Y_mean;
            /// Return transformation
            return transformation;
        }
        /// @param Source (one 3D point per column)
        /// @param Target (one 3D point per column)
        template <typename Derived1, typename Derived2>
        inline Eigen::Affine3d point_to_point(Eigen::MatrixBase<Derived1>& X, Eigen::MatrixBase<Derived2>& Y) {
            return point_to_point(X, Y, Eigen::VectorXd::Ones(X.cols()));
        }
	}

	double getTime(){
		struct timeval start1;
		gettimeofday(&start1, NULL);
		return double(start1.tv_sec+(start1.tv_usec/1000000.0));
	}

	Eigen::Matrix4d getMatTest(const double * const camera, int mode){
		Eigen::Matrix4d ret = Eigen::Matrix4d::Identity();
		double rr [9];
		ceres::AngleAxisToRotationMatrix(camera,rr);

		ret(0,0) = rr[0];
		ret(1,0) = rr[1];
		ret(2,0) = rr[2];

		ret(0,1) = rr[3];
		ret(1,1) = rr[4];
		ret(2,1) = rr[5];

		ret(0,2) = rr[6];
		ret(1,2) = rr[7];
		ret(2,2) = rr[8];

		ret(0,3) = camera[3];
		ret(1,3) = camera[4];
		ret(2,3) = camera[5];
		return ret;
	}


	double * getCamera(Eigen::Matrix4d mat, int mode){
		double * camera = new double[6];
		double rr [9];
		rr[0] = mat(0,0);
		rr[1] = mat(1,0);
		rr[2] = mat(2,0);

		rr[3] = mat(0,1);
		rr[4] = mat(1,1);
		rr[5] = mat(2,1);

		rr[6] = mat(0,2);
		rr[7] = mat(1,2);
		rr[8] = mat(2,2);
		ceres::RotationMatrixToAngleAxis(rr,camera);

		camera[3] = mat(0,3);
		camera[4] = mat(1,3);
		camera[5] = mat(2,3);
		return camera;
	}
}
