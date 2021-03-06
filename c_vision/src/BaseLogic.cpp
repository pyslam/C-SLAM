/*
 * c_vision,
 *
 *
 * Copyright (C) 2014 Davide Tateo
 * Versione 1.0
 *
 * This file is part of c_vision.
 *
 * c_vision is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * c_vision is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with c_vision.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "BaseLogic.h"

#include <tf/LinearMath/Quaternion.h>
#include <tf/LinearMath/Matrix3x3.h>

#include <c_slam_msgs/NamedPolygon.h>

using namespace std;
using namespace cv;

BaseLogic::BaseLogic(ros::NodeHandle& n, ParameterServer& parameters) :
			n(n), it(n), classifierParam(parameters.getClassifierParams())
{
	imuSubscriber = n.subscribe(parameters.getImuSource() + "/imu", 1, &BaseLogic::handleImu, this);

	connectToClassificationServer();

	roll = 0;
	R = cv::Mat::eye(2, 2, CV_64F);
}

void BaseLogic::handleImu(const sensor_msgs::Imu& imu)
{
	if (camera_frame_id.empty())
		return;

	double roll, pitch, yaw;
	double x = imu.orientation.x;
	double y = imu.orientation.y;
	double z = imu.orientation.z;
	double w = imu.orientation.w;

	tf::Quaternion quaternion(x, y, z, w);
	tf::StampedTransform transform;
	ros::Time now = ros::Time(0);
	tfListener.waitForTransform(imu.header.frame_id, camera_frame_id, now,
				ros::Duration(1.0));
	tfListener.lookupTransform(imu.header.frame_id, camera_frame_id,
				ros::Time(0), transform);
	quaternion *= transform.getRotation();

	tf::Matrix3x3 rotationMatrix(quaternion);
	rotationMatrix.getRPY(pitch, roll, yaw);

	this->roll = roll;

	double a = cos(roll);
	double b = sin(roll);
	cv::Matx22d R(a, -b, b, a);
	this->R = cv::Mat(R);
}

void BaseLogic::connectToClassificationServer()
{
	classificationService = n.serviceClient<c_fuzzy::Classification>(
				"classification", true);
}

void BaseLogic::callClassificationService(c_fuzzy::Classification& serviceCall)
{
	if (classificationService.isValid())
	{
		classificationService.call(serviceCall);
	}
	else
	{
		ROS_ERROR("Service down, waiting reconnection...");
		classificationService.waitForExistence();
		connectToClassificationServer();
	}
}

void BaseLogic::sendFeatures(
			const vector<pair<vector<Point>, string> >& features, ros::Time t, size_t id)
{
	for (size_t i = 0; i < features.size(); i++)
	{
		const vector<Point>& polygon = features[i].first;
		const string& name = features[i].second;

		c_slam_msgs::NamedPolygon message;

		message.header.frame_id = camera_frame_id;
		message.header.stamp = t;
		message.id = id;
		message.polygonLabel = name;

		for (int i = 0; i < polygon.size(); i++)
		{
			geometry_msgs::Point32 point;
			point.x = polygon[i].x;
			point.y = polygon[i].y;
			point.z = 0;

			message.polygon.points.push_back(point);
		}

		publisher.publish(message);
	}
}

